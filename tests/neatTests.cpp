// Unit tests for the NEAT library (no physics or graphics)
#include <cmath>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "check.h"

#include "GeneticAlgorithm.h"
#include "Genome.h"
#include "InnovationStore.h"
#include "Network.h"
#include "random.h"

using namespace std;

namespace {

struct LinkRec {
    int innov, in, out;
    double weight;
    bool enabled;
    int mirror;             //-1 if untied or not saved
};

string saved (const GenomeP &g) {
    ostringstream s;
    g->save(s);
    return s.str();
}

vector<LinkRec> linksOf (const GenomeP &g) {
    vector<LinkRec> links;
    istringstream in(saved(g));
    string line;
    while (getline(in, line)) {
        if (line.compare(0, 5, "link ") != 0) continue;
        istringstream ls(line.substr(5));
        LinkRec l;
        int enabled;
        ls>>l.innov>>l.in>>l.out>>l.weight>>enabled;
        l.enabled = enabled != 0;
        if (!(ls>>l.mirror)) l.mirror = -1;
        links.push_back(l);
    }
    return links;
}

GenomeP loadText (const string &text, ExpParameters *P) {
    istringstream in(text);
    return Genome::load(in, P);
}

ExpParameters params (int nIn, int nOut) {
    ExpParameters P;
    P.popSize = 50;
    P.nInput = nIn;
    P.nOutput = nOut;
    return P;
}

vector<double> runNet (const GenomeP &g, const vector<double> &input,
                       int nOut, int times = 1) {
    unique_ptr<Network> N(g->createNewNetwork());
    vector<double> in(input), out(nOut, 0.0);
    for (int i = 0; i < times; ++i) N->run(in.data(), out.data());
    return out;
}

double sigmoid (double x) { return 1.0/(1.0 + exp(-3.0*x)); }

// A genome with some structure: a few generations of mutation and
// structural growth, as the GA would produce
GenomeP grownGenome (ExpParameters *P, InnovationStore &IS, int gens = 20) {
    GenomeP g(new Genome(P));
    for (int i = 0; i < gens; ++i) {
        IS.newGeneration();
        g = g->singleMate(&IS);
        g->mutate();
    }
    return g;
}

}

TEST(saveLoadRoundTrip) {
    seed_rand(1);
    ExpParameters P = params(4, 2);
    P.addNodeMutationRate = 0.5;
    P.addLinkMutationRate = 0.5;
    InnovationStore IS(&P);
    GenomeP g = grownGenome(&P, IS);
    CHECK(g->numHiddenNodes() > 0);

    GenomeP back = loadText(saved(g), &P);
    CHECK(back != nullptr);
    if (!back) return;
    CHECK(saved(back) == saved(g));
    CHECK(back->numLinks() == g->numLinks());
    CHECK(back->numHiddenNodes() == g->numHiddenNodes());

    vector<double> in = {1.0, 0.3, -0.2, 0.7};
    vector<double> a = runNet(g, in, 2, 5), b = runNet(back, in, 2, 5);
    CHECK(a == b);
}

TEST(loadRejectsMismatchedExperiment) {
    ExpParameters P = params(2, 1), Q = params(3, 1);
    GenomeP g(new Genome(&P));
    CHECK(loadText(saved(g), &Q) == nullptr);
    CHECK(loadText("not a genome\n", &P) == nullptr);
    CHECK(loadText("genome inputs 2 outputs 1 nodes 3 links 2\n"
                   "link 0 0 2 0.5 1\n", &P) == nullptr);       // truncated
}

TEST(cloneIsEqualAndIndependent) {
    seed_rand(2);
    ExpParameters P = params(3, 2);
    P.weightMutationRate = 1.0;
    InnovationStore IS(&P);
    GenomeP g = grownGenome(&P, IS, 5);
    GenomeP c = g->clone();
    CHECK(saved(c) == saved(g));
    string before = saved(g);
    c->mutate();
    CHECK(saved(g) == before);          // original untouched
    CHECK(saved(c) != before);
}

