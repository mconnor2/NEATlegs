#include "World.h"
#include "Creature.h"
#include "BoxScreen.h"

#include <iostream>
#include <boost/mem_fn.hpp>
#include <boost/bind.hpp>

const float World::fGravity = 10.0;

void bodyDestroy(Body *p) {
#ifdef __DEBUG
    cerr<<"Destroying a body, but not really."<<endl;
#endif
}

void jointDestroy(Joint *j) {
#ifdef __DEBUG
    cerr<<"Destroying a joint, but not really."<<endl;
#endif
}

World::World (float _hz, int _Viterations, int _Piterations) : 
	      timeStep(1.0f/_hz), velocityIterations(_Viterations),
	      positionIterations(_Piterations)
{
    //Set gravity pointing downward
    b2Vec2 gravity(0.0f, -fGravity);
    bool doSleep = true;

    //Create world
    b2W = new b2World(gravity, doSleep);

    //Create ground
    b2BodyDef groundBodyDef;
    groundBodyDef.position.Set(0.0f, -10.0f);
    //groundBodyDef.AddShape(&groundBoxDef);

    ground.reset(b2W->CreateBody(&groundBodyDef),
		 bodyDestroy);
		 //boost::bind(&b2World::DestroyBody, b2W, _1));
    
    b2PolygonShape groundBoxDef;
    groundBoxDef.SetAsBox(100.0f, 10.0f);
   
    b2FixtureDef groundFixtureDef;
    groundFixtureDef.shape = &groundBoxDef;

    groundFixtureDef.density = 0.0f;
    groundFixtureDef.friction = 1.0f;

    ground->CreateFixture(&groundFixtureDef);
}

World::~World () {
    delete b2W;
}

BodyP World::createBody (const b2BodyDef *def) {
    //Use Box2d world::DestroyBody for clean up
    //  good chance this is a bad idea...
    BodyP b(b2W->CreateBody(def),
	    bodyDestroy);
	    //boost::bind(&b2World::DestroyBody, b2W, _1));
    return b;
}

JointP World::createJoint (const b2JointDef *def) {
    JointP j(b2W->CreateJoint(def),
	     jointDestroy);
	     //boost::bind(&b2World::DestroyJoint, b2W, _1));
    return j;
}

CreatureP World::createCreature (const libconfig::Config &creatureConfig) {
    CreatureP cp(new Creature());
    if (!cp->initFromFile(creatureConfig, this)) {
	//Problem with initialization, so return empty CreatureP
	return CreatureP();
    }
    
    int id = beings.size();
    beings.push_back(cp);
    return cp;
}
/*	
int World::addCreature (CreatureP &c) {
    int id = beings.size();
    beings.push_back(c);
    return id;
}
*/
void World::step () {
    //Update forces (muscles) on objects
    for_each(beings.begin(), beings.end(),
	     boost::mem_fn(&Creature::update));

    b2W->Step(timeStep, velocityIterations, positionIterations);
    b2W->ClearForces();
}

void World::draw (BoxScreen *screen) const {
    //Draw the ground, and the draw all the bodies.
    screen->drawBody(ground);

    for_each(beings.begin(),beings.end(),
    	     boost::bind(&Creature::draw, _1, screen));

}
	
