#ifndef __BOXSCREEN_H
#define __BOXSCREEN_H

//Box2D shape drawing helpers
// translates from Box2D coordinates to screen pixel coordinates
#include <box2d/box2d.h>

#include "boxTypes.h"
#include "Display.h"

class BoxScreen {
    public:
	// Box origin defaults to bottom center of the display
	BoxScreen (Display *d, float _pM = 10.0f);

	void drawBody (BodyId b);

	void drawGrid ();

	void worldLine (const Vec2 &p1B, const Vec2 &p2B, Color c);

	void keepViewable (const Vec2 &pW);

	inline void box2pixel (const Vec2 &boxV, Vec2 &screenV) const;

	//Pixel border kept around a followed point
	static const int SideBorder = 300, TopBottomBorder = 32;
    private:
	float pM;		//pixels/meter
	Vec2 BoxOriginP;	//location of box origin in pixel space

	void drawShape (BodyId b, b2ShapeId s);

	Display *display;
};

inline void BoxScreen::box2pixel (const Vec2 &boxV, Vec2 &screenV) const {
    screenV.x = BoxOriginP.x + pM*boxV.x;
    screenV.y = BoxOriginP.y - pM*boxV.y;
}

#endif
