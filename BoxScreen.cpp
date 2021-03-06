#include "BoxScreen.h"

#include <iostream>
#include <SDL/SDL_gfxPrimitives.h>

using namespace std;



BoxScreen::BoxScreen (SDL_Surface *s, float _pM,
		      float BoxOriginX, float BoxOriginY) :
		    pM(_pM), BoxOriginP(BoxOriginX,BoxOriginY),
		    screen(s)
{

}

/**
 * Make sure that the world point passed is viewable in screen space.
 * If not, shift origin accordingly.
 */
void BoxScreen::keepViewable (const Vec2 &pW) {
    b2Vec2 xP;
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
	
void BoxScreen::drawBody (const BodyP &b) {
    for (Fixture *f = b->GetFixtureList(); f; f = f->GetNext()) {
	drawShape (b,f->GetShape());
    }
}

void BoxScreen::drawShape (const BodyP &b, const Shape *s) {

    switch(s->GetType()) {
	case b2Shape::e_circle:
	{
	    const b2CircleShape* circle = dynamic_cast<const b2CircleShape*>(s);
	    b2Vec2 xP;
	    box2pixel(b->GetWorldPoint(circle->m_p), xP);
	    float32 rP = pM * circle->m_radius;
	    //Draw a red circle with SDL_gfx
	    circleColor(screen, (int)xP.x, (int)xP.y, (int)rP, 0xFF0000FF);
//	    cout<<"CIRCLE: "<<xP.x<<", "<<xP.y<<" radius: "<<rP<<endl;
	}
	break;
	case b2Shape::e_polygon:
	{
	  const b2PolygonShape* poly = dynamic_cast<const b2PolygonShape*>(s);
          
	  if (poly->GetVertexCount() > 1) {
	      //glBegin(GL_LINE_LOOP);

	      b2Vec2 v1P, v2P;
	      b2Vec2 firstP;
	      box2pixel(b->GetWorldPoint(poly->GetVertex(0)),firstP);
	      v1P = firstP;
//	      cout<<"POLYGON: ";
	      for (int32 i = 1; i < poly->GetVertexCount(); ++i)
	      {
//		  cout<<"("<<v1P.x<<", "<<v1P.y<<") ";
		  box2pixel(b->GetWorldPoint(poly->GetVertex(i)),v2P);

		  //Draw a green polygon with SDL_gfx
		  lineColor(screen, 
			    (int)v1P.x, (int)v1P.y,
			    (int)v2P.x, (int)v2P.y,
			    0x00FF00FF);
		  v1P = v2P;
	      }
//	      cout<<endl;
	      //Complete the loop
	      lineColor(screen, 
			(int)v1P.x, (int)v1P.y,
			(int)firstP.x, (int)firstP.y,
			0x00FF00FF);
	      //glEnd();
	  }
	}
	break;
	
	case b2Shape::e_unknown:
	default:
	    cerr<<"drawShape: unknown shape."<<endl;
    }
}
	
void BoxScreen::worldLine (const Vec2 &p1B, const Vec2 &p2B, Color c) {
    Vec2 p1P, p2P;
    box2pixel(p1B,p1P); box2pixel(p2B,p2P);
    lineColor(screen, (int)p1P.x, (int)p1P.y, (int)p2P.x, (int)p2P.y, c);
}

/**
 * Draw a grid of pixels, one every 10 meters starting from the origin.
 *
 * From the origin point, need to figure out where the minimum x,y coordinates
 *  are that are in the screen space, then draw from there.
 *
 *  min_c o.x + c*10*pM >= 0
 *        c >= -o.x / 10*pM
 *        c = ceil(-o.x / 10*pM)
 */
void BoxScreen::drawGrid () {
    float sx = BoxOriginP.x - floor((float)BoxOriginP.x / (1.0f * pM))*1.0f*pM,
	  sy = BoxOriginP.y - floor((float)BoxOriginP.y / (1.0f * pM))*1.0f*pM;

//    cout<<"Starting grid: "<<(float)sx<<", "<<(float)sy
//	<<" with "<<10*pM<<" pixels / 10 meters"<<endl;

    for (float y = sy; y<HEIGHT; y+=1.0f*pM) {
	for (float x = sx; x<WIDTH; x+=1.0f*pM) {
	    pixelColor(screen, (int)x-1, (int)y, 0x00888888);
	    pixelColor(screen, (int)x+1, (int)y, 0x00888888);
	    pixelColor(screen, (int)x, (int)y-1, 0x00888888);
	    pixelColor(screen, (int)x, (int)y+1, 0x00888888);
	    pixelColor(screen, (int)x, (int)y, 0x00888888);
	}
    }
}
