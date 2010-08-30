#include "World.h"

#include <boost/mem_fn.hpp>
#include <boost/bind.hpp>

World::World (float _hz, int _iterations) : 
	      timeStep(1.0f/_hz), iterations(_iterations) 
{
    //Code taken basically straight from Box2D user manual
    b2AABB worldAABB;
    worldAABB.lowerBound.Set(-100.0f, -100.0f);
    worldAABB.upperBound.Set(500.0f, 500.0f);

    //Set gravity pointing downward
    b2Vec2 gravity(0.0f, -10.0f);
    bool doSleep = true;

    //Create world
    b2W = new b2World(worldAABB, gravity, doSleep);

    //Create ground
    b2BodyDef groundBodyDef;
    groundBodyDef.position.Set(0.0f, -10.0f);
    //groundBodyDef.AddShape(&groundBoxDef);

    ground.reset(b2W->CreateBody(&groundBodyDef),
		 boost::bind(&b2World::DestroyBody, b2W, _1));
    
    b2PolygonDef groundBoxDef;
    groundBoxDef.SetAsBox(500.0f, 10.0f);
    groundBoxDef.density = 0.0f;
    groundBoxDef.friction = 1.0f;

    ground->CreateShape(&groundBoxDef);
}

World::~World () {
    delete b2W;
}

BodyP World::createBody (const b2BodyDef *def) {
    //Use Box2d world::DestroyBody for clean up
    //  good chance this is a bad idea...
    BodyP b(b2W->CreateBody(def), 
	    boost::bind(&b2World::DestroyBody, b2W, _1));
    return b;
}

JointP World::createJoint (const b2JointDef *def) {
    JointP j(b2W->CreateJoint(def),
	     boost::bind(&b2World::DestroyJoint, b2W, _1));
    return j;
}

int World::addCreature (Creature *c) {
    CreatureP cp(c);
    return addCreature(cp);
}
	
int World::addCreature (CreatureP &c) {
    int id = beings.size();
    beings.push_back(c);
    return id;
}

void World::step () {
    //Update forces (muscles) on objects
    for_each(beings.begin(), beings.end(),
	     boost::mem_fn(&Creature::update));

    b2W->Step(timeStep, iterations);
}

void World::draw (BoxScreen *screen) const {
    //Draw the ground, and the draw all the bodies.
    screen->drawBody(ground);

    for_each(beings.begin(),beings.end(),
    	     boost::bind(&Creature::draw, _1, screen));

}
	
