#ifndef __WORLD_H
#define __WORLD_H

#include <Box2D.h>
#include <vector>
#include <libconfig.h++>

#include "boxTypes.h"

using namespace std;

class BoxScreen;

/**
 * World management class.  Stores pointer to world, handles overall
 * simulation stepping, etc.
 */
class World {
    public:
	World (float _hz = 60.0f, int _Viterations = 10, int _Piterations = 10);

	~World ();
    
	void step ();
	
	void draw (BoxScreen *screen) const;

	//CreatureP createCreature (const char* creatureConfig);
	CreatureP createCreature (const libconfig::Config &creatureConfig);
	
	//int addCreature (CreatureP &c);
	
	BodyP createBody (const b2BodyDef *def); 
	JointP createJoint (const b2JointDef *def); 
	
	static const float fGravity;

    private:
	b2World *b2W;
	
	BodyP ground;

	creatureList beings; 

	float32 timeStep;
	int32 velocityIterations, positionIterations;
};

#endif