TEST(compatProperties) {
    seed_rand(3);
    ExpParameters P = params(3, 2);
    InnovationStore IS(&P);
    GenomeP a = grownGenome(&P, IS, 10), b = grownGenome(&P, IS, 10);
    CHECK(a->compat(a) == 0.0);
    CHECK(a->compat(a->clone()) == 0.0);
    CHECK(a->compat(b) == b->compat(a));
    CHECK(a->compat(b) > 0.0);

    // Pure weight changes: distance is compatWDiff * mean |dw|
    GenomeP c = a->clone();
    P.weightMutationRate = 1.0;
    c->mutate();
    vector<LinkRec> la = linksOf(a), lc = linksOf(c);
    double dw = 0;
    for (size_t i = 0; i < la.size(); ++i)
        dw += fabs(la[i].weight - lc[i].weight);
    CHECK_NEAR(a->compat(c), P.compatWDiff * dw / la.size(), 1e-9);
}

// Mismatching genes are counted; matching genes' weights are averaged
TEST(compatCountsGenesAndAveragesWeights) {
    ExpParameters P = params(2, 1);
    GenomeP a = loadText("genome inputs 2 outputs 1 nodes 4 links 4\n"
                         "link 0 0 2 0.5 1\n"
                         "link 1 1 2 0.5 1\n"
                         "link 3 0 3 1.0 1\n"
                         "link 4 3 2 0.5 1\n", &P);
    GenomeP b = loadText("genome inputs 2 outputs 1 nodes 3 links 2\n"
                         "link 0 0 2 1.5 1\n"
                         "link 1 1 2 0.5 1\n", &P);
    CHECK(a && b);
    if (!a || !b) return;
    // 2 of 4 genes don't match; matching weights differ by 1 and 0
    CHECK_NEAR(a->compat(b), P.compatGDiff * 2 + P.compatWDiff * 0.5, 1e-12);
}

TEST(matingKeepsInnovationsSortedAndUnique) {
    seed_rand(4);
    ExpParameters P = params(4, 2);
    P.addNodeMutationRate = 0.3;
    P.addLinkMutationRate = 0.3;
    InnovationStore IS(&P);
    vector<GenomeP> pop;
    for (int i = 0; i < 20; ++i) pop.push_back(GenomeP(new Genome(&P)));

    for (int gen = 0; gen < 30; ++gen) {
        IS.newGeneration();             // as GeneticAlgorithm does
        vector<GenomeP> next;
        for (int i = 0; i < 20; ++i) {
            GenomeP p1 = pop[rand_int() % pop.size()],
                    p2 = pop[rand_int() % pop.size()];
            GenomeP child = p1->mate(p2, &IS);
            child->mutate();

            vector<LinkRec> lc = linksOf(child), l1 = linksOf(p1);
            bool sorted = true;
            for (size_t k = 1; k < lc.size(); ++k)
                if (lc[k].innov <= lc[k-1].innov) sorted = false;
            CHECK(sorted);

            // Without inheritAllLinks the child has every gene of the
            // dominant parent (p1), plus at most 3 new ones
            size_t k = 0;
            for (const LinkRec &l : l1) {
                while (k < lc.size() && lc[k].innov < l.innov) ++k;
                CHECK(k < lc.size() && lc[k].innov == l.innov);
            }
            CHECK(lc.size() >= l1.size() && lc.size() <= l1.size() + 3);
            next.push_back(child);
        }
        pop.swap(next);
    }
}

// Add-link only joins neurons that aren't connected yet (NEAT paper,
// section 3.1), even when asked to add a link every time
TEST(addLinkNeverDuplicates) {
    seed_rand(5);
    ExpParameters P = params(3, 2);
    P.addLinkMutationRate = 1.0;
    P.addNodeMutationRate = 0.2;
    InnovationStore IS(&P);
    vector<GenomeP> pop;
    for (int i = 0; i < 10; ++i) pop.push_back(GenomeP(new Genome(&P)));
    for (int gen = 0; gen < 40; ++gen) {
        IS.newGeneration();
        vector<GenomeP> next;
        for (int i = 0; i < 10; ++i) {
            GenomeP p1 = pop[rand_int() % pop.size()],
                    p2 = pop[rand_int() % pop.size()];
            next.push_back(i % 2 ? p1->mate(p2, &IS) : p1->singleMate(&IS));
        }
        pop.swap(next);
    }
    int dups = 0;
    for (const GenomeP &g : pop) {
        vector<LinkRec> l = linksOf(g);
        for (size_t i = 0; i < l.size(); ++i)
            for (size_t j = i+1; j < l.size(); ++j)
                if (l[i].in == l[j].in && l[i].out == l[j].out) ++dups;
        CHECK(g->numLinks() > 6);       // links were still added
    }
    CHECK(dups == 0);
}

