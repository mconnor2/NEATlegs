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
    o.headFloor = -100;                 //never
    return o;
}

bool sameResult (const EpisodeResult &a, const EpisodeResult &b) {
    return a.steps == b.steps && a.end == b.end && a.maxHeadX == b.maxHeadX &&
           a.maxHeadY == b.maxHeadY && a.energy == b.energy &&
           a.forceTime == b.forceTime;
}

}

TEST(inputsAndOutputsMatchSpec) {
    CreatureSpec spec = testCreature::spec();
    Simulation sim(spec);
    CHECK(sim.numInputs() == spec.numInputs() && sim.numInputs() == 7);
    CHECK(sim.numOutputs() == 2);

    vector<double> in(sim.numInputs(), -1.0);
    sim.readSensors(in.data());
    CHECK(in[0] == 1.0);                        // bias
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
    o.headFloor = 100;                  // head starts ~1.2 m up
    EpisodeResult r = runEpisode(spec, o, wobbler(2));
    CHECK(r.steps == 1);
    CHECK(r.end == EpisodeEnd::HeadBelowFloor);
    CHECK(r.maxHeadX == 0 && r.maxHeadY == 0);  // the ending step doesn't count
}

TEST(endsWhenForbiddenLimbTouches) {
    CreatureSpec spec = testCreature::spec();
    EpisodeOptions o = noFloor(600);
    o.groundLimbs = {"b"};              // "a" starts 0.25 m up and falls
    EpisodeResult r = runEpisode(spec, o, wobbler(2));
    CHECK(r.end == EpisodeEnd::ForbiddenContact);
    CHECK(r.steps > 1 && r.steps < 600);

    o.groundLimbs = {"a", "b"};         // everything allowed
    CHECK(runEpisode(spec, o, wobbler(2)).end == EpisodeEnd::MaxSteps);
}

// Once positive work reaches the budget the episode ends, counting that
// step; a step earlier it hadn't been reached
TEST(endsWhenEnergySpent) {
    CreatureSpec spec = testCreature::spec();
    double full = runEpisode(spec, noFloor(200), wobbler(2)).energy;
    EpisodeOptions o = noFloor(200);
    o.energyBudget = full / 3;
    EpisodeResult r = runEpisode(spec, o, wobbler(2));
    CHECK(r.end == EpisodeEnd::EnergySpent);
    CHECK(r.energy >= o.energyBudget);
    CHECK(r.steps > 1 && r.steps < 200);
    CHECK(runEpisode(spec, noFloor(r.steps - 1), wobbler(2)).energy <
          o.energyBudget);

    o.energyBudget = full * 2;          // never reached
    CHECK(runEpisode(spec, o, wobbler(2)).end == EpisodeEnd::MaxSteps);
}

// A muscle held at maxForce accumulates exactly maxForce x time, whether
// or not anything moves
TEST(forceTimeOfASaturatedMuscle) {
    std::string text = testCreature::Text;
    const std::string from = "minK = 10.0; maxK = 50.0; minEq = 0.1; maxEq = 0.4; kd = 1.0;\n"
                             "      maxForce = 10.0; maxPower = 5.0;",
                      to = "minK = 1e6; maxK = 1e6; minEq = 0.4; maxEq = 0.4; kd = 1.0;\n"
                           "      maxForce = 10.0;";
    size_t at = text.find(from);
    CHECK(at != std::string::npos);
    text.replace(at, from.size(), to);
    CreatureSpec spec = testCreature::spec(text);
    EpisodeResult r = runEpisode(spec, noFloor(120), wobbler(2));
    double expect = 10.0 * 120 / 60.0;
    CHECK(fabs(r.forceTime - expect) < 1e-4 * expect);
}

// The energy cost weighs positive work, absorbed work and force-time;
// the default is positive work alone, exactly
TEST(energyCostCombinesWorkAndForce) {
    CreatureSpec spec = testCreature::spec();
    EpisodeResult plain = runEpisode(spec, noFloor(200), wobbler(2));
    CHECK(plain.energy == plain.positiveWork);
    CHECK(plain.positiveWork > 0 && plain.negativeWork < 0 && plain.forceTime > 0);

    EpisodeOptions o = noFloor(200);
    o.energyCost = {4.0, 0.8, 0.3};
    EpisodeResult r = runEpisode(spec, o, wobbler(2));
    CHECK(r.positiveWork == plain.positiveWork);        // accounting only
    CHECK(r.energy == 4.0*r.positiveWork - 0.8*r.negativeWork + 0.3*r.forceTime);

    // The budget ends the episode on the combined cost: charging only
    // force-time, half of it runs out halfway
    o.energyCost = {0.0, 0.0, 1.0};
    o.energyBudget = plain.forceTime / 2;
    EpisodeResult b = runEpisode(spec, o, wobbler(2));
    CHECK(b.end == EpisodeEnd::EnergySpent);
    CHECK(b.energy == b.forceTime && b.energy >= o.energyBudget);
    CHECK(b.steps > 1 && b.steps < 200);
}

