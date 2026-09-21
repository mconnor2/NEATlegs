#include "BoxScreen.h"

#include <cmath>
#include <iostream>
#include <vector>

using namespace std;

BoxScreen::BoxScreen (Display *d, float _pM) : pM(_pM), display(d)
{
    if (display) {
	BoxOriginP = {display->width() / 2.0f, display->height() - 1.0f};
    } else {
	BoxOriginP = {0.0f, 0.0f};
    }
}

/**
 * Make sure that the world point passed is viewable in screen space.
 * If not, shift origin accordingly.
 */
void BoxScreen::keepViewable (const Vec2 &pW) {
    if (!display) return;

    const float LeftBorder = SideBorder,
		RightBorder = display->width() - SideBorder,
		TopBorder = TopBottomBorder,
		BottomBorder = display->height() - TopBottomBorder;

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

void BoxScreen::drawBody (BodyId b) {
    int n = b2Body_GetShapeCount(b);
    vector<b2ShapeId> shapes(n);
    b2Body_GetShapes(b, shapes.data(), n);
    for (b2ShapeId s : shapes) {
	drawShape(b, s);
    }
}

void BoxScreen::drawShape (BodyId b, b2ShapeId s) {

    switch(b2Shape_GetType(s)) {
	case b2_circleShape:
	{
	    b2Circle circle = b2Shape_GetCircle(s);
	    Vec2 xP;
	    box2pixel(b2Body_GetWorldPoint(b, circle.center), xP);
	    float rP = pM * circle.radius;
	    //Draw a red circle
	    display->circle(xP.x, xP.y, rP, 0xFF0000FF);
	}
	break;
	case b2_polygonShape:
	{
	    b2Polygon poly = b2Shape_GetPolygon(s);

	    if (poly.count > 1) {
		Vec2 v1P, v2P;
		Vec2 firstP;
		box2pixel(b2Body_GetWorldPoint(b, poly.vertices[0]), firstP);
		v1P = firstP;
		for (int i = 1; i < poly.count; ++i)
		{
		    box2pixel(b2Body_GetWorldPoint(b, poly.vertices[i]), v2P);

		    //Draw a green polygon
		    display->line(v1P.x, v1P.y, v2P.x, v2P.y, 0x00FF00FF);
		    v1P = v2P;
		}
		//Complete the loop
		display->line(v1P.x, v1P.y, firstP.x, firstP.y, 0x00FF00FF);
	    }
	}
	break;

	default:
	    cerr<<"drawShape: unknown shape."<<endl;
    }
}

void BoxScreen::worldLine (const Vec2 &p1B, const Vec2 &p2B, Color c) {
    Vec2 p1P, p2P;
    box2pixel(p1B,p1P); box2pixel(p2B,p2P);
    display->line(p1P.x, p1P.y, p2P.x, p2P.y, c);
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
    float sx = BoxOriginP.x - floor(BoxOriginP.x / pM)*pM,
	  sy = BoxOriginP.y - floor(BoxOriginP.y / pM)*pM;

    const Color grey = 0x00888888;
    for (float y = sy; y<display->height(); y+=pM) {
	for (float x = sx; x<display->width(); x+=pM) {
	    display->point(x-1, y, grey);
	    display->point(x+1, y, grey);
	    display->point(x, y-1, grey);
	    display->point(x, y+1, grey);
	    display->point(x, y, grey);
	}
    }
}