TEST(networkMatchesHandCalculation) {
    ExpParameters P = params(2, 1);
    // bias(0) and x(1) -> output(2); a disabled link must be ignored
    GenomeP g = loadText("genome inputs 2 outputs 1 nodes 3 links 3\n"
                         "link 0 0 2 0.2 1\n"
                         "link 1 1 2 0.4 1\n"
                         "link 2 1 2 50 0\n", &P);
    CHECK(g != nullptr);
    if (!g) return;
    double x = 0.5;
    vector<double> out = runNet(g, {1.0, x}, 1);
    CHECK_NEAR(out[0], sigmoid(0.2 + 0.4*x), 1e-12);
}

TEST(networkHiddenNodeSettles) {
    ExpParameters P = params(2, 1);
    // x -> hidden(3) -> output(2), plus bias -> output.  One propagation
    // step per run(), so with constant input it settles after two runs.
    GenomeP g = loadText("genome inputs 2 outputs 1 nodes 4 links 3\n"
                         "link 0 0 2 0.5 1\n"
                         "link 1 1 3 1.0 1\n"
                         "link 2 3 2 2.0 1\n", &P);
    CHECK(g != nullptr);
    if (!g) return;
    double x = -0.3;
    vector<double> out = runNet(g, {1.0, x}, 1, 3);
    CHECK_NEAR(out[0], sigmoid(0.5 + 2.0*sigmoid(x)), 1e-12);
}

// Networks keep their own copy of the structure: building and running
// them must not change the genome
TEST(networkLeavesGenomeUntouched) {
    seed_rand(5);
    ExpParameters P = params(3, 2);
    P.addNodeMutationRate = 0.5;
    InnovationStore IS(&P);
    GenomeP g = grownGenome(&P, IS, 10);
    string before = saved(g);
    runNet(g, {1.0, 0.2, 0.4}, 2, 10);
    CHECK(saved(g) == before);
}

// ...so one genome can be evaluated on several threads at once
TEST(sameGenomeOnManyThreads) {
    seed_rand(6);
    ExpParameters P = params(3, 2);
    P.addNodeMutationRate = 0.5;
    P.addLinkMutationRate = 0.5;
    InnovationStore IS(&P);
    GenomeP g = grownGenome(&P, IS, 15);
    vector<double> expected = runNet(g, {1.0, 0.3, -0.6}, 2, 50);

    const int T = 8;
    vector<vector<double>> got(T);
    vector<thread> threads;
    for (int t = 0; t < T; ++t)
        threads.emplace_back([&, t]() {
            for (int rep = 0; rep < 20; ++rep)
                got[t] = runNet(g, {1.0, 0.3, -0.6}, 2, 50);
        });
    for (auto &th : threads) th.join();
    for (int t = 0; t < T; ++t) CHECK(got[t] == expected);
}

// A neuron nothing feeds (its only incoming link is disabled) used to make
// the first run() loop forever waiting for it to activate
TEST(unreachableNeuronDoesNotHang) {
    ExpParameters P = params(2, 1);
    GenomeP g = loadText("genome inputs 2 outputs 1 nodes 4 links 3\n"
                         "link 0 0 2 0.5 1\n"
                         "link 1 1 3 1.0 0\n"   // only way into neuron 3
                         "link 2 3 2 2.0 1\n", &P);
    CHECK(g != nullptr);
    if (!g) return;
    vector<double> out = runNet(g, {1.0, 0.5}, 1, 3);
    // neuron 3 sits at sigmoid(0) = 0.5 once it's updated
    CHECK_NEAR(out[0], sigmoid(0.5 + 2.0*0.5), 1e-12);
}

TEST(innovationsSharedWithinGeneration) {
    ExpParameters P = params(3, 2);
    InnovationStore IS(&P);
    int a, b, c;
    CHECK(!IS.addLink(0, 4, a));
    CHECK(IS.addLink(0, 4, b));         // seen this generation
    CHECK(a == b);
    CHECK(!IS.addLink(1, 4, c));
    CHECK(c != a);

    int pre1, post1, n1, pre2, post2, n2;
    CHECK(!IS.addNode(2, pre1, post1, n1));
    CHECK(IS.addNode(2, pre2, post2, n2));
    CHECK(pre1 == pre2 && post1 == post2 && n1 == n2);
    CHECK(n1 == P.nInput + P.nOutput);          // first new neuron id

    IS.newGeneration();
    int d;
    CHECK(!IS.addLink(0, 4, d));                // forgotten: new innovation
    CHECK(d != a);
}

