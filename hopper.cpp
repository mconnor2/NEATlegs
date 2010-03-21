#include <iostream>
#include <stdlib.h>
#include <stdio.h>

#include <SDL/SDL.h>
#include <SDL/SDL_gfxPrimitives.h>
#include <SDL/SDL_framerate.h>
#include <SDL/SDL_ttf.h>

#include "NEAT/random.h"
#include "NEAT/Network.h"
#include "NEAT/Genome.h"
#include "NEAT/GeneticAlgorithm.h"

using namespace std;

const int Width  = 640;
const int Height = 320;

TTF_Font *fnt;

/**
 * Single Pole balancing experiment.  Physics test of NEAT to evolve
 * motion controller.  Code taken from Ken Stanley's original NEAT
 * implimentation, and he states:
//     cart_and_pole() was take directly from the pole simulator written
//     by Richard Sutton and Charles Anderson.
 *
 * Also use as a test for saving and displaying network behaviour.
 *
 */
class hopper : public unary_function<const GenomeP, double> {
    public:
	const int MAX_STEPS;
	const bool random_start;

	hopper(int max_steps, World *_W, Creature *_C) :
	    MAX_STEPS(max_steps), W(_W), random_start(_random_start) 
	{ }

	double operator()(const GenomeP &g, 
			  int Generation = 0, SDL_Surface *screen = NULL) {
	   auto_ptr<Network> N(g->createNewNetwork());
	
	   int steps=0,y;
	    
	   double in[5];  //Input loading array
	   double out[2] = {0,0}; //Output, L or R

	   FPSmanager fpsm;

	   SDL_Surface *text_surf = NULL;

	   SDL_Rect text_loc;
	   text_loc.x = 10;
	   text_loc.y = 10;

	   if (screen) {
	     int rate = static_cast<int>(2.0/TAU);
	     SDL_initFramerate(&fpsm);
	     SDL_setFramerate(&fpsm,rate);
    
	     BoxScreen s(screen);

	     if (fnt) {
		SDL_Color fgColor={255,255,255};

		char num[16];
		snprintf(num,16,"%d",Generation);
		text_surf = TTF_RenderText_Blended(fnt,num,fgColor);
	     }
	   }
	   
	   /*--- Iterate through the action-learn loop. ---*/
	   while (steps++ < MAX_STEPS)
	     {
	       
	       /*-- setup the input layer based on the four iputs --*/
	       in[0]=1.0;  //Bias
	       
	       //Get input from the creature
	       in[1]=(x + 2.4) / 4.8;;
	       in[2]=(x_dot + .75) / 1.5;
	       in[3]=(theta + twelve_degrees) / .41;
	       in[4]=(theta_dot + 1.0) / 2.0;

	       //Run input through network
	       N->run(in, out);

	       //Update creature's muscles

	       /* Advance the world */
	       W->step();

	       if (screen) {
		    ClearScreen(screen);

		    if (text_surf) {
			SDL_BlitSurface(text_surf,NULL,screen,&text_loc);
		    }

		    W->draw(s); 
		    SDL_Flip(screen);

		    if (HandleEvent()) break;

		    SDL_framerateDelay(&fpsm);
	       }

	       /*--- Check for failure.  If so, return steps ---*/
	       // For hopper, failure is if creature's head drops below
	       // some level.

	     }

	     if (text_surf)
		 SDL_FreeSurface(text_surf);

//	cout<<"Made it "<<steps<<" steps..."
//	    <<static_cast<double>(steps)/MAX_STEPS<<endl;

	    return (g->fitness = static_cast<double>(steps)/(MAX_STEPS+1));
	};

    private:
	Creature *C;
	World *W;

	/**
	 * Draw display balance cart, 
	 *
	 *  Convert 640 to 320 into [-3.2 : 3.2], [0 : 3.2] for display
	 *  purposes.
	 */
	void display_cart(int steps, float x, float theta, SDL_Surface *screen)
	{
	    const int zeroX = Width>>1;
	    const int zeroY = Height-1;
    
	    int bx = x*100 + zeroX, by = zeroY;
	    int tx = bx + sin(theta)*200;
	    int ty = by - cos(theta)*200;

	    //Give a progress bar of sorts near the top
	    lineColor(screen, 0,0, Width*((float)steps)/MAX_STEPS,0,0xFF0000FF);

	    //Display edges of the board
	    // Center
	    lineColor(screen, zeroX, zeroY, zeroX, zeroY-10, 0xFF0000FF);
	    // Edges
	    lineColor(screen, zeroX-240,zeroY, zeroX-240,zeroY-10, 0x0000FFFF);
	    lineColor(screen, zeroX+240,zeroY, zeroX+240,zeroY-10, 0x0000FFFF);

	    //cout<<bx<<", "<<by<<" - "<<tx<<", "<<ty<<endl;

	    lineColor(screen, bx, by, tx, ty, 0x00FF00FF);
	};

