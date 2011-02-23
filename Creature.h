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
    //Creature (World *w);	
    /* Create Creature's body and add it to the world */
    int initFromFile(const char *configFile, World *w);

    void wake ();	//make sure all the bodies are awake
    void update ();

    void draw (BoxScreen *screen) const;

    bool getShapePosition(const string &name, Vec2 *p) const;

    void reset ();

    // If we want to access some body parts by name
    bodyMap limbs;
    jointMap joints;
    shapeMap shapes;

    /* Muscles necessary for controlling creature */
    muscleList muscles;
    
    private:

    /* Bodies and joints specifying the creature */
    bodyList parts;

    /* Brains controlling the muscles */
};

class Muscle {
    public:
	Muscle (const BodyP &b1, const Vec2 &l1, 
		const BodyP &b2, const Vec2 &l2,
		float _minK, float _maxK, 
		float _minEq, float _maxEq,
		float _kd) :
		body1(b1), end1L(l1), body2(b2), end2L(l2),
		minK(_minK), maxK(_maxK), minEq(_minEq), maxEq(_maxEq),
		k((_minK + _maxK) / 2.), eq((_minEq + _maxEq)/2.), kd(_kd) 
	{ }

	float update ();

	void draw (BoxScreen *screen) const;

	void scaleLength (double sc);

	void scaleStrength (double sc);

    private:
	float k, minK, maxK;	//Hook's constant
	float kd;		//Spring dampening
	float eq, minEq, maxEq;	//Spring's equilibrium distance

	BodyP body1, body2;	//Bodies muscle connects
	Vec2 end1L, end2L;	//Local point of contact (fixed)
};

#endif
