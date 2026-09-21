#include "Display.h"

#include <cmath>
#include <iostream>
#include <stdexcept>

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#ifndef NEATLEGS_FONT_PATH
#define NEATLEGS_FONT_PATH "ProggyClean.ttf"
#endif

using namespace std;

Display::Display (const char *title, int width, int height, int fps) :
		  w(width), h(height), frameNS(1000000000ull / fps)
{
    if (!SDL_Init(SDL_INIT_VIDEO))
	throw runtime_error(string("Couldn't initialize SDL: ") +
			    SDL_GetError());

    if (!SDL_CreateWindowAndRenderer(title, w, h, 0, &window, &renderer))
	throw runtime_error(string("Couldn't create window: ") +
			    SDL_GetError());

    if (TTF_Init()) {
	font = TTF_OpenFont(NEATLEGS_FONT_PATH, 12);
	if (!font) font = TTF_OpenFont("ProggyClean.ttf", 12);
	if (!font) cerr<<"TTF_OpenFont: "<<SDL_GetError()<<endl;
    } else {
	cerr<<"TTF_Init: "<<SDL_GetError()<<endl;
    }
}

Display::~Display () {
    if (font) TTF_CloseFont(font);
    TTF_Quit();
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    SDL_Quit();
}

void Display::setColor (Color c) {
    SDL_SetRenderDrawColor(renderer, (c >> 24) & 0xFF, (c >> 16) & 0xFF,
			   (c >> 8) & 0xFF, c & 0xFF);
}

void Display::clear () {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
}

void Display::present () {
    SDL_RenderPresent(renderer);
}

void Display::line (float x1, float y1, float x2, float y2, Color c) {
    setColor(c);
    SDL_RenderLine(renderer, x1, y1, x2, y2);
}

void Display::circle (float cx, float cy, float r, Color c) {
    const int N = 32;
    SDL_FPoint pts[N + 1];
    for (int i = 0; i <= N; ++i) {
	float a = 2.0f * (float)M_PI * i / N;
	pts[i].x = cx + r * cosf(a);
	pts[i].y = cy + r * sinf(a);
    }
    setColor(c);
    SDL_RenderLines(renderer, pts, N + 1);
}

void Display::point (float x, float y, Color c) {
    setColor(c);
    SDL_RenderPoint(renderer, x, y);
}

void Display::text (float x, float y, const string &s) {
    if (!font || s.empty()) return;

    SDL_Color white = {255, 255, 255, 255};
    SDL_Surface *surf = TTF_RenderText_Blended(font, s.c_str(), s.size(),
					       white);
    if (!surf) return;

    SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surf);
    if (tex) {
	SDL_FRect dst = {x, y, (float)surf->w, (float)surf->h};
	SDL_RenderTexture(renderer, tex, NULL, &dst);
	SDL_DestroyTexture(tex);
    }
    SDL_DestroySurface(surf);
}

DisplayEvent Display::poll () {
    DisplayEvent result = DisplayEvent::None;
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
	switch (event.type) {
	    case SDL_EVENT_KEY_DOWN:
		if (event.key.key == SDLK_SPACE) {
		    if (result == DisplayEvent::None)
			result = DisplayEvent::Space;
		    break;
		}
		return DisplayEvent::Quit;
	    case SDL_EVENT_QUIT:
		return DisplayEvent::Quit;
	}
    }
    return result;
}

void Display::waitFrame () {
    uint64_t now = SDL_GetTicksNS();
    if (lastFrame && now - lastFrame < frameNS)
	SDL_DelayNS(frameNS - (now - lastFrame));
    lastFrame = SDL_GetTicksNS();
}
