#ifndef __WORLD_H
#define __WORLD_H

#include <Box2D.h>
#include <vector>

#include "boxTypes.h"
//#include "Creature.h"
//#include "BoxScreen.h"

using namespace std;

class BoxScreen;
class Creature;

/**
 * World management class.  Stores pointer to world, handles overall
 * simulation stepping, etc.
 */
class World {
    public:
	World (float _hz = 100.0f, int _iterations = 20);

	~World ();
    
	void step ();
	
	void draw (BoxScreen *screen) const;

	CreatureP createCreature (const char* creatureConfig);
	//int addCreature (CreatureP &c);
	
	static const float fGravity;

	friend class Creature;

    private:
	b2World *b2W;
	
	BodyP createBody (const b2BodyDef *def); 
	JointP createJoint (const b2JointDef *def); 

	BodyP ground;

	creatureList beings; 

	float32 timeStep;
	int32 iterations;
};

#endif
