// Gait analysis of saved genomes for a two-footed creature: replays each
// headless and prints one line of foot-contact statistics.
//
// Usage: gait [-f left,right] config.cfg genome...
//   -f: the two foot limbs (default footL,footR)
//
// Columns: distance (max head x), steps, end, head (median height after
// 2 s), air (share of steps with neither foot down), duty L/R, touchdowns
// L/R, slide (worse foot's share of its travel spent sliding on the
// ground), stride (median right-foot touchdown interval, s), phase (circular
// mean of left touchdowns within the right stride, 0..1), R (its
// consistency, 0..1), class.
//
// A contact counts once the foot has been down 2 steps, and ends after it
// has been up 3, so bounces and single-step flickers don't count.  Phase
// and class use touchdowns after the first 2 s.  class: alt when the phase
// is 0.3..0.7, together below 0.2 or above 0.8, both with R >= 0.5;
// "few" with fewer than 5 touchdowns per foot; otherwise "neither".
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include <libconfig.h++>

#include "CreatureSpec.h"
#include "GeneticAlgorithm.h"
#include "Genome.h"
#include "Network.h"
#include "Simulation.h"

using namespace std;

namespace {

const char *endName (EpisodeEnd e) {
    switch (e) {
        case EpisodeEnd::MaxSteps: return "steps";
        case EpisodeEnd::HeadBelowFloor: return "fell";
        case EpisodeEnd::ForbiddenContact: return "contact";
        case EpisodeEnd::EnergySpent: return "energy";
        default: return "other";
    }
}

// Debounced contact: down after 2 steps in contact, up after 3 out
vector<bool> debounce (const vector<bool> &raw) {
    vector<bool> out(raw.size());
    bool down = false;
    int run = 0;
    for (size_t i = 0; i < raw.size(); ++i) {
        if (raw[i] != down) {
            ++run;
            if (run >= (down ? 3 : 2)) {
                down = raw[i];
                run = 0;
                //Back-date the change to when it started
                for (size_t k = i + 1 - (down ? 2 : 3); k < i; ++k)
                    out[k] = down;
            }
        } else {
            run = 0;
        }
        out[i] = down;
    }
    return out;
}

vector<int> touchdowns (const vector<bool> &c) {
    vector<int> t;
    for (size_t i = 1; i < c.size(); ++i)
        if (c[i] && !c[i-1]) t.push_back((int)i);
    return t;
}

double median (vector<double> v) {
    if (v.empty()) return 0;
    sort(v.begin(), v.end());
    return v[v.size() / 2];
}

}

