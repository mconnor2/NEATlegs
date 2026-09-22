/**
 * Muscle energy check: drop a creature far above the ground (no contacts)
 * and drive its muscles with adversarial patterns, then verify
 *
 *  1. the centre of mass follows free fall (muscles are internal forces)
 *  2. internal kinetic energy gained <= positive work the muscles did
 *     (the work accounting captures every joule going in)
 *  3. positive work <= sum of maxPower * time (the power limit holds),
 *     for creatures whose muscles all have a maxPower
 *
 * Usage: muscleEnergy config.cfg     exit status 0 on pass
 */
#include <cmath>
#include <cstdio>
#include <functional>
#include <vector>

#include <libconfig.h++>

#include "Creature.h"
#include "World.h"

using namespace std;

static const int Steps = 600;		//10 seconds at 60 Hz
static const float Hz = 60.0f;

struct Body {
    BodyId id;
    float mass, inertia;
};

//Kinetic energy relative to the centre of mass (what muscles can change)
static double internalKE (const vector<Body> &bodies, Vec2 &comVel) {
    double ke = 0, M = 0;
    Vec2 p = {0, 0};
    for (const Body &b : bodies) {
	Vec2 v = b2Body_GetLinearVelocity(b.id);
	float w = b2Body_GetAngularVelocity(b.id);
	ke += 0.5*b.mass*b2Dot(v, v) + 0.5*b.inertia*w*w;
	p += b.mass * v;
	M += b.mass;
    }
    comVel = (1.0f/M) * p;
    return ke - 0.5*M*b2Dot(comVel, comVel);
}

// Returns false on failure
static bool runPattern (const libconfig::Config &config, const char *label,
			const function<void(int step, int m, Muscle &)> &drive)
{
    World w(Hz);
    CreatureP C = w.createCreature(config);
    if (!C) return false;

    //Lift everything far above the ground
    vector<Body> bodies;
    for (auto &kv : C->limbs) {
	bodies.push_back({kv.second, b2Body_GetMass(kv.second),
			  b2Body_GetRotationalInertia(kv.second)});
	Vec2 p = b2Body_GetPosition(kv.second);
	b2Body_SetTransform(kv.second, {p.x, p.y + 10000.0f},
			    b2Body_GetRotation(kv.second));
    }

    double powerBudget = 0;
    bool limited = true;
    for (auto &m : C->muscles) {
	if (isinf(m->maxPowerLimit())) limited = false;
	else powerBudget += m->maxPowerLimit();
    }

    Vec2 v0;
    double ke0 = internalKE(bodies, v0);
    double maxGain = 0, maxComErr = 0;
    for (int step = 1; step <= Steps; ++step) {
	for (size_t m = 0; m < C->muscles.size(); ++m)
	    drive(step, m, *C->muscles[m]);
	w.step();

	Vec2 v;
	double gain = internalKE(bodies, v) - ke0;
	maxGain = max(maxGain, gain - C->positiveWork());
	double expectVy = v0.y - World::fGravity * step / Hz;
	maxComErr = max(maxComErr, (double)fabs(v.x - v0.x));
	maxComErr = max(maxComErr, fabs(v.y - expectVy));
    }

    double T = Steps / Hz;
    double pos = C->positiveWork();
    //Tolerances: float physics, and the power limit is applied using the
    // velocity at the start of each step
    bool comOK = maxComErr < 1e-2 * (1 + World::fGravity * T);
    bool energyOK = maxGain <= 0.01 * pos + 1e-3;
    bool powerOK = !limited || pos <= 1.1 * powerBudget * T;

    printf("  %-14s work +%10.2f J / %10.2f J  budget %10s  "
	   "KE excess %8.3g J  CoM err %.2g  %s\n",
	   label, pos, C->negativeWork(),
	   limited ? to_string((int)(powerBudget*T)).c_str() : "none",
	   maxGain, maxComErr,
	   comOK && energyOK && powerOK ? "ok" : "FAIL");
    if (!comOK) printf("    centre of mass left free fall\n");
    if (!energyOK)
	printf("    kinetic energy grew more than muscle work accounts for\n");
    if (!powerOK)
	printf("    positive work %.1f J exceeds maxPower budget %.1f J\n",
	       pos, powerBudget*T);
    return comOK && energyOK && powerOK;
}

int main (int argc, char **argv) {
    if (argc != 2) {
	fprintf(stderr, "Usage: muscleEnergy config.cfg\n");
	return 2;
    }
    libconfig::Config config;
    try {
	config.readFile(argv[1]);
    } catch (...) {
	fprintf(stderr, "Can't read %s\n", argv[1]);
	return 2;
    }
    config.setAutoConvert(true);

    printf("%s\n", argv[1]);
    bool ok = true;

    //Random control every step (fixed seed so failures reproduce)
    unsigned seed = 12345;
    auto rnd = [&]() {
	seed = seed * 1664525u + 1013904223u;
	return (seed >> 8) / 16777216.0;
    };
    ok &= runPattern(config, "random", [&](int, int, Muscle &m) {
	m.scaleStrength(rnd());
	m.scaleLength(rnd());
    });

    //Square waves, stiff throughout; antagonists out of phase.  Covers
    // pumping at and around the natural frequencies
    for (int period : {4, 10, 20, 40}) {
	char label[32];
	snprintf(label, sizeof(label), "square/%d", period);
	ok &= runPattern(config, label, [&](int step, int mi, Muscle &m) {
	    bool on = ((step / (period/2)) + mi) % 2;
	    m.scaleStrength(1.0);
	    m.scaleLength(on ? 0.0 : 1.0);
	});
    }

    printf(ok ? "PASS\n" : "FAIL\n");
    return ok ? 0 : 1;
}
