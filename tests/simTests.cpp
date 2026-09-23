// Episodes (Simulation / runEpisode): end rules, controller wiring,
// determinism, and many threads sharing one spec.
//
// Usage: simTests [kanga2.cfg]   (optional real creature for the
//                                 multi-thread test)
#include <cmath>
#include <string>
#include <thread>
#include <vector>

#include <libconfig.h++>

#include "check.h"
#include "testCreature.h"

#include "Simulation.h"

using namespace std;

namespace {

// Deterministic "policy" that depends on the sensors, so trajectories do too
void wobble (const double *in, double *out, int nOut) {
    for (int i = 0; i < nOut; ++i)
	out[i] = 0.5 + 0.5 * sin(3.0 * in[1 + i % 6] + i);
}

Controller wobbler (int nOut) {
    return [nOut](const double *in, double *out) { wobble(in, out, nOut); };
}

EpisodeOptions noFloor (int steps) {
    EpisodeOptions o;
    o.maxSteps = steps;
    o.headFloor = -100;			//never
    return o;
}

bool sameResult (const EpisodeResult &a, const EpisodeResult &b) {
    return a.steps == b.steps && a.end == b.end && a.maxHeadX == b.maxHeadX &&
	   a.maxHeadY == b.maxHeadY && a.energy == b.energy;
}

}

TEST(inputsAndOutputsMatchSpec) {
    CreatureSpec spec = testCreature::spec();
    Simulation sim(spec);
    CHECK(sim.numInputs() == spec.numInputs() && sim.numInputs() == 7);
    CHECK(sim.numOutputs() == 2);

    vector<double> in(sim.numInputs(), -1.0);
    sim.readSensors(in.data());
    CHECK(in[0] == 1.0);			// bias
    for (double v : in) CHECK(v >= 0.0 && v <= 1.0);
}

// out[2m] sets muscle m's stiffness, out[2m+1] its rest length, each
// scaled into the muscle's range and clamped
TEST(outputsDriveMuscles) {
    CreatureSpec spec = testCreature::spec();
    const MuscleSpec &ms = spec.muscles[0];
    Simulation sim(spec);
    Muscle &m = *sim.creature().muscles[0];

    double full[2] = {1.0, 0.0};
    sim.applyOutputs(full);
    CHECK(m.stiffness() == ms.maxK && m.restLength() == ms.minEq);

    double mid[2] = {0.5, 0.5};
    sim.applyOutputs(mid);
    CHECK_NEAR(m.stiffness(), (ms.minK + ms.maxK) / 2, 1e-4);
    CHECK_NEAR(m.restLength(), (ms.minEq + ms.maxEq) / 2, 1e-6);

    double beyond[2] = {7.0, -3.0};
    sim.applyOutputs(beyond);
    CHECK(m.stiffness() == ms.maxK && m.restLength() == ms.minEq);
}

TEST(runsToMaxSteps) {
    CreatureSpec spec = testCreature::spec();
    EpisodeResult r = runEpisode(spec, noFloor(40), wobbler(2));
    CHECK(r.steps == 40);
    CHECK(r.end == EpisodeEnd::MaxSteps);
    CHECK(r.energy > 0.0);
}

TEST(endsWhenHeadBelowFloor) {
    CreatureSpec spec = testCreature::spec();
    EpisodeOptions o = noFloor(100);
    o.headFloor = 100;			// head starts ~1.2 m up
    EpisodeResult r = runEpisode(spec, o, wobbler(2));
    CHECK(r.steps == 1);
    CHECK(r.end == EpisodeEnd::HeadBelowFloor);
    CHECK(r.maxHeadX == 0 && r.maxHeadY == 0);	// the ending step doesn't count
}

TEST(endsWhenForbiddenLimbTouches) {
    CreatureSpec spec = testCreature::spec();
    EpisodeOptions o = noFloor(600);
    o.groundLimbs = {"b"};		// "a" starts 0.25 m up and falls
    EpisodeResult r = runEpisode(spec, o, wobbler(2));
    CHECK(r.end == EpisodeEnd::ForbiddenContact);
    CHECK(r.steps > 1 && r.steps < 600);

    o.groundLimbs = {"a", "b"};		// everything allowed
    CHECK(runEpisode(spec, o, wobbler(2)).end == EpisodeEnd::MaxSteps);
}