namespace {
EpisodeResult ended (EpisodeEnd end, int steps, double x) {
    EpisodeResult r;
    r.end = end;
    r.steps = steps;
    r.maxHeadX = x;
    return r;
}
}

TEST(survivalCountsFallsOnly) {
    CHECK(survival(ended(EpisodeEnd::MaxSteps, 1000, 0), 1000) == 1.0);
    CHECK(survival(ended(EpisodeEnd::EnergySpent, 300, 0), 1000) == 1.0);
    CHECK(survival(ended(EpisodeEnd::HeadBelowFloor, 250, 0), 1000) == 0.25);
    CHECK(survival(ended(EpisodeEnd::ForbiddenContact, 500, 0), 1000) == 0.5);
    CHECK(survival(ended(EpisodeEnd::Stopped, 100, 0), 1000) == 0.1);
}

TEST(fitnessShaping) {
    // Defaults: plain max head x, bit for bit, however the episode ended
    FitnessOptions plain;
    for (EpisodeEnd e : {EpisodeEnd::MaxSteps, EpisodeEnd::HeadBelowFloor})
        CHECK(episodeFitness(ended(e, 123, 1.2345678), 1000, plain) ==
              1.2345678);

    FitnessOptions f;
    f.base = 0.5;
    f.survivalExponent = 1;
    EpisodeResult dive = ended(EpisodeEnd::HeadBelowFloor, 100, 2.0),
                  shuffle = ended(EpisodeEnd::HeadBelowFloor, 500, 1.0),
                  stand = ended(EpisodeEnd::MaxSteps, 1000, 0.0),
                  walk = ended(EpisodeEnd::MaxSteps, 1000, 3.0),
                  budget = ended(EpisodeEnd::EnergySpent, 400, 3.0);
    CHECK(fabs(episodeFitness(dive, 1000, f) - 0.25) < 1e-12);
    CHECK(episodeFitness(dive, 1000, f) < episodeFitness(shuffle, 1000, f));
    CHECK(episodeFitness(stand, 1000, f) == 0.5);       // base alone
    CHECK(episodeFitness(walk, 1000, f) == 3.5);
    CHECK(episodeFitness(budget, 1000, f) == 3.5);      // spending isn't falling

    f.survivalExponent = 2;                     // harsher on early falls
    CHECK(fabs(episodeFitness(shuffle, 1000, f) - 0.375) < 1e-12);
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
    CHECK(!sim.step());                 // finished episodes stay finished
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
    d.readString("global: { headFloor = 1; };");        // integer
    CHECK(episodeOptionsFromConfig(d).headFloor == 1.0);

    libconfig::Config budget;
    budget.readString("global: { energyBudget = 50; fitnessBase = 0.5; "
                      "survivalExponent = 2; };");
    CHECK(episodeOptionsFromConfig(budget).energyBudget == 50.0);
    FitnessOptions f = fitnessOptionsFromConfig(budget);
    CHECK(f.base == 0.5 && f.survivalExponent == 2.0);

    libconfig::Config cost;
    cost.readString("global: { energyCost = { positiveWork = 4.0; "
                    "negativeWork = 0.83; forceTime = 1; }; };");
    EnergyCost ec = episodeOptionsFromConfig(cost).energyCost;
    libconfig::Config longer;
    longer.readString("global: { maxSteps = 3000; };");
    CHECK(episodeOptionsFromConfig(longer).maxSteps == 3000);
    CHECK(ec.positiveWork == 4.0 && ec.negativeWork == 0.83 && ec.forceTime == 1.0);

    for (const char *bad : {"global: { energyBudget = -1.0; };",
                            "global: { survivalExponent = -1; };",
                            "global: { energyCost = { forceTime = -0.5; }; };",
                            "global: { maxSteps = 0; };"}) {
        libconfig::Config c;
        c.readString(bad);
        bool threw = false;
        try {
            episodeOptionsFromConfig(c);
            fitnessOptionsFromConfig(c);
        } catch (std::exception &) { threw = true; }
        CHECK(threw);
    }

    libconfig::Config none;
    CHECK(episodeOptionsFromConfig(none).energyBudget == 0);
    EnergyCost dc = episodeOptionsFromConfig(none).energyCost;
    CHECK(dc.positiveWork == 1 && dc.negativeWork == 0 && dc.forceTime == 0);
    FitnessOptions nf = fitnessOptionsFromConfig(none);
    CHECK(nf.base == 0 && nf.survivalExponent == 0);
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
