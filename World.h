#ifndef __WORLD_H
#define __WORLD_H

#include <box2d/box2d.h>
#include <vector>

#include "boxTypes.h"

struct CreatureSpec;
class Renderer;

// Draw a body's shapes (boxes as polygons, balls as circles)
void drawBody (BodyId b, Renderer &r);

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

        // Ground, then each creature
        void draw (Renderer &r) const;

        // Build a creature from a parsed spec (thread safe: the spec is
        // only read)
        CreatureP createCreature (const CreatureSpec &spec);

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
