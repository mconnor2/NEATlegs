#include "Simulation.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include <libconfig.h++>

using namespace std;

// Read an optional number that may be written as an int or a float
static void readNumber (const libconfig::Config &config, const char *path,
                        double &value) {
    if (!config.exists(path)) return;
    const libconfig::Setting &s = config.lookup(path);
    value = s.getType() == libconfig::Setting::TypeFloat ?
            (double)s : (double)(int)s;
}

EpisodeOptions episodeOptionsFromConfig (const libconfig::Config &config,
                                         EpisodeOptions opt) {
    if (config.exists("global.maxSteps")) {
        opt.maxSteps = (int)config.lookup("global.maxSteps");
        if (opt.maxSteps <= 0)
            throw runtime_error("maxSteps must be positive");
    }
    readNumber(config, "global.headFloor", opt.headFloor);
    readNumber(config, "global.energyBudget", opt.energyBudget);
    if (opt.energyBudget < 0)
        throw runtime_error("energyBudget must not be negative");
    EnergyCost &c = opt.energyCost;
    readNumber(config, "global.energyCost.positiveWork", c.positiveWork);
    readNumber(config, "global.energyCost.negativeWork", c.negativeWork);
    readNumber(config, "global.energyCost.forceTime", c.forceTime);
    if (c.positiveWork < 0 || c.negativeWork < 0 || c.forceTime < 0)
        throw runtime_error("energyCost weights must not be negative");
    if (config.exists("global.groundLimbs")) {
        const libconfig::Setting &gl = config.lookup("global.groundLimbs");
        opt.groundLimbs.clear();
        for (int i = 0; i < gl.getLength(); ++i)
            opt.groundLimbs.push_back(gl[i].c_str());
    }
    return opt;
}

Simulation::Simulation (const CreatureSpec &spec, const EpisodeOptions &_opt) :
                        opt(_opt), world(_opt.hz, _opt.subSteps),
                        nInputs(spec.numInputs()), nOutputs(spec.numOutputs())
{
    int head = spec.pointIndex("head");
    if (head < 0)
        throw runtime_error("creature must define a shape named 'head'");
    for (const string &name : opt.groundLimbs)
        if (spec.limbIndex(name) < 0)
            throw runtime_error("groundLimbs names unknown limb '" + name + "'");

    body = world.createCreature(spec);
    headPoint = body->shapes[spec.points[head].name];

    if (!opt.groundLimbs.empty()) {
        for (auto &kv : body->limbs)
            if (find(opt.groundLimbs.begin(), opt.groundLimbs.end(), kv.first) ==
                opt.groundLimbs.end())
                mustNotTouch.push_back(kv.second);
    }

    body->reset();
    if (opt.maxSteps <= 0) res.end = EpisodeEnd::MaxSteps;
}

void Simulation::readSensors (double *in) const {
    body->setInput(in);
}

void Simulation::applyOutputs (const double *out) {
    for (size_t m = 0; m < body->muscles.size(); ++m) {
        body->muscles[m]->scaleStrength(out[2*m]);
        body->muscles[m]->scaleLength(out[2*m + 1]);
    }
}

Vec2 Simulation::head () const {
    return b2Body_GetWorldPoint(headPoint.b, headPoint.localPos);
}

void Simulation::stop () {
    if (running()) res.end = EpisodeEnd::Stopped;
}

bool Simulation::step (const function<bool ()> &afterPhysics) {
    if (!running()) return false;

    world.step();
    ++res.steps;
    res.positiveWork = body->positiveWork();
    res.negativeWork = body->negativeWork();
    res.forceTime = body->forceTime();
    res.energy = opt.energyCost(res.positiveWork, res.negativeWork,
                                res.forceTime);

    if (afterPhysics && !afterPhysics()) {
        res.end = EpisodeEnd::Stopped;
        return false;
    }

    Vec2 h = head();
    if (h.y < opt.headFloor) {
        res.end = EpisodeEnd::HeadBelowFloor;
        return false;
    }
    for (BodyId b : mustNotTouch) {
        if (body->touchesOutside(b)) {
            res.end = EpisodeEnd::ForbiddenContact;
            return false;
        }
    }
    if (h.x > res.maxHeadX) res.maxHeadX = h.x;
    if (h.y > res.maxHeadY) res.maxHeadY = h.y;

    if (res.steps >= opt.maxSteps) res.end = EpisodeEnd::MaxSteps;
    else if (opt.energyBudget > 0 && res.energy >= opt.energyBudget)
        res.end = EpisodeEnd::EnergySpent;
    return running();
}

EpisodeResult runEpisode (const CreatureSpec &spec, const EpisodeOptions &opt,
                          const Controller &controller,
                          const FrameCallback &frame) {
    Simulation sim(spec, opt);
    vector<double> in(sim.numInputs()), out(sim.numOutputs(), 0.0);
    function<bool ()> afterPhysics;
    if (frame) afterPhysics = [&]() { return frame(sim); };

    while (sim.running()) {
        sim.readSensors(in.data());
        controller(in.data(), out.data());
        sim.applyOutputs(out.data());
        sim.step(afterPhysics);
    }
    return sim.result();
}

double EnergyCost::operator() (double posWork, double negWork,
                               double ft) const {
    return positiveWork*posWork - negativeWork*negWork + forceTime*ft;
}

FitnessOptions fitnessOptionsFromConfig (const libconfig::Config &config,
                                         FitnessOptions f) {
    readNumber(config, "global.fitnessBase", f.base);
    readNumber(config, "global.survivalExponent", f.survivalExponent);
    if (f.base < 0 || f.survivalExponent < 0)
        throw runtime_error("fitnessBase and survivalExponent must not be "
                            "negative");
    return f;
}

double survival (const EpisodeResult &r, int maxSteps) {
    if (r.end == EpisodeEnd::MaxSteps || r.end == EpisodeEnd::EnergySpent ||
        maxSteps <= 0)
        return 1.0;
    return min(1.0, (double)r.steps / maxSteps);
}

double episodeFitness (const EpisodeResult &r, int maxSteps,
                       const FitnessOptions &f) {
    double d = r.maxHeadX + f.base;
    if (f.survivalExponent == 0) return d;
    return d * pow(survival(r, maxSteps), f.survivalExponent);
}

const char *toString (EpisodeEnd end) {
    switch (end) {
        case EpisodeEnd::Running: return "running";
        case EpisodeEnd::MaxSteps: return "max steps";
        case EpisodeEnd::HeadBelowFloor: return "head below floor";
        case EpisodeEnd::ForbiddenContact: return "limb touched ground";
        case EpisodeEnd::EnergySpent: return "energy spent";
        case EpisodeEnd::Stopped: return "stopped";
    }
    return "?";
}
