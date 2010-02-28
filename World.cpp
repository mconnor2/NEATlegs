#include "World.h"

World::World (float _hz, int _iterations) : 
	      timeStep(1.0f/_hz), iterations(_iterations) 
{
    //Code taken basically straight from Box2D user manual
    b2AABB worldAABB;
    worldAABB.lowerBound.Set(-100.0f, -100.0f);
    worldAABB.upperBound.Set(150.0f, 150.0f);

    //Set gravity pointing downward
    b2Vec2 gravity(0.0f, -10.0f);
    bool doSleep = true;

    //Create world
    b2W = new b2World(worldAABB, gravity, doSleep);

    //Create ground
    b2BodyDef groundBodyDef;
    groundBodyDef.position.Set(0.0f, -10.0f);
    //groundBodyDef.AddShape(&groundBoxDef);

    ground = b2W->CreateBody(&groundBodyDef);
    
    b2PolygonDef groundBoxDef;
    groundBoxDef.SetAsBox(50.0f, 10.0f);
    groundBoxDef.density = 0.0f;
    groundBoxDef.friction = 1.0f;

    ground->CreateShape(&groundBoxDef);
}

World::~World () {
    delete b2W;
}

b2Body *World::createBody (const b2BodyDef *def) {
    return b2W->CreateBody(def);
}

b2Joint *World::createJoint (const b2JointDef *def) {
    return b2W->CreateJoint(def);
}
	
int World::addCreature (Creature *c) {
    int id = beings.size();
    beings.push_back(c);
    return id;
}

void World::step () {
    //Update forces (muscles) on objects
    for (vector<Creature *>::iterator ci = beings.begin();
        ci != beings.end(); ++ci)
    {
	(*ci)->update();
    }

    b2W->Step(timeStep, iterations);
}

void World::draw (BoxScreen *screen) {
    //Draw the ground, and the draw all the bodies.
    screen->drawBody(ground);

    for (vector<Creature *>::iterator ci = beings.begin();
        ci != beings.end(); ++ci)
    {
	Creature *c = *ci;
	c->draw(screen);
    }
    
}
	
