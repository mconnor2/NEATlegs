#include "World.h"
#include "Creature.h"
#include "BoxScreen.h"

#include <iostream>
#include <mutex>

const float World::fGravity = 10.0;

//Box2D keeps every world in a global table (max 128) that isn't
// protected, so worlds must be created and destroyed one at a time.  Once
// created, each world is only touched by the thread that owns it.
static std::mutex worldTableMutex;

//libconfig lazily builds its C++ Setting wrappers on first access, so even
// read-only lookups on a shared Config race.  Creature construction is cheap
// compared to simulation, so just read configs one thread at a time.
static std::mutex configMutex;

World::World (float _hz, int _subSteps) :
	      timeStep(1.0f/_hz), subSteps(_subSteps)
{
    //Set gravity pointing downward
    b2WorldDef worldDef = b2DefaultWorldDef();
    worldDef.gravity = {0.0f, -fGravity};
    worldDef.enableSleep = true;

    //Create world
    {
	std::lock_guard<std::mutex> lock(worldTableMutex);
	b2W = b2CreateWorld(&worldDef);
    }

    //Create ground
    b2BodyDef groundBodyDef = b2DefaultBodyDef();
    groundBodyDef.position = {0.0f, -10.0f};
    ground = b2CreateBody(b2W, &groundBodyDef);

    b2Polygon groundBox = b2MakeBox(100.0f, 10.0f);

    b2ShapeDef groundShapeDef = b2DefaultShapeDef();
    groundShapeDef.density = 0.0f;
    groundShapeDef.material.friction = 1.0f;

    b2CreatePolygonShape(ground, &groundShapeDef, &groundBox);
}

World::~World () {
    //Creatures only hold ids, so it's fine to let them outlive the world
    std::lock_guard<std::mutex> lock(worldTableMutex);
    b2DestroyWorld(b2W);
}

CreatureP World::createCreature (const libconfig::Config &creatureConfig) {
    CreatureP cp(new Creature());
    std::unique_lock<std::mutex> lock(configMutex);
    if (!cp->initFromFile(creatureConfig, this)) {
	//Problem with initialization, so return empty CreatureP
	return CreatureP();
    }

    beings.push_back(cp);
    return cp;
}

void World::step () {
    //Update forces (muscles) on objects.  Box2D clears applied forces
    // after every step.
    for (auto &c : beings) c->update();

    b2World_Step(b2W, timeStep, subSteps);

    for (auto &c : beings) c->afterStep(timeStep);
}

void World::draw (BoxScreen *screen) const {
    //Draw the ground, and the draw all the bodies.
    screen->drawBody(ground);

    for (auto &c : beings) c->draw(screen);
}
