#ifndef __RENDERER_H
#define __RENDERER_H

#include "Color.h"
#include "boxTypes.h"

/**
 * Receives what the physics draws, in world coordinates (metres, y up).
 * World and Creature draw through this, so the physics library doesn't
 * depend on any graphics library; BoxScreen maps it onto a Canvas.
 */
class Renderer {
    public:
        virtual ~Renderer () { }

        // Closed polygon through n points
        virtual void polygon (const Vec2 *points, int n, Color c) = 0;
        virtual void circle (Vec2 centre, float radius, Color c) = 0;
        virtual void segment (Vec2 a, Vec2 b, Color c) = 0;
};

// Colors the physics draws with
const Color BodyColor = 0x00FF00FF;     //Box outlines
const Color BallColor = 0xFF0000FF;     //Circles
const Color MuscleColor = 0xFF0000FF;

#endif
