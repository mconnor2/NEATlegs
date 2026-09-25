#include "BoxScreen.h"

#include <cmath>

BoxScreen::BoxScreen (Canvas *c, float _pM) : pM(_pM), canvas(c)
{
    if (canvas) {
        BoxOriginP = {canvas->width() / 2.0f, canvas->height() - 1.0f};
    } else {
        BoxOriginP = {0.0f, 0.0f};
    }
}

/**
 * Make sure that the world point passed is viewable in screen space.
 * If not, shift origin accordingly.
 */
void BoxScreen::keepViewable (const Vec2 &pW) {
    if (!canvas) return;

    const float LeftBorder = SideBorder,
                RightBorder = canvas->width() - SideBorder,
                TopBorder = TopBottomBorder,
                BottomBorder = canvas->height() - TopBottomBorder;

    Vec2 xP;
    box2pixel(pW, xP);

    if (xP.x < LeftBorder)
        BoxOriginP.x += LeftBorder - xP.x;
    if (xP.x > RightBorder)
        BoxOriginP.x -= xP.x - RightBorder;
    if (xP.y > BottomBorder)
        BoxOriginP.y += BottomBorder - xP.y;
    if (xP.y < TopBorder)
        BoxOriginP.y -= xP.y - TopBorder;
}

void BoxScreen::polygon (const Vec2 *points, int n, Color c) {
    if (!canvas || n < 2) return;
    Vec2 firstP, v1P, v2P;
    box2pixel(points[0], firstP);
    v1P = firstP;
    for (int i = 1; i < n; ++i) {
        box2pixel(points[i], v2P);
        canvas->line(v1P.x, v1P.y, v2P.x, v2P.y, c);
        v1P = v2P;
    }
    //Complete the loop
    canvas->line(v1P.x, v1P.y, firstP.x, firstP.y, c);
}

void BoxScreen::circle (Vec2 centre, float radius, Color c) {
    if (!canvas) return;
    Vec2 xP;
    box2pixel(centre, xP);
    canvas->circle(xP.x, xP.y, pM * radius, c);
}

void BoxScreen::segment (Vec2 a, Vec2 b, Color c) {
    if (!canvas) return;
    Vec2 p1P, p2P;
    box2pixel(a, p1P);
    box2pixel(b, p2P);
    canvas->line(p1P.x, p1P.y, p2P.x, p2P.y, c);
}

/**
 * Draw a grid of small crosses, one every meter starting from the origin.
 *
 * From the origin point, need to figure out where the minimum x,y coordinates
 *  are that are in the screen space, then draw from there.
 *
 *  min_c o.x + c*pM >= 0
 *        c >= -o.x / pM
 *        c = ceil(-o.x / pM)
 */
void BoxScreen::drawGrid () {
    if (!canvas) return;
    float sx = BoxOriginP.x - floor(BoxOriginP.x / pM)*pM,
          sy = BoxOriginP.y - floor(BoxOriginP.y / pM)*pM;

    const Color grey = 0x00888888;
    for (float y = sy; y<canvas->height(); y+=pM) {
        for (float x = sx; x<canvas->width(); x+=pM) {
            canvas->point(x-1, y, grey);
            canvas->point(x+1, y, grey);
            canvas->point(x, y-1, grey);
            canvas->point(x, y+1, grey);
            canvas->point(x, y, grey);
        }
    }
}
