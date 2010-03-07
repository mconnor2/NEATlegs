#ifndef __CREATURE_H
#define __CREATURE_H

#include <Box2D.h>
#include <vector>
#include <map>

#include "boxTypes.h"
#include "BoxScreen.h"
#include "World.h"

class World;
class Creature;
class Muscle;

using namespace std;

class Creature {
    public:
    /* Create Creature's body and add it to the world */
    //XXX Should take in file specifying geometry
    Creature (World *w);	
    
    void wake ();	//make sure all the bodies are awake
    void update ();

    void draw (BoxScreen *screen) const;

    void reset ();

    private:

    /* Bodies and joints specifying the creature */
    bodyList parts;
    jointList joints;

    /* Muscles necessary controlling creature */
    muscleList muscles;

    /* Brains controlling the muscles */
};

class Muscle {
    public:
	Muscle (const BodyP &b1, const Vec2 &l1, 
		const BodyP &b2, const Vec2 &l2,
		float _k, float _eq) :
		body1(b1), end1L(l1), body2(b2), end2L(l2),
		k(_k), eq(_eq) 
	{ }

	float update ();

	void draw (BoxScreen *screen) const;
    
    private:
	float k;		//Hook's constant
	float eq;		//Spring's equilibrium distance

	BodyP body1, body2;	//Bodies muscle connects
	Vec2 end1L, end2L;	//Local point of contact (fixed)
};

#endif
