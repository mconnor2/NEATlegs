#include "Simulation.h"

#include <algorithm>
#include <stdexcept>

#include <libconfig.h++>

using namespace std;

EpisodeOptions episodeOptionsFromConfig (const libconfig::Config &config,
					 EpisodeOptions opt) {
    if (config.exists("global.headFloor")) {
	const libconfig::Setting &f = config.lookup("global.headFloor");
	opt.headFloor = f.getType() == libconfig::Setting::TypeFloat ?
			(double)f : (double)(int)f;
    }
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
    res.energy = body->positiveWork();

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

const char *toString (EpisodeEnd end) {
    switch (end) {
	case EpisodeEnd::Running: return "running";
	case EpisodeEnd::MaxSteps: return "max steps";
	case EpisodeEnd::HeadBelowFloor: return "head below floor";
	case EpisodeEnd::ForbiddenContact: return "limb touched ground";
	case EpisodeEnd::Stopped: return "stopped";
    }
    return "?";
}
