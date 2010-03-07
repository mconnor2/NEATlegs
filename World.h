#ifndef __WORLD_H
#define __WORLD_H

#include <Box2D.h>
#include <vector>

#include "boxTypes.h"
#include "Creature.h"
#include "BoxScreen.h"

using namespace std;

class Creature;

/**
 * World management class.  Stores pointer to world, handles overall
 * simulation stepping, etc.
 */
class World {
    public:
	World (float _hz = 120.0f, int _iterations = 20);

	~World ();
    
	void step ();
	
	void draw (BoxScreen *screen) const;

	BodyP createBody (const b2BodyDef *def); 
	JointP createJoint (const b2JointDef *def); 

	int addCreature (Creature *c);
	int addCreature (CreatureP &c);

    private:
	b2World *b2W;

	BodyP ground;

	creatureList beings; 

	float32 timeStep;
	int32 iterations;
};

#endif