// The frame callback runs once per step, after physics and before the end
// rules, and can stop the episode
TEST(frameCallbackRunsEachStepAndCanStop) {
    CreatureSpec spec = testCreature::spec();
    int frames = 0;
    EpisodeResult r = runEpisode(spec, noFloor(50), wobbler(2),
				 [&](const Simulation &sim) {
	++frames;
	CHECK(sim.result().steps == frames);
	return frames < 5;
    });
    CHECK(frames == 5 && r.steps == 5);
    CHECK(r.end == EpisodeEnd::Stopped);

    // Drawing in the callback must not change the outcome
    EpisodeResult quiet = runEpisode(spec, noFloor(50), wobbler(2));
    EpisodeResult watched = runEpisode(spec, noFloor(50), wobbler(2),
				       [](const Simulation &) { return true; });
    CHECK(sameResult(quiet, watched));
}

TEST(stepByStepMatchesRunEpisode) {
    CreatureSpec spec = testCreature::spec();
    EpisodeOptions o = noFloor(60);
    Simulation sim(spec, o);
    vector<double> in(sim.numInputs()), out(sim.numOutputs(), 0.0);
    while (sim.running()) {
	sim.readSensors(in.data());
	wobble(in.data(), out.data(), sim.numOutputs());
	sim.applyOutputs(out.data());
	sim.step();
    }
    CHECK(sameResult(sim.result(), runEpisode(spec, o, wobbler(2))));
    CHECK(sim.result().energy == sim.creature().positiveWork());
    CHECK(!sim.step());			// finished episodes stay finished
    sim.stop();
    CHECK(sim.result().end == EpisodeEnd::MaxSteps);
}

TEST(invalidSetupsAreRejected) {
    CreatureSpec spec = testCreature::spec();
    EpisodeOptions o;
    o.groundLimbs = {"nope"};
    bool threw = false;
    try { Simulation sim(spec, o); } catch (std::exception &) { threw = true; }
    CHECK(threw);

    CreatureSpec headless = testCreature::spec(
	testCreature::Text.substr(0, testCreature::Text.find("shapes = ( { name")));
    threw = false;
    try { Simulation sim(headless); } catch (std::exception &) { threw = true; }
    CHECK(threw);
}

TEST(optionsFromConfig) {
    libconfig::Config c;
    c.readString("global: { headFloor = 0.35; groundLimbs = (\"foot\", \"shin\"); };");
    EpisodeOptions o = episodeOptionsFromConfig(c);
    CHECK(o.headFloor == 0.35);
    CHECK(o.groundLimbs.size() == 2u && o.groundLimbs[1] == "shin");
    CHECK(o.maxSteps == 1000);

    libconfig::Config d;
    d.readString("global: { headFloor = 1; };");	// integer
    CHECK(episodeOptionsFromConfig(d).headFloor == 1.0);

    libconfig::Config none;
    EpisodeOptions defaults;
    defaults.maxSteps = 20;
    EpisodeOptions e = episodeOptionsFromConfig(none, defaults);
    CHECK(e.headFloor == 0.75 && e.groundLimbs.empty() && e.maxSteps == 20);
}

TEST(episodesAreDeterministic) {
    CreatureSpec spec = testCreature::spec();
    EpisodeResult a = runEpisode(spec, noFloor(300), wobbler(2)),
		  b = runEpisode(spec, noFloor(300), wobbler(2));
    CHECK(sameResult(a, b));
}

static const char *realConfig;

// One parsed spec shared by many threads (no lock): every thread must get
// exactly the single-threaded result
TEST(threadsShareOneSpec) {
    CreatureSpec spec = testCreature::spec();
    EpisodeOptions o = noFloor(200);
    if (realConfig) {
	libconfig::Config c;
	c.readFile(realConfig);
	spec = parseCreatureSpec(c);
	o = episodeOptionsFromConfig(c, noFloor(200));
    }
    int nOut = spec.numOutputs();
    EpisodeResult expected = runEpisode(spec, o, wobbler(nOut));

    const int T = 8;
    vector<vector<EpisodeResult>> got(T);
    vector<thread> threads;
    for (int t = 0; t < T; ++t)
	threads.emplace_back([&, t]() {
	    for (int rep = 0; rep < 10; ++rep)
		got[t].push_back(runEpisode(spec, o, wobbler(nOut)));
	});
    for (auto &th : threads) th.join();
    for (auto &results : got)
	for (auto &r : results) CHECK(sameResult(r, expected));
}

int main (int argc, const char **argv) {
    realConfig = argc > 1 ? argv[1] : nullptr;
    return check::runTests();
}