int main (int argc, char **argv) {
    string feet[2] = {"footL", "footR"};
    int first = 1;
    if (argc > 2 && string(argv[1]) == "-f") {
        string f = argv[2];
        size_t comma = f.find(',');
        if (comma == string::npos) {
            fprintf(stderr, "gait: -f takes two limbs, e.g. footL,footR\n");
            return 1;
        }
        feet[0] = f.substr(0, comma);
        feet[1] = f.substr(comma + 1);
        first = 3;
    }
    if (argc < first + 2) {
        fprintf(stderr, "Usage: gait [-f left,right] config.cfg genome...\n");
        return 1;
    }
    libconfig::Config config;
    try {
        config.readFile(argv[first]);
    } catch (exception &e) {
        fprintf(stderr, "gait: can't read %s\n", argv[first]);
        return 1;
    }
    config.setAutoConvert(true);
    ExpParameters P;
    if (!P.loadFromFile(config)) return 1;
    CreatureSpec spec;
    try {
        spec = parseCreatureSpec(config);
    } catch (exception &e) {
        fprintf(stderr, "%s: %s\n", argv[first], e.what());
        return 1;
    }
    for (const string &f : feet)
        if (spec.limbIndex(f) < 0) {
            fprintf(stderr, "gait: no limb '%s'\n", f.c_str());
            return 1;
        }
    EpisodeOptions opt = episodeOptionsFromConfig(config);
    P.nInput = spec.numInputs();
    P.nOutput = spec.numOutputs();
    P.mirror = spec.neuronMirror();
    const double dt = 1.0 / opt.hz;
    const int skip = (int)(2.0 / dt);

    printf("%-40s %6s %5s %7s %5s %5s %9s %7s %5s %6s %5s %5s %4s %s\n",
           "genome", "dist", "steps", "end", "head", "air", "duty", "tds",
           "slide", "stride", "phase", "R", "tied", "class");
    for (int a = first + 1; a < argc; ++a) {
        ifstream in(argv[a]);
        GenomeP g = Genome::load(in, &P);
        if (!g) {
            fprintf(stderr, "gait: can't load %s\n", argv[a]);
            continue;
        }
        unique_ptr<Network> N(g->createNewNetwork());
        Controller controller = [&](const double *i, double *o) {
            N->run(i, o);
        };
        vector<bool> raw[2];
        vector<double> footX[2], headY;
        FrameCallback frame = [&](const Simulation &sim) {
            const Creature &c = sim.creature();
            for (int f = 0; f < 2; ++f) {
                BodyId b = c.limbs.at(feet[f]);
                raw[f].push_back(c.touchesOutside(b));
                footX[f].push_back(b2Body_GetPosition(b).x);
            }
            headY.push_back(sim.head().y);
            return true;
        };
        EpisodeResult r = runEpisode(spec, opt, controller, frame);

        size_t n = headY.size();
        vector<bool> c[2] = {debounce(raw[0]), debounce(raw[1])};
        int air = 0;
        double duty[2] = {0, 0}, slide[2] = {0, 0}, travel[2] = {0, 0};
        for (size_t i = 0; i < n; ++i) {
            if (!c[0][i] && !c[1][i]) ++air;
            for (int f = 0; f < 2; ++f) {
                duty[f] += c[f][i];
                if (i == 0) continue;
                double dx = fabs(footX[f][i] - footX[f][i-1]);
                travel[f] += dx;
                if (c[f][i] && c[f][i-1]) slide[f] += dx;
            }
        }
        double worse = 0;
        for (int f = 0; f < 2; ++f)
            if (travel[f] > 0.5) worse = max(worse, slide[f] / travel[f]);

        vector<int> td[2] = {touchdowns(c[0]), touchdowns(c[1])};
        vector<double> strides;
        for (size_t k = 1; k < td[1].size(); ++k)
            if (td[1][k-1] >= skip)
                strides.push_back((td[1][k] - td[1][k-1]) * dt);
        double sx = 0, sy = 0;
        int nPhase = 0;
        for (int t : td[0]) {
            if (t < skip) continue;
            auto it = upper_bound(td[1].begin(), td[1].end(), t);
            if (it == td[1].begin() || it == td[1].end()) continue;
            double phase = double(t - *(it-1)) / (*it - *(it-1));
            sx += cos(2*M_PI*phase);
            sy += sin(2*M_PI*phase);
            ++nPhase;
        }
        double meanPhase = 0, R = 0;
        if (nPhase) {
            meanPhase = atan2(sy, sx) / (2*M_PI);
            if (meanPhase < 0) meanPhase += 1;
            R = hypot(sx, sy) / nPhase;
        }
        const char *cls = "neither";
        if (td[0].size() < 5 || td[1].size() < 5 || nPhase < 3) cls = "few";
        else if (R >= 0.5 && meanPhase >= 0.3 && meanPhase <= 0.7)
            cls = "alt";
        else if (R >= 0.5 && (meanPhase < 0.2 || meanPhase > 0.8))
            cls = "together";

        vector<double> heads(headY.begin() + min(n, (size_t)skip),
                             headY.end());
        printf("%-40s %6.2f %5d %7s %5.2f %5.2f %4.2f/%4.2f %3zu/%3zu "
               "%5.2f %6.2f %5.2f %5.2f %4.2f %s\n",
               argv[a], r.maxHeadX, r.steps, endName(r.end), median(heads),
               n ? double(air) / n : 0, n ? duty[0] / n : 0,
               n ? duty[1] / n : 0, td[0].size(), td[1].size(), worse,
               median(strides), meanPhase, R,
               g->numLinks() ? double(g->numTiedLinks()) / g->numLinks() : 0,
               cls);
    }
    return 0;
}
