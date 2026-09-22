#ifndef __CREATURE_SPEC_H
#define __CREATURE_SPEC_H

#include <limits>
#include <optional>
#include <string>
#include <vector>

#include "boxTypes.h"

namespace libconfig { class Config; }

/**
 * Plain description of a creature, parsed once from a config file.
 *
 * Everything is validated and every name resolved to an index when it is
 * parsed, so building a creature from a spec can't fail, and a spec can be
 * shared read-only by any number of threads (libconfig itself can't: it
 * lazily builds its C++ wrappers even on reads).
 */
struct ShapeSpec {
    enum Type { Box, Ball } type = Box;
    float w = 0, h = 0;			//Box half extents
    float radius = 0;			//Ball
    Vec2 position = {0, 0};		//Ball centre, local to the limb
    float density = 0;
    float friction = 0.2f;		//Box2D 2.x default the configs assume
    int groupIndex = 0;
};

struct LimbSpec {
    std::string name;
    Vec2 position = {0, 0};
    float angle = 0;
    std::optional<float> angularDamping;	//Box2D default if unset
    std::vector<ShapeSpec> shapes;
};

struct JointSpec {			//Revolute, limits always enabled
    std::string name;
    int limb1 = -1, limb2 = -1;
    Vec2 anchor = {0, 0};		//World position
    float lowerAngle = 0, upperAngle = 0;
};

struct MuscleSpec {
    static constexpr float Unlimited = std::numeric_limits<float>::infinity();

    std::string name;
    int limb1 = -1, limb2 = -1;
    Vec2 pos1 = {0, 0}, pos2 = {0, 0};	//Attachment points, limb local
    float minK = 0, maxK = 0, minEq = 0, maxEq = 0, kd = 0;
    float maxForce = Unlimited, maxPower = Unlimited;
};

struct PointSpec {			//Named point on a limb ("shapes" section)
    std::string name;
    int limb = -1;
    Vec2 localPos = {0, 0};
};

struct SensorSpec {
    enum Type { Joint, Height, BodyAngle, AngularVelocity, Velocity, Contact };
    Type type = Joint;
    int target = -1;	//Joint index for Joint, point index for Height,
			// limb index otherwise
    double min = 0, max = 0;		//Unused for Joint and Contact
    bool vertical = false;		//Velocity: y axis instead of x
};

struct CreatureSpec {
    std::vector<LimbSpec> limbs;
    std::vector<JointSpec> joints;
    std::vector<MuscleSpec> muscles;
    std::vector<PointSpec> points;
    std::vector<SensorSpec> sensors;

    // Index by name, or -1
    int limbIndex (const std::string &name) const;
    int jointIndex (const std::string &name) const;
    int pointIndex (const std::string &name) const;

    // Network inputs (sensors plus bias) and outputs (two per muscle)
    int numInputs () const { return (int)sensors.size() + 1; }
    int numOutputs () const { return 2 * (int)muscles.size(); }
};

// Parse the limbs, joints, muscles, shapes and sensors sections.  Throws
// std::runtime_error naming the offending entry if anything is missing,
// malformed or refers to an unknown name.
CreatureSpec parseCreatureSpec (const libconfig::Config &config);

#endif