TEST(seededRandomIsRepeatable) {
    seed_rand(42);
    vector<double> a;
    for (int i = 0; i < 100; ++i)
        a.push_back(i % 2 ? rand_double() : rand_gauss());
    seed_rand(42);
    vector<double> b;
    for (int i = 0; i < 100; ++i)
        b.push_back(i % 2 ? rand_double() : rand_gauss());
    CHECK(a == b);
    seed_rand(43);
    CHECK(rand_double() != a[1]);
}

namespace {

// XOR on fixed inputs: deterministic and cheap
double xorFitness (const GenomeP &g) {
    static const double cases[4][3] = {{0,0,0}, {0,1,1}, {1,0,1}, {1,1,0}};
    unique_ptr<Network> N(g->createNewNetwork());
    double err = 0;
    for (auto &c : cases) {
        double in[3] = {1.0, c[0], c[1]}, out[1] = {0};
        for (int k = 0; k < 4; ++k) N->run(in, out);
        err += (out[0] - c[2]) * (out[0] - c[2]);
    }
    return (g->fitness = 4.0 - err);
}

struct RunResult {
    vector<GenerationStats> history;
    string best;
};

RunResult runGA (uint64_t seed, int gens) {
    seed_rand(seed);
    ExpParameters P = params(3, 1);
    P.popSize = 60;
    P.compatThresh = 3.0;
    P.targetSpecies = 5;
    FitnessFunction f = xorFitness;
    GeneticAlgorithm GA(&P, &f);
    for (int i = 0; i < gens; ++i) GA.nextGeneration();
    return {GA.history(), saved(GA.topGenomes().at(0).second)};
}

bool sameStats (const GenerationStats &a, const GenerationStats &b) {
    return a.generation == b.generation &&
           a.populationSize == b.populationSize &&
           a.maxFitness == b.maxFitness && a.meanFitness == b.meanFitness &&
           a.minFitness == b.minFitness && a.nSpecies == b.nSpecies &&
           a.meanCompat == b.meanCompat &&
           a.meanEnabledLinks == b.meanEnabledLinks;
}

}

// Same seed, same run, even though fitness is evaluated on many threads
TEST(gaIsDeterministicForASeed) {
    RunResult a = runGA(7, 25), b = runGA(7, 25);
    CHECK(a.history.size() == 25u && b.history.size() == 25u);
    bool same = a.history.size() == b.history.size();
    for (size_t i = 0; same && i < a.history.size(); ++i)
        same = sameStats(a.history[i], b.history[i]);
    CHECK(same);
    CHECK(a.best == b.best);

    RunResult c = runGA(8, 25);
    CHECK(c.best != a.best);
}

// Fitness is deterministic and species champions are carried over, so the
// best fitness can never drop, even when the champion is alone in an old
// species (which is otherwise culled)
TEST(gaNeverLosesChampion) {
    for (uint64_t seed = 1; seed <= 10; ++seed) {
        seed_rand(seed);
        ExpParameters P = params(3, 1);
        P.popSize = 40;
        P.compatThresh = 0.5;
        P.targetSpecies = 15;
        P.oldAge = 1;
        FitnessFunction f = xorFitness;
        GeneticAlgorithm GA(&P, &f);
        for (int i = 0; i < 40; ++i) GA.nextGeneration();
        bool monotone = true;
        for (size_t i = 1; i < GA.history().size(); ++i)
            if (GA.history()[i].maxFitness < GA.history()[i-1].maxFitness)
                monotone = false;
        CHECK(monotone);
    }
}

