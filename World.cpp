#include "World.h"
#include "Creature.h"
#include "Renderer.h"

#include <iostream>
#include <mutex>
#include <vector>

const float World::fGravity = 10.0;

//Box2D keeps every world in a global table (max 128) that isn't
// protected, so worlds must be created and destroyed one at a time.  Once
// created, each world is only touched by the thread that owns it.
static std::mutex worldTableMutex;

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

CreatureP World::createCreature (const CreatureSpec &spec) {
    CreatureP cp(new Creature(spec, *this));
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

void World::draw (Renderer &r) const {
    drawBody(ground, r);
    for (auto &c : beings) c->draw(r);
}

void drawBody (BodyId b, Renderer &r) {
    int n = b2Body_GetShapeCount(b);
    std::vector<b2ShapeId> shapes(n);
    b2Body_GetShapes(b, shapes.data(), n);
    for (b2ShapeId s : shapes) {
	switch (b2Shape_GetType(s)) {
	    case b2_circleShape: {
		b2Circle circle = b2Shape_GetCircle(s);
		r.circle(b2Body_GetWorldPoint(b, circle.center), circle.radius,
			 BallColor);
		break;
	    }
	    case b2_polygonShape: {
		b2Polygon poly = b2Shape_GetPolygon(s);
		if (poly.count < 2) break;
		Vec2 pts[B2_MAX_POLYGON_VERTICES];
		for (int i = 0; i < poly.count; ++i)
		    pts[i] = b2Body_GetWorldPoint(b, poly.vertices[i]);
		r.polygon(pts, poly.count, BodyColor);
		break;
	    }
	    default:
		break;
	}
    }
}
