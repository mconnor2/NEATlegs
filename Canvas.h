#ifndef __CANVAS_H
#define __CANVAS_H

#include <string>

#include "Color.h"

/**
 * Something to draw on, in pixels (origin top left).  Display implements it
 * with SDL; tests use a recording canvas, so drawing code can be tested
 * without a window.
 */
class Canvas {
    public:
	virtual ~Canvas () { }

	virtual int width () const = 0;
	virtual int height () const = 0;

	virtual void line (float x1, float y1, float x2, float y2, Color c) = 0;
	virtual void circle (float cx, float cy, float r, Color c) = 0;
	virtual void point (float x, float y, Color c) = 0;
	virtual void text (float x, float y, const std::string &s,
			   Color c = 0xFFFFFFFF) = 0;
};

#endif
