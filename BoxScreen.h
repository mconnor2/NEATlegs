#ifndef __BOXSCREEN_H
#define __BOXSCREEN_H

#include "Canvas.h"
#include "Renderer.h"
#include "boxTypes.h"

/**
 * Draws the physics world onto a Canvas: maps world coordinates (metres,
 * y up) to pixels (y down) at a fixed scale, with a camera that can follow
 * a point.  Doesn't depend on SDL, so it can be tested with a recording
 * canvas.
 */
class BoxScreen : public Renderer {
    public:
        // World origin starts at the bottom centre of the canvas.  A null
        // canvas makes every call a no-op.
        BoxScreen (Canvas *c, float _pM = 10.0f);

        void polygon (const Vec2 *points, int n, Color c) override;
        void circle (Vec2 centre, float radius, Color c) override;
        void segment (Vec2 a, Vec2 b, Color c) override;

        // A dot grid, one metre apart
        void drawGrid ();

        // Shift the camera so this world point stays within the borders
        void keepViewable (const Vec2 &pW);

        inline void box2pixel (const Vec2 &boxV, Vec2 &screenV) const;

        //Pixel border kept around a followed point
        static const int SideBorder = 300, TopBottomBorder = 32;
    private:
        float pM;               //pixels/meter
        Vec2 BoxOriginP;        //location of box origin in pixel space

        Canvas *canvas;
};

inline void BoxScreen::box2pixel (const Vec2 &boxV, Vec2 &screenV) const {
    screenV.x = BoxOriginP.x + pM*boxV.x;
    screenV.y = BoxOriginP.y - pM*boxV.y;
}

#endif
