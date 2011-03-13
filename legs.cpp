#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>

#include <vector>
//#include <ext/slist>

#include <SDL/SDL.h>
#include <SDL/SDL_gfxPrimitives.h>
#include <SDL/SDL_framerate.h>

#include "BoxScreen.h"
#include "World.h"
#include "Creature.h"

using namespace std;

const int Width = 640;
const int Height = 480;

const double Pi = 3.14159265358979323;

int HandleEvent()
{
    SDL_Event event; 
    int status = 0;

    /* Check for events */
    while ( SDL_PollEvent(&event) ) {
	switch (event.type) {
	    case SDL_KEYDOWN:
		switch(event.key.keysym.sym){
		    case SDLK_SPACE:
			return 1;
			break;
		}
	    case SDL_QUIT:
                exit(0);
                break;
	}
    }
    return status;
}

void ClearScreen(SDL_Surface *screen)
{
	int i;
	/* Set the screen to black */
	if ( SDL_LockSurface(screen) == 0 ) {
		Uint32 black;
		Uint8 *pixels;
		black = SDL_MapRGB(screen->format, 0, 0, 0);
		pixels = (Uint8 *)screen->pixels;
		for ( i=0; i<screen->h; ++i ) {
			memset(pixels, black,
				screen->w*screen->format->BytesPerPixel);
			pixels += screen->pitch;
		}
		SDL_UnlockSurface(screen);
	}
}

void runSimulation (SDL_Surface *screen, World *world, Creature *C) {
    FPSmanager fpsm;

    BoxScreen s(screen);

    int rate = 120;
    SDL_initFramerate(&fpsm);
    SDL_setFramerate(&fpsm,rate);

    int nFrames = 100;
    long frames = 0;
    Uint32 ticks = SDL_GetTicks(), nt;
    double sec;
    while (1) {
	ClearScreen(screen);
	
	//Draw and update the world
	world->draw(&s);
	world->step();

	//Check for exit
	if (HandleEvent()) {
	    C->reset();
	}

	//Flip
	SDL_Flip(screen);

	++frames;
	if (frames%nFrames == 0) {
	    nt = SDL_GetTicks();
	    sec = (nt-ticks)/1000.0;
	    printf("%d frames in %0.4f seconds, %0.4f fps\n", 
		    nFrames, sec, nFrames/sec);
	    ticks = nt;
	}

	SDL_framerateDelay(&fpsm);  
    }
}

void exitingfunc () {
   SDL_Quit();
}

int main (int argc, char **argv) {
    srand(time(NULL));

    /* Process arguments */
    int opt;
    char *configFile = NULL;
    while ((opt = getopt(argc, argv, "C:N:h")) != -1) {
	switch(opt) {
	    case 'N':
	    break;
	    case 'C':
		configFile = optarg;
	    break;
	    default:
		printf("Usage: legs [-N ?] [-h this?]\n");
		exit(1);
	}
    }
    
    if (!configFile) {
	fprintf(stderr, "Must specify config file.\n");
	exit(1);
    }

    SDL_Surface *screen;

    /* Initialize SDL */
    if ( SDL_Init(SDL_INIT_VIDEO) < 0 ) {
	fprintf(stderr,
		"Couldn't initialize SDL: %s\n", SDL_GetError());
	exit(1);
    }
    atexit(exitingfunc);
	
    Uint32 video_flags;

    int desired_bpp = 32;
    video_flags = SDL_HWSURFACE | SDL_DOUBLEBUF;

    /* Initialize the display */
    screen = SDL_SetVideoMode(Width, Height, desired_bpp, video_flags);
    if ( screen == NULL ) {
	fprintf(stderr, "Couldn't set %dx%dx%d video mode: %s\n",
				Width, Height, desired_bpp, SDL_GetError());
    	exit(1);
    }

    /* Show some info */
    printf("Set %dx%dx%d mode\n",
	    screen->w, screen->h, screen->format->BitsPerPixel);
    printf("Video surface located in %s memory.\n",
	   (screen->flags&SDL_HWSURFACE) ? "video" : "system");

    /* Check for double buffering */
    if ( screen->flags & SDL_DOUBLEBUF ) {
    	printf("Double-buffering enabled - good!\n");
    }

    /* Set the window manager title bar */
    SDL_WM_SetCaption("Walkabout!", "Legs!");

    /* Initialize the World, take default hz and iteration */
    World w(60.0f,10,10);

    // Create a creature that is added to the world
    Creature walker;

    if (!walker.initFromFile(configFile, &w)) {
	fprintf(stderr, "Couldn't init creature from file, exiting.\n");
	exit(1);
    }

    runSimulation(screen, &w, &walker);
}
