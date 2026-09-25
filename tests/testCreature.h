#ifndef __TESTS_TEST_CREATURE_H
#define __TESTS_TEST_CREATURE_H

// A small creature for tests: limb "a" (box, bottom 0.25 m above the
// ground) and limb "b" above it (box plus ball, tilted 0.3 rad), joint "j",
// muscle "m", named point "head" on b, and one sensor of each type.

#include <string>

#include <libconfig.h++>

#include "CreatureSpec.h"

namespace testCreature {

// Two limbs, one joint, one muscle, one of each sensor type
inline const std::string Text = R"(
limbs = (
    { name = "a"; position = { x = 0.0; y = 0.5; }; angle = 0.0;
      shapes = ( { type = "box"; w = 0.05; h = 0.25; density = 1.0;
                   groupIndex = -1; } ); },
    { name = "b"; position = { x = 0.0; y = 1.0; }; angle = 0.3;
      angularDamping = 0.1;
      shapes = ( { type = "box"; w = 0.05; h = 0.25; density = 1.0;
                   groupIndex = -1; },
                 { type = "ball"; radius = 0.05;
                   position = { x = 0.0; y = 0.25; }; density = 2.0;
                   friction = 1.5; } ); });
joints = (
    { name = "j"; type = "revolute"; obj1 = "a"; obj2 = "b";
      position = { x = 0.0; y = 0.75; };
      lowerAngle = -1.0; upperAngle = 1.0; });
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

inline CreatureSpec spec (const std::string &text = Text) {
    libconfig::Config config;
    config.readString(text);
    return parseCreatureSpec(config);
}

}

#endif
