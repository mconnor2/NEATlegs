#ifndef __WORLD_H
#define __WORLD_H

#include <box2d/box2d.h>
#include <vector>
#include <libconfig.h++>

#include "boxTypes.h"

using namespace std;

class BoxScreen;

/**
 * World management class.  Owns the Box2D world, handles overall
 * simulation stepping, etc.
 */
class World {
    public:
	World (float _hz = 60.0f, int _subSteps = 4);

	~World ();

	World (const World &) = delete;
	World &operator= (const World &) = delete;

	void step ();

	void draw (BoxScreen *screen) const;

	CreatureP createCreature (const libconfig::Config &creatureConfig);

	b2WorldId id () const { return b2W; }

	static const float fGravity;

    private:
	b2WorldId b2W;

	BodyId ground;

	creatureList beings;

	float timeStep;
	int subSteps;
};

#endif
