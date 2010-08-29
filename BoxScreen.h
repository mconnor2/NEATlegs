#ifndef __BOXSCREEN_H
#define __BOXSCREEN_H

//Box2D shape drawing helpers
// translates from Box2D coordinates to SDL pixel coordinates
#include <Box2D.h>
#include <SDL/SDL.h>

#include "boxTypes.h"

typedef Uint32 Color;

#define WIDTH 640
#define HEIGHT 480

class BoxScreen {
    public:
	BoxScreen (SDL_Surface *s, float _pM = 10.0f,
		   float BoxOriginX = WIDTH/2.0f, float BoxOriginY = HEIGHT - 1.0f);

	void drawBody (const BodyP &b);

	void drawGrid ();

	void worldLine (const Vec2 &p1B, const Vec2 &p2B, Color c);

	void keepViewable (const Vec2 &pW);

	inline void box2pixel (const Vec2 &boxV, Vec2 &screenV);

	//Default pixel border for 640x480:
	static const int LeftBorder = 32, RightBorder = WIDTH-32,
			 TopBorder = 32, BottomBorder = HEIGHT-32;
    private:
	float pM;		//pixels/meter
	Vec2 BoxOriginP;	//location of box origin in pixel space
	
	void drawShape (const BodyP &b, const Shape *s);

	SDL_Surface *screen;
};

inline void BoxScreen::box2pixel (const Vec2 &boxV, Vec2 &screenV) {
    screenV.x = BoxOriginP.x + pM*boxV.x;
    screenV.y = BoxOriginP.y - pM*boxV.y;
}

#endif
