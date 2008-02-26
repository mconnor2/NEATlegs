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
	
void BoxScreen::drawBody (Body *b) {
    for (Shape *s = b->GetShapeList(); s; s = s->m_next) {
	drawShape ( s);
    }
}

void BoxScreen::drawShape (const Shape *s) {
    switch(s->GetType()) {
	case e_circleShape:
	{
	    const b2CircleShape* circle = (const b2CircleShape*)s;
	    b2Vec2 xP;
	    box2pixel(circle->m_position, xP);
	    float32 rP = pM * circle->m_radius;
	    //Draw a red circle with SDL_gfx
	    circleColor(screen, (int)xP.x, (int)xP.y, (int)rP, 0xFF0000FF);
//	    cout<<"CIRCLE: "<<xP.x<<", "<<xP.y<<" radius: "<<rP<<endl;
	}
	break;
	case e_boxShape:
	case e_polyShape:
	{
	  const b2PolyShape* poly = (const b2PolyShape*)s;
          
	  if (poly->m_vertexCount > 1) {
	      //glBegin(GL_LINE_LOOP);
	      b2Vec2 v1P, v2P;
	      b2Vec2 firstP, v = poly->m_position + 
			         b2Mul(poly->m_R, poly->m_vertices[0]);
	      box2pixel(v,firstP);
	      v1P = firstP;
//	      cout<<"POLYGON: ";
	      for (int32 i = 1; i < poly->m_vertexCount; ++i)
	      {
//		  cout<<"("<<v1P.x<<", "<<v1P.y<<") ";
		  v = poly->m_position + 
		      b2Mul(poly->m_R, poly->m_vertices[i]);
		  box2pixel(v,v2P);

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
	case e_meshShape:
	    cerr<<"drawShape: Don't know how to draw this shape yet!"<<endl;
	break;
	case e_unknownShape:
	default:
	    cerr<<"drawShape: unknown shape."<<endl;
    }
}
	
void BoxScreen::worldLine (const Vec2 &p1B, const Vec2 &p2B, Color c) {
    Vec2 p1P, p2P;
    box2pixel(p1B,p1P); box2pixel(p2B,p2P);
    lineColor(screen, (int)p1P.x, (int)p1P.y, (int)p2P.x, (int)p2P.y, c);
}
