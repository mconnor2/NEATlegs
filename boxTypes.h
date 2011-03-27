#ifndef __BOX_TYPE_H__
#define __BOX_TYPE_H__

#include <Box2D/Box2D.h>
#include <string>
#include <vector>
#include <map>

#include <boost/shared_ptr.hpp>

class World;
class Creature;
class Muscle;

typedef b2Vec2 Vec2;

typedef b2Body Body;
typedef boost::shared_ptr<Body> BodyP;
struct BodyPos {
    Vec2 defaultPos;
    float angle;
    BodyP b;
};
typedef std::vector<BodyPos> bodyPosList;
typedef std::map<std::string, BodyP> bodyMap;

typedef b2Shape Shape;
struct shapePos {
    Vec2 localPos;
    BodyP b;
};
typedef std::map<std::string, shapePos> shapeMap;

typedef b2Fixture Fixture;

typedef b2Joint Joint;
typedef boost::shared_ptr<Joint> JointP;

typedef b2RevoluteJoint RevoluteJoint;
typedef boost::shared_ptr<RevoluteJoint> RevoluteJointP;
typedef std::vector<RevoluteJointP> jointList;
typedef std::map<std::string, RevoluteJointP> jointMap;

typedef boost::shared_ptr<Muscle> MuscleP;
typedef std::vector<MuscleP> muscleList;

typedef boost::shared_ptr<Creature> CreatureP;
typedef std::vector<CreatureP> creatureList;

#endif
