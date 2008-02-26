#ifndef __BOXSCREEN_H
#define __BOXSCREEN_H

//Box2D shape drawing helpers
// translates from Box2D coordinates to SDL pixel coordinates
#include <Box2D.h>
#include <SDL/SDL.h>

typedef b2Body Body;
typedef b2Shape Shape;
typedef b2Vec2 Vec2;

typedef Uint32 Color;

class BoxScreen {
    public:
	BoxScreen (SDL_Surface *s, float _pM = 10.0f,
		   float BoxOriginX = 320.0f, float BoxOriginY = 479.0f);

	void drawBody (Body *b);
	void drawShape (const Shape *s);

	void worldLine (const Vec2 &p1B, const Vec2 &p2B, Color c);

	inline void box2pixel (const Vec2 &boxV, Vec2 &screenV);
    private:
	float pM;		//pixels/meter
	Vec2 BoxOriginP;	//location of box origin in pixel space

	SDL_Surface *screen;
};

inline void BoxScreen::box2pixel (const Vec2 &boxV, Vec2 &screenV) {
    screenV.x = BoxOriginP.x + pM*boxV.x;
    screenV.y = BoxOriginP.y - pM*boxV.y;
}

#endif