// A species whose best fitness hasn't improved for dropoffAge generations
// stops reproducing (and so disappears), unless it holds the champion
TEST(gaDropsStagnantSpecies) {
    const int dropoff = 3;
    int dropped = 0;
    for (uint64_t seed = 1; seed <= 5; ++seed) {
        seed_rand(seed);
        ExpParameters P = params(3, 1);
        P.popSize = 60;
        P.targetSpecies = 6;
        P.dropoffAge = dropoff;
        FitnessFunction f = xorFitness;
        GeneticAlgorithm GA(&P, &f);
        for (int i = 0; i < 40; ++i) GA.nextGeneration();

        map<int, pair<double, int> > best;     //id -> best, generations
        bool ok = true;
        for (const GenerationStats &g : GA.history()) {
            set<int> present;
            for (const SpecieStats &sp : g.species) {
                present.insert(sp.id);
                auto it = best.find(sp.id);
                if (it == best.end()) {
                    best[sp.id] = make_pair(sp.maxFitness, 0);
                    continue;
                }
                if (sp.maxFitness > it->second.first)
                    it->second = make_pair(sp.maxFitness, 0);
                else
                    ++it->second.second;
                if (it->second.second > dropoff &&
                    sp.maxFitness < g.maxFitness)
                    ok = false;
            }
            for (auto it = best.begin(); it != best.end(); ) {
                if (present.count(it->first)) { ++it; continue; }
                if (it->second.second == dropoff) ++dropped;
                it = best.erase(it);
            }
        }
        CHECK(ok);
    }
    CHECK(dropped > 0);         //the rule was exercised
}

TEST(gaStatsAreConsistent) {
    RunResult r = runGA(9, 15);
    for (size_t i = 0; i < r.history.size(); ++i) {
        const GenerationStats &s = r.history[i];
        CHECK(s.generation == (int)i);
        if (i > 0) CHECK(s.populationSize == 60);
        CHECK(s.minFitness <= s.meanFitness && s.meanFitness <= s.maxFitness);
        CHECK(s.nSpecies >= 1 && s.nSpecies <= s.populationSize);
        int members = 0;
        for (const SpecieStats &sp : s.species) members += sp.size;
        CHECK(members == s.populationSize);
    }
}

// One champion per species, with its raw fitness (the species' max in the
// stats), fittest first, the first being the population's best
TEST(gaKeepsSpeciesChampions) {
    seed_rand(11);
    ExpParameters P = params(3, 1);
    P.popSize = 60;
    P.targetSpecies = 6;
    FitnessFunction f = xorFitness;
    GeneticAlgorithm GA(&P, &f);
    for (int i = 0; i < 15; ++i) {
        GA.nextGeneration();
        const auto &champs = GA.speciesChampions();
        const GenerationStats &s = GA.history().back();
        CHECK(champs.size() == s.species.size());
        set<int> ids;
        for (size_t k = 0; k < champs.size(); ++k) {
            ids.insert(champs[k].specieId);
            if (k) CHECK(champs[k].fitness <= champs[k-1].fitness);
            for (const SpecieStats &sp : s.species)
                if (sp.id == champs[k].specieId)
                    CHECK(sp.maxFitness == champs[k].fitness);
        }
        CHECK(ids.size() == champs.size());
        CHECK(!champs.empty() &&
              champs[0].fitness == GA.topGenomes().at(0).first);
    }
}

namespace {

// 5 inputs (bias, a left/right pair, a midline input, a clock pair) and
// 4 outputs (two left/right pairs)
ExpParameters mirrorParams () {
    ExpParameters P = params(6, 4);
    P.mirror = {0, 2, 1, 3, 5, 4, 7, 6, 9, 8};
    return P;
}

// Every tied link's partner is there, tied back, with the same weight and
// enabled state, joining the mirrors of its ends; hidden neurons get one
// mirror each, and the mirror of a mirror is the neuron itself
bool tiesConsistent (const vector<LinkRec> &links, const ExpParameters &P) {
    map<int, const LinkRec *> byInnov;
    for (const LinkRec &l : links) byInnov[l.innov] = &l;
    map<int, int> sigma;
    for (int i = 0; i < (int)P.mirror.size(); ++i) sigma[i] = P.mirror[i];
    auto maps = [&](int a, int b) {
        auto it = sigma.insert(make_pair(a, b)).first;
        return it->second == b;
    };
    for (const LinkRec &l : links) {
        if (l.mirror < 0) continue;
        auto p = byInnov.find(l.mirror);
        if (p == byInnov.end()) return false;
        const LinkRec &m = *p->second;
        if (m.mirror != l.innov || m.weight != l.weight ||
            m.enabled != l.enabled) return false;
        if (!maps(l.in, m.in) || !maps(l.out, m.out)) return false;
    }
    for (auto &kv : sigma)
        if (sigma.count(kv.second) == 0 || sigma[kv.second] != kv.first)
            return false;
    return true;
}

int tiedCount (const vector<LinkRec> &links) {
    int n = 0;
    for (const LinkRec &l : links) n += l.mirror >= 0;
    return n;
}

}

