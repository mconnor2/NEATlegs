// Creature configs: parsing to a CreatureSpec (validation, defaults, name
// resolution) and building a Creature from a spec.
//
// Usage: creatureConfig [config.cfg ...]   extra configs must all build
#include <cmath>
#include <string>

#include <libconfig.h++>

#include "check.h"
#include "testCreature.h"

#include "Creature.h"
#include "CreatureSpec.h"
#include "World.h"

using std::string;

namespace {

const string &Base = testCreature::Text;

// Base with the first occurrence of `from` replaced by `to`
string withChange (const string &from, const string &to) {
    string s = Base;
    size_t i = s.find(from);
    CHECK(i != string::npos);
    if (i != string::npos) s.replace(i, from.size(), to);
    return s;
}

CreatureSpec parse (const string &text) {
    libconfig::Config config;
    config.readString(text);
    return parseCreatureSpec(config);
}

// The parse error message, or "" if the text parses
string parseError (const string &text) {
    try {
	parse(text);
    } catch (std::exception &e) {
	return e.what();
    }
    return "";
}

bool mentions (const string &message, const string &part) {
    if (message.find(part) != string::npos) return true;
    fprintf(stderr, "  error \"%s\" doesn't mention \"%s\"\n",
	    message.c_str(), part.c_str());
    return false;
}

}

TEST(specHasEverythingResolved) {
    CreatureSpec s = parse(Base);
    CHECK(s.limbs.size() == 2u && s.joints.size() == 1u);
    CHECK(s.muscles.size() == 1u && s.points.size() == 1u);
    CHECK(s.sensors.size() == 6u);
    CHECK(s.numInputs() == 7 && s.numOutputs() == 2);

    CHECK(s.joints[0].limb1 == 0 && s.joints[0].limb2 == 1);
    CHECK(s.muscles[0].limb1 == 0 && s.muscles[0].limb2 == 1);
    CHECK(s.points[0].limb == 1);
    CHECK(s.limbIndex("b") == 1 && s.limbIndex("zz") == -1);
    CHECK(s.pointIndex("head") == 0 && s.jointIndex("j") == 0);

    CHECK(s.sensors[0].type == SensorSpec::Joint && s.sensors[0].target == 0);
    CHECK(s.sensors[1].type == SensorSpec::Height && s.sensors[1].target == 0);
    CHECK(s.sensors[1].min == 0 && s.sensors[1].max == 2);
    CHECK(s.sensors[4].type == SensorSpec::Velocity && s.sensors[4].vertical);
    CHECK(s.sensors[5].type == SensorSpec::Contact && s.sensors[5].target == 0);

    const LimbSpec &b = s.limbs[1];
    CHECK(b.angle == 0.3f);
    CHECK(b.angularDamping && *b.angularDamping == 0.1f);
    CHECK(b.shapes[1].type == ShapeSpec::Ball && b.shapes[1].radius == 0.05f);
    CHECK(b.shapes[1].friction == 1.5f && b.shapes[1].density == 2.0f);
}

TEST(specDefaults) {
    CreatureSpec s = parse(withChange("maxForce = 10.0; maxPower = 5.0; ", ""));
    const LimbSpec &a = s.limbs[0];
    CHECK(!a.angularDamping);			// Box2D's default
    CHECK(a.shapes[0].friction == 0.2f);	// Box2D 2.x default
    CHECK(s.limbs[1].shapes[1].groupIndex == 0);
    CHECK(std::isinf(s.muscles[0].maxForce) && std::isinf(s.muscles[0].maxPower));

    CreatureSpec noAngle = parse(withChange("angle = 0.0;", ""));
    CHECK(noAngle.limbs[0].angle == 0.0f);

    // Sensors and named points are optional
    string bare = Base.substr(0, Base.find("shapes = ( { name"));
    CreatureSpec b = parse(bare);
    CHECK(b.points.empty() && b.sensors.empty() && b.numInputs() == 1);
}

TEST(integersAcceptedForNumbers) {
    CreatureSpec s = parse(withChange("lowerAngle = -1.0;", "lowerAngle = -1;"));
    CHECK(s.joints[0].lowerAngle == -1.0f);
}

