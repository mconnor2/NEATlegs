// Creature config parsing: valid configs build, broken ones fail cleanly
// (createCreature returns an empty pointer instead of crashing or
// handing Box2D a null id).
//
// Usage: creatureConfig [config.cfg ...]   extra configs must all build
#include <string>

#include <libconfig.h++>

#include "check.h"

#include "Creature.h"
#include "World.h"

using namespace std;

namespace {

// Two limbs, one joint, one muscle, one of each sensor type
const string Base = R"(
limbs = (
    { name = "a"; position = { x = 0.0; y = 0.5; }; angle = 0.0;
      shapes = ( { type = "box"; w = 0.05; h = 0.25; density = 1.0; groupIndex = -1; } ); },
    { name = "b"; position = { x = 0.0; y = 1.0; }; angle = 0.0;
      shapes = ( { type = "box"; w = 0.05; h = 0.25; density = 1.0; groupIndex = -1; },
                 { type = "ball"; radius = 0.05; position = { x = 0.0; y = 0.25; }; density = 1.0; } ); });
joints = (
    { name = "j"; type = "revolute"; obj1 = "a"; obj2 = "b";
      position = { x = 0.0; y = 0.75; }; lowerAngle = -1.0; upperAngle = 1.0; });
muscles = (
    { name = "m"; obj1 = "a"; pos1 = { x = 0.05; y = 0.1; };
      obj2 = "b"; pos2 = { x = 0.05; y = -0.1; };
      minK = 10.0; maxK = 50.0; minEq = 0.1; maxEq = 0.4; kd = 1.0;
      maxForce = 10.0; maxPower = 5.0; });
shapes = ( { name = "head"; body = "b"; position = { x = 0.0; y = 0.25; }; } );
sensors = (
    { type = "JointSensor"; target = "j"; },
    { type = "HeightSensor"; target = "head"; minH = 0; maxH = 2; },
    { type = "BodyAngleSensor"; target = "b"; minA = -1.6; maxA = 1.6; },
    { type = "AngularVelocitySensor"; target = "b"; minW = -10; maxW = 10; },
    { type = "VelocitySensor"; target = "b"; axis = "y"; minV = -3; maxV = 3; },
    { type = "ContactSensor"; target = "a"; });
)";

// Base with the first occurrence of `from` replaced by `to`
string withChange (const string &from, const string &to) {
    string s = Base;
    size_t i = s.find(from);
    CHECK(i != string::npos);
    if (i != string::npos) s.replace(i, from.size(), to);
    return s;
}

CreatureP build (const string &text, World &w) {
    libconfig::Config config;
    config.readString(text);
    config.setAutoConvert(true);
    return w.createCreature(config);
}

bool builds (const string &text) {
    World w;
    return build(text, w) != nullptr;
}

}

TEST(validConfigBuilds) {
    World w;
    CreatureP c = build(Base, w);
    CHECK(c != nullptr);
    if (!c) return;
    CHECK(c->limbs.size() == 2u);
    CHECK(c->joints.size() == 1u);
    CHECK(c->muscles.size() == 1u);
    CHECK(c->numSensors() == 6 + 1);		// plus bias

    double in[7];
    c->setInput(in);
    CHECK(in[0] == 1.0);			// bias first
    for (double v : in) CHECK(v >= 0.0 && v <= 1.0);
}

TEST(unknownNamesAreRejected) {
    CHECK(!builds(withChange(R"(obj2 = "b"; pos2)", R"(obj2 = "nope"; pos2)")));	// muscle
    CHECK(!builds(withChange(R"(obj1 = "a"; obj2 = "b";)", R"(obj1 = "nope"; obj2 = "b";)")));	// joint
    CHECK(!builds(withChange(R"(target = "j";)", R"(target = "nope";)")));	// joint sensor
    CHECK(!builds(withChange(R"(target = "head";)", R"(target = "nope";)")));	// shape sensor
    CHECK(!builds(withChange(R"(body = "b";)", R"(body = "nope";)")));	// shape
}

TEST(badValuesAreRejected) {
    CHECK(!builds(withChange(R"(type = "JointSensor")", R"(type = "NoSuchSensor")")));
    CHECK(!builds(withChange(R"(type = "box"; w = 0.05; h = 0.25; density = 1.0; groupIndex = -1; } ); },)",
			  R"(type = "cone"; w = 0.05; h = 0.25; density = 1.0; groupIndex = -1; } ); },)")));
    CHECK(!builds(withChange(R"(type = "revolute")", R"(type = "prismatic")")));
    CHECK(!builds(withChange(R"(axis = "y")", R"(axis = "z")")));
    CHECK(!builds(withChange("minK = 10.0; ", "")));	// required muscle field
    CHECK(!builds(withChange("joints = (", "joints_missing = (")));	// required section
}

TEST(optionalMuscleLimitsDefaultToUnlimited) {
    World w;
    CreatureP c = build(withChange("maxForce = 10.0; maxPower = 5.0; ", ""), w);
    CHECK(c != nullptr);
    if (c) CHECK(std::isinf(c->muscles[0]->maxPowerLimit()));
}

static const char **extraConfigs;
static int nExtra;

TEST(shippedConfigsBuild) {
    for (int i = 0; i < nExtra; ++i) {
	libconfig::Config config;
	config.readFile(extraConfigs[i]);
	config.setAutoConvert(true);
	World w;
	CreatureP c = w.createCreature(config);
	if (!c) fprintf(stderr, "  %s failed to build\n", extraConfigs[i]);
	CHECK(c != nullptr);
    }
}

int main (int argc, const char **argv) {
    extraConfigs = argv + 1;
    nExtra = argc - 1;
    return check::runTests();
}