	bool HandleEvent()
	{
	    SDL_Event event; 

	    /* Check for events */
	    while ( SDL_PollEvent(&event) ) {
		switch (event.type) {
		    case SDL_KEYDOWN:
		    case SDL_QUIT:
			exit(0);
			break;
		}
	    }
	    return false;
	}


	void ClearScreen(SDL_Surface *screen)
	{
	    /* Set the screen to black */
	    if ( SDL_LockSurface(screen) == 0 ) {
		Uint32 black;
		Uint8 *pixels;
		black = SDL_MapRGB(screen->format, 0, 0, 0);
		pixels = (Uint8 *)screen->pixels;
		for (int i=0; i<screen->h; ++i ) {
		    memset(pixels, black,
			   screen->w*screen->format->BytesPerPixel);
		    pixels += screen->pitch;
		}
		SDL_UnlockSurface(screen);
	    }
	}
};

void exitingfunc () {
   TTF_Quit();
   SDL_Quit();
}

int main (int argc, char **argv) {
    //set random seed to come from udev random
    dev_seed_rand();

    SDL_Surface *screen = NULL;
    bool drawGen = false;

    /* Process arguments */
    int opt;
    while ((opt = getopt(argc, argv, "Vh")) != -1) {
	switch(opt) {
	    case 'V':
		drawGen = true;
	    break;
	    default:
		printf("Usage: hopper [-V for video] [-h this]\n");
		exit(1);
	}
    }
    
    if (drawGen) {

	/* Initialize SDL */
	if ( SDL_Init(SDL_INIT_VIDEO) < 0 ) {
	    fprintf(stderr,
		    "Couldn't initialize SDL: %s\n", SDL_GetError());
	    exit(1);
	}
	atexit(exitingfunc);
	    
	Uint32 video_flags;

	const int desired_bpp = 32;
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

	TTF_Init();

	//Hardcode font location
	fnt = TTF_OpenFont("ProggyClean.ttf",12);
	if (!fnt) {
	    printf("TTF_OpenFont: %s\n", TTF_GetError());
	    fnt = NULL;
	}

	/* Set the window manager title bar */
	SDL_WM_SetCaption("Single Pole Balance", "Single Pole Balance");
    }

    ExpParameters P;
    //Setup experiment parameters:
    // Keep population small so we can watch the results at first.
    P.popSize = 100;
    
    //5 inputs, 2 outputs that fight to determine left or right
    #ifdef ALLIN
    P.nInput = 5; P.nOutput = 2;
    #else
    P.nInput = 3; P.nOutput = 2;
    #endif

    //Mating probabilities:
    P.inheritAllLinks = false;
    P.inheritDominant = 0.9;
    P.linkEnabledRate = 0.1;

    P.weightMutationRate   = 0.2;
    P.weightPerturbScale   = 0.1;
    P.weightPerturbNormal  = 0.6;
    P.weightPerturbUniform = 0.39;
    
    P.addLinkMutationRate = 0.3;
    P.addNodeMutationRate = 0.01;
    
    P.compatGDiff = 1.0;
    P.compatWDiff = 0.4;
    
    P.compatThresh = 3;
    P.specieMate = 0.99;

    P.oldAge = 5;
    
    /* Initialize the World, take default hz and iteration */
    World w;

    // Create a creature that is added to the world
    Creature hoppy(&w);

    hopper fit(10000, &w, &hoppy);
    
    GeneticAlgorithm<hopper> GA(&P, &fit);

    double maxFit = -1e9, curMaxFit = 0;
    
    //cout<<"Generation 0"<<endl;
    //GA->printPopulation();

    for (int gen = 0; gen < 1000; gen++) {
	//Each generation will receive a different input, so network
	// can't just memorize pattern
	//fit.regenerate();

	curMaxFit = GA.nextGeneration();
	if (curMaxFit > maxFit) maxFit = curMaxFit;
	cout<<"  After generation "<<gen<<", maximum fitness =  "<<maxFit<<endl;
	cout<<"========================================================="<<endl;

	if (drawGen)
	    fit(GA.bestIndiv(), gen, screen);

	//cout<<"Generation "<<gen+1<<endl;
	//GA.printPopulation();
	
	//if (error == 0) break;
    }
    return 0;
}