TEST(mirrorInitialGenomeIsTied) {
    ExpParameters P = mirrorParams();
    seed_rand(21);
    GenomeP g(new Genome(&P));
    vector<LinkRec> l = linksOf(g);
    CHECK(l.size() == 24u && tiedCount(l) == 24);
    CHECK(tiesConsistent(l, P));
    // bias -> output 6 mirrors bias -> output 7
    CHECK(l[0].mirror == 1 && l[0].weight == l[1].weight);

    // Untied, it's the genome the same seed gives without a mirror map
    ExpParameters Q = params(6, 4);
    P.mirrorInitTied = 0;
    seed_rand(21);
    GenomeP u(new Genome(&P));
    seed_rand(21);
    GenomeP plain(new Genome(&Q));
    vector<LinkRec> lu = linksOf(u), lp = linksOf(plain);
    CHECK(tiedCount(lu) == 0);
    bool same = lu.size() == lp.size();
    for (size_t i = 0; same && i < lu.size(); ++i)
        same = lu[i].weight == lp[i].weight;
    CHECK(same);
}

// Through mutation, structural growth and crossover the ties stay
// consistent, and nearly everything stays tied without untying
TEST(mirrorTiesSurviveEvolution) {
    seed_rand(22);
    ExpParameters P = mirrorParams();
    P.addNodeMutationRate = 0.3;
    P.addLinkMutationRate = 0.3;
    P.weightMutationRate = 0.8;
    P.linkEnabledRate = 0.3;
    P.inheritDominant = 0.5;            //average matching genes often
    InnovationStore IS(&P);
    vector<GenomeP> pop;
    for (int i = 0; i < 20; ++i) pop.push_back(GenomeP(new Genome(&P)));
    bool consistent = true, sorted = true, noDups = true;
    int hidden = 0;
    for (int gen = 0; gen < 30; ++gen) {
        IS.newGeneration();
        vector<GenomeP> next;
        for (int i = 0; i < 20; ++i) {
            GenomeP p1 = pop[rand_int() % pop.size()],
                    p2 = pop[rand_int() % pop.size()];
            GenomeP child = i % 3 ? p1->mate(p2, &IS) : p1->singleMate(&IS);
            child->mutate();
            vector<LinkRec> l = linksOf(child);
            consistent = consistent && tiesConsistent(l, P);
            set<pair<int, int> > ends;
            for (size_t k = 0; k < l.size(); ++k) {
                if (k && l[k].innov <= l[k-1].innov) sorted = false;
                if (!ends.insert(make_pair(l[k].in, l[k].out)).second)
                    noDups = false;
            }
            next.push_back(child);
        }
        pop.swap(next);
    }
    int tied = 0, links = 0;
    for (const GenomeP &g : pop) {
        tied += g->numTiedLinks();
        links += g->numLinks();
        hidden += g->numHiddenNodes();
    }
    CHECK(consistent);
    CHECK(sorted);
    CHECK(noDups);
    CHECK(hidden > 0);
    CHECK(tied > 0.9 * links);
}

// A fully tied network treats mirrored inputs the same way: run on the
// mirror image of an input sequence, it gives the mirror image of the
// outputs (to rounding, since sums are added in a different order)
TEST(mirrorTiedNetworkIsEquivariant) {
    seed_rand(23);
    ExpParameters P = mirrorParams();
    P.addNodeMutationRate = 0.4;
    P.addLinkMutationRate = 0.4;
    P.weightMutationRate = 0.8;
    InnovationStore IS(&P);
    GenomeP g = grownGenome(&P, IS, 40);
    CHECK(g->numHiddenNodes() >= 4);
    CHECK(g->numTiedLinks() == g->numLinks());
    CHECK(tiesConsistent(linksOf(g), P));

    unique_ptr<Network> a(g->createNewNetwork()), b(g->createNewNetwork());
    double worst = 0;
    for (int t = 0; t < 20; ++t) {
        vector<double> in(6), mirrored(6), outA(4), outB(4);
        in[0] = 1.0;
        for (int k = 1; k < 6; ++k) in[k] = rand_double();
        for (int k = 0; k < 6; ++k) mirrored[P.mirror[k]] = in[k];
        a->run(in.data(), outA.data());
        b->run(mirrored.data(), outB.data());
        for (int k = 0; k < 4; ++k)
            worst = max(worst, fabs(outB[P.mirror[6 + k] - 6] - outA[k]));
    }
    CHECK(worst < 1e-12);

    // Untie everything and it no longer holds
    P.untieRate = 1.0;
    g->mutate();
    CHECK(g->numTiedLinks() == 0);
}

