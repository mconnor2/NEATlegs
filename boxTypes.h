#ifndef __BOX_TYPE_H__
#define __BOX_TYPE_H__

#include <box2d/box2d.h>
#include <string>
#include <vector>
#include <map>
#include <memory>

class World;
class Creature;
class Muscle;

typedef b2Vec2 Vec2;

// Box2D 3 hands out ids (small value types) instead of pointers; the World
// owns everything and frees it all when it is destroyed.
typedef b2BodyId BodyId;
struct BodyPos {
    Vec2 defaultPos;
    float angle;
    BodyId b;
};
typedef std::vector<BodyPos> bodyPosList;
typedef std::map<std::string, BodyId> bodyMap;

struct shapePos {
    Vec2 localPos;
    BodyId b;
};
typedef std::map<std::string, shapePos> shapeMap;

typedef b2JointId JointId;
typedef std::map<std::string, JointId> jointMap;

typedef std::shared_ptr<Muscle> MuscleP;
typedef std::vector<MuscleP> muscleList;

typedef std::shared_ptr<Creature> CreatureP;
typedef std::vector<CreatureP> creatureList;

#endif
