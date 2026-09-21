#ifndef __DISPLAY_H
#define __DISPLAY_H

#include <cstdint>
#include <string>

struct SDL_Window;
struct SDL_Renderer;
struct TTF_Font;

// Colors are packed 0xRRGGBBAA (same convention SDL_gfx used)
typedef uint32_t Color;

enum class DisplayEvent { None, Space, Quit };

/**
 * SDL3 window + renderer wrapper with the handful of drawing primitives
 * the simulations need (lines, circles, points, text) and a frame rate
 * limiter.  Coordinates are in pixels, origin at top left.
 */
class Display {
    public:
	// Throws std::runtime_error if SDL or the window can't be set up
	Display (const char *title, int width, int height, int fps = 60);
	~Display ();

	Display (const Display &) = delete;
	Display &operator= (const Display &) = delete;

	int width () const { return w; }
	int height () const { return h; }

	void clear ();
	void present ();

	void line (float x1, float y1, float x2, float y2, Color c);
	void circle (float cx, float cy, float r, Color c);
	void point (float x, float y, Color c);
	void text (float x, float y, const std::string &s,
		   Color c = 0xFFFFFFFF);

	// Drain the event queue.  Space bar -> Space; window close or any
	// other key -> Quit.
	DisplayEvent poll ();

	// True once poll() has seen a Quit
	bool quitRequested () const { return quit; }

	// Sleep so that successive calls happen at most fps times a second
	void waitFrame ();

    private:
	void setColor (Color c);

	int w, h;
	SDL_Window *window = nullptr;
	SDL_Renderer *renderer = nullptr;
	TTF_Font *font = nullptr;

	uint64_t frameNS;
	uint64_t lastFrame = 0;
	bool quit = false;
};

#endif