TEST(unknownNamesAreRejected) {
    CHECK(mentions(parseError(withChange(R"(obj2 = "b"; pos2)", R"(obj2 = "nope"; pos2)")),
		   "muscle 0 'm': unknown limb 'nope'"));
    CHECK(mentions(parseError(withChange(R"(obj1 = "a"; obj2 = "b";)", R"(obj1 = "nope"; obj2 = "b";)")),
		   "joint 0 'j': unknown limb 'nope'"));
    CHECK(mentions(parseError(withChange(R"(target = "j";)", R"(target = "nope";)")),
		   "unknown joint 'nope'"));
    CHECK(mentions(parseError(withChange(R"(target = "head";)", R"(target = "nope";)")),
		   "unknown shape 'nope'"));
    CHECK(mentions(parseError(withChange(R"(body = "b";)", R"(body = "nope";)")),
		   "shape 0 'head': unknown limb 'nope'"));
}

TEST(badValuesAreRejected) {
    CHECK(mentions(parseError(withChange(R"(type = "JointSensor")", R"(type = "NoSuchSensor")")),
		   "unknown sensor type"));
    CHECK(mentions(parseError(withChange(R"(type = "box"; w = 0.05; h = 0.25; density = 1.0; groupIndex = -1; } ); },)",
					 R"(type = "cone"; w = 0.05; h = 0.25; density = 1.0; groupIndex = -1; } ); },)")),
		   "limb 0 'a': shape 0: unknown shape type 'cone'"));
    CHECK(mentions(parseError(withChange(R"(type = "revolute")", R"(type = "prismatic")")),
		   "unknown joint type 'prismatic'"));
    CHECK(mentions(parseError(withChange(R"(axis = "y")", R"(axis = "z")")), "axis"));
    CHECK(mentions(parseError(withChange("minK = 10.0; ", "")), "missing 'minK'"));
    CHECK(mentions(parseError(withChange("joints = (", "joints_missing = (")),
		   "no 'joints' section"));
    CHECK(mentions(parseError(withChange("minK = 10.0;", "minK = \"stiff\";")),
		   "'minK' must be a number"));
    CHECK(mentions(parseError(withChange(R"({ name = "b";)", R"({ name = "a";)")),
		   "duplicate name 'a'"));
}

TEST(builtCreatureMatchesSpec) {
    CreatureSpec s = parse(Base);
    World w;
    CreatureP c = w.createCreature(s);
    CHECK(c->bodies().size() == 2u);
    CHECK(c->limbs.size() == 2u && c->joints.size() == 1u);
    CHECK(c->muscles.size() == 1u && c->shapes.count("head") == 1u);
    CHECK(c->numSensors() == s.numInputs());

    for (size_t i = 0; i < s.limbs.size(); ++i) {
	BodyId b = c->bodies()[i];
	Vec2 p = b2Body_GetPosition(b);
	CHECK(p.x == s.limbs[i].position.x && p.y == s.limbs[i].position.y);
	// b2MakeRot uses an approximate sine/cosine (0.3 rad reads back as
	// about 0.3015), so only close
	CHECK_NEAR(b2Rot_GetAngle(b2Body_GetRotation(b)), s.limbs[i].angle, 5e-3);
	CHECK(b2Body_GetShapeCount(b) == (int)s.limbs[i].shapes.size());
	CHECK(B2_ID_EQUALS(b, c->limbs[s.limbs[i].name]));
    }
    CHECK(c->muscles[0]->maxPowerLimit() == 5.0f);

    double in[7];
    c->setInput(in);
    CHECK(in[0] == 1.0);			// bias first
    for (double v : in) CHECK(v >= 0.0 && v <= 1.0);
}

// One spec, many creatures: the spec is read-only once parsed
TEST(oneSpecBuildsManyCreatures) {
    CreatureSpec s = parse(Base);
    World w1, w2;
    CreatureP a = w1.createCreature(s), b = w2.createCreature(s);
    for (int i = 0; i < 30; ++i) { w1.step(); w2.step(); }
    Vec2 pa = b2Body_GetPosition(a->bodies()[1]), pb = b2Body_GetPosition(b->bodies()[1]);
    CHECK(pa.x == pb.x && pa.y == pb.y);
}

static const char **extraConfigs;
static int nExtra;

TEST(shippedConfigsBuild) {
    for (int i = 0; i < nExtra; ++i) {
	libconfig::Config config;
	config.readFile(extraConfigs[i]);
	try {
	    CreatureSpec s = parseCreatureSpec(config);
	    World w;
	    CreatureP c = w.createCreature(s);
	    CHECK(c->numSensors() == s.numInputs());
	} catch (std::exception &e) {
	    fprintf(stderr, "  %s: %s\n", extraConfigs[i], e.what());
	    CHECK(false);
	}
    }
}

int main (int argc, const char **argv) {
    extraConfigs = argv + 1;
    nExtra = argc - 1;
    return check::runTests();
}