// For speciation a tied pair is one gene: a mirrored add-node (4 links)
// is as far as a plain one (2 links)
TEST(mirrorCompatCountsPairsOnce) {
    for (bool mirror : {false, true}) {
        seed_rand(27);
        ExpParameters P = mirror ? mirrorParams() : params(6, 4);
        P.addNodeMutationRate = 1.0;
        P.addLinkMutationRate = 0;
        InnovationStore IS(&P);
        GenomeP g(new Genome(&P));
        GenomeP c = g->singleMate(&IS);
        CHECK(c->numLinks() == g->numLinks() + (mirror ? 4 : 2));
        CHECK_NEAR(g->compat(c), 2 * P.compatGDiff, 1e-12);
    }
}

TEST(mirrorSaveLoadKeepsTies) {
    seed_rand(24);
    ExpParameters P = mirrorParams();
    P.addNodeMutationRate = 0.5;
    InnovationStore IS(&P);
    GenomeP g = grownGenome(&P, IS, 10);
    GenomeP back = loadText(saved(g), &P);
    CHECK(back && saved(back) == saved(g));
    CHECK(back && back->numTiedLinks() == g->numTiedLinks());
    // Files without the column load untied
    ExpParameters Q = params(6, 4);
    GenomeP old = loadText(saved(GenomeP(new Genome(&Q))), &P);
    CHECK(old && old->numTiedLinks() == 0);
}

// With nothing tied and no mirrored structure, a mirror map changes nothing
TEST(mirrorRatesZeroLeaveRunsUnchanged) {
    auto run = [](bool mirror) {
        seed_rand(25);
        ExpParameters P = params(3, 1);
        P.popSize = 60;
        P.targetSpecies = 5;
        if (mirror) {
            P.mirror = {0, 2, 1, 3};
            P.mirrorInitTied = 0;
            P.mirrorStructure = 0;
        }
        FitnessFunction f = xorFitness;
        GeneticAlgorithm GA(&P, &f);
        for (int i = 0; i < 20; ++i) GA.nextGeneration();
        RunResult r = {GA.history(), ""};
        for (const LinkRec &l : linksOf(GA.topGenomes().at(0).second))
            r.best += to_string(l.innov) + " " + to_string(l.weight) + "\n";
        return r;
    };
    RunResult a = run(false), b = run(true);
    bool same = a.history.size() == b.history.size();
    for (size_t i = 0; same && i < a.history.size(); ++i)
        same = sameStats(a.history[i], b.history[i]);
    CHECK(same);
    CHECK(a.best == b.best);
    CHECK(b.history.back().tiedFraction == 0);
}

TEST(mirrorGaRunsAndRejectsBadMaps) {
    seed_rand(26);
    ExpParameters P = params(3, 1);
    P.popSize = 60;
    P.targetSpecies = 5;
    P.mirror = {0, 2, 1, 3};
    P.untieRate = 0.01;
    FitnessFunction f = xorFitness;
    GeneticAlgorithm GA(&P, &f);
    for (int i = 0; i < 10; ++i) GA.nextGeneration();
    CHECK(GA.history()[0].tiedFraction == 1.0);
    CHECK(GA.history().back().tiedFraction > 0.5);

    auto rejects = [](vector<int> m) {
        ExpParameters Q = params(3, 2);
        Q.popSize = 10;
        Q.mirror = m;
        FitnessFunction f = xorFitness;
        try {
            GeneticAlgorithm GA(&Q, &f);
        } catch (std::invalid_argument &) {
            return true;
        }
        return false;
    };
    CHECK(rejects({0, 2, 1, 3}));               //wrong size
    CHECK(rejects({0, 2, 0, 3, 4}));            //not its own inverse
    CHECK(rejects({0, 3, 2, 1, 4}));            //input to output
    CHECK(!rejects({0, 2, 1, 4, 3}));
}

int main () {
    return check::runTests();
}
