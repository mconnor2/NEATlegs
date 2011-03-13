#include <iostream>
#include <stdlib.h>
#include <stdio.h>

#include <boost/function.hpp>

#include <SDL/SDL.h>
#include <SDL/SDL_gfxPrimitives.h>
#include <SDL/SDL_framerate.h>
#include <SDL/SDL_ttf.h>

#include "NEAT/random.h"
#include "NEAT/Network.h"
#include "NEAT/Genome.h"
#include "NEAT/GeneticAlgorithm.h"

#include "BoxScreen.h"
#include "World.h"
#include "Creature.h"

using namespace std;

const int Width  = 640;
const int Height = 480;

const double Pi = 3.14159265358979323;

TTF_Font *fnt;

/**
 * Test for creature hopping.  Most crap is hard coded at this point.
 *
 */
class hopper : public unary_function<const GenomeP, double> {
    public:
	const int MAX_STEPS;
	//const bool random_start;

	const static double HEAD_FLOOR = 0.75;

	hopper(int max_steps, World *_W, CreatureP _C, ExpParameters *_P) :
	    MAX_STEPS(max_steps), C(_C), W(_W), P(_P) 
	{ }

	double operator()(const GenomeP &g, 
			  int Generation = 0, SDL_Surface *screen = NULL) {
	    auto_ptr<Network> N(g->createNewNetwork());
	
	    int steps=0,y;
	    
	    double *in = new double[P->nInput];  //Input loading array
	    //Output: thigh muscle length(%max), k; shin muscle length(%max), k
	    double *out = new double[P->nOutput];
	    int nMuscles = C->muscles.size();
	    
	    for (int i = 0; i<P->nOutput; ++i) out[i] = 0.0;

	    FPSmanager fpsm;

	    SDL_Surface *text_surf = NULL;

	    SDL_Rect text_loc;
	    text_loc.x = 10;
	    text_loc.y = 10;
	    
	    // 100 pixels a meter
	    BoxScreen s(screen, 100.0f);

	    if (screen) {
		int rate = 60; //static_cast<int>(2.0/TAU);
		SDL_initFramerate(&fpsm);
		SDL_setFramerate(&fpsm,rate);
    
		if (fnt) {
		    SDL_Color fgColor={255,255,255};

		    char num[16];
		    snprintf(num,16,"%d",Generation);
		    text_surf = TTF_RenderText_Blended(fnt,num,fgColor);
		}
	    }
	   
	    C->reset();
/*	    {
		shapePos headPos = C->shapes["head"];
	        Vec2 headV = headPos.b->GetWorldPoint(headPos.localPos);
		cout<<"Head position: "<<headV.x<<", "<<headV.y<<endl;
	    }
*/
	    double score = 0;
	    double maxX = 0, maxY = 0;

	    /*--- Iterate through the action-learn loop. ---*/
	    while (steps++ < MAX_STEPS) {
			
		/* Read input from Creature's Sensors */
		C->setInput(in);
/*
		if (screen) {
		    for (int i = 0; i<P->nInput; ++i) {
			cout<<in[i]<<" ";
		    }
		    cout<<endl;
		}
*/
		//Run input through network
		N->run(in, out);

		//Update creature's muscles based on output
		for (int m = 0; m<nMuscles; ++m) {
		    C->muscles[m]->scaleStrength(out[m*2]);
		    C->muscles[m]->scaleLength(out[m*2+1]);
		}

		/* Advance the world */
		W->step();
		
	        shapePos headPos = C->shapes["head"];
	        Vec2 headV = headPos.b->GetWorldPoint(headPos.localPos);

		if (screen) {
		    ClearScreen(screen);

		    if (text_surf) {
			SDL_BlitSurface(text_surf,NULL,screen,&text_loc);
		    }

//		    cout<<"Head height: "<<headV.x<<", "<<headV.y<<endl;
		    
		    s.keepViewable(headV);
		    s.drawGrid();
		    W->draw(&s);
		    SDL_Flip(screen);

		    if (HandleEvent()) break;

		    SDL_framerateDelay(&fpsm);
		}

		/*--- Check for failure.  If so, return steps ---*/
		// For hopper, failure is if creature's head drops below
		// some level.
		if (headV.y < HEAD_FLOOR) break;
		score += headV.y*headV.y;
		if (headV.x > maxX) maxX = headV.x;
		if (headV.y > maxY) maxY = headV.y;
	    }

	    if (text_surf)
		SDL_FreeSurface(text_surf);

	    delete [] in;
	    delete [] out;

//	cout<<"Made it "<<steps<<" steps..."
//	    <<static_cast<double>(steps)/MAX_STEPS<<endl;

	    //return (g->fitness = static_cast<double>(steps)/(MAX_STEPS+1));
	    //return (g->fitness = score/MAX_STEPS);
	    return (g->fitness = maxX);
	    //return (g->fitness = maxY);
	};

    private:
	CreatureP C;
	World *W;
	const ExpParameters *P;

	/**
	 * Draw display balance cart, 
	 *
	 *  Convert 640 to 320 into [-3.2 : 3.2], [0 : 3.2] for display
	 *  purposes.
	 */
/*
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

	};
*/

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
    char *configFile = NULL;
    while ((opt = getopt(argc, argv, "VC:h")) != -1) {
	switch(opt) {
	    case 'V':
		drawGen = true;
	    break;
	    case 'C':
		configFile = optarg;
	    break;
	    default:
		printf("Usage: hopper -C config file [-V for video] [-h this]\n");
		exit(1);
	}
    }
    
    if (!configFile) {
	fprintf(stderr, "Must specify config file.\n");
	exit(1);
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
	SDL_WM_SetCaption("Hopper Test", "Hopper Test");
    }

    ExpParameters P;
    
    if (!P.loadFromFile(configFile)) {
	cerr<<"Problem loading ExpParameters, exiting."<<endl;
	exit(1);
    }
   
    cout<<"Generating pop size of "<<P.popSize<<endl;

    /* Initialize the World, take default hz and iteration */
    World w(60.0f,10,10);

    // Create a creature that is added to the world
    CreatureP hoppy = w.createCreature(configFile);

    if (!hoppy) {
	cerr<<"Problem loading Creature, exiting."<<endl;
	exit(1);
    }
    
    P.nInput = hoppy->numSensors();
    P.nOutput = hoppy->muscles.size()*2;

    hopper fit(1000, &w, hoppy, &P);
    boost::function<double (const GenomeP)> f = fit;

    GeneticAlgorithm GA(&P, &f);

    double maxFit = -1e9, curMaxFit = 0;
    
    //cout<<"Generation 0"<<endl;
    //GA->printPopulation();

    cout<<"Initialized.  Starting simulation."<<endl;

    for (int gen = 0; gen < 1000; gen++) {
	//Each generation will receive a different input, so network
	// can't just memorize pattern
	//fit.regenerate();

	curMaxFit = GA.nextGeneration();
	if (curMaxFit > maxFit) maxFit = curMaxFit;
	cout<<"  After generation "<<gen<<", maximum fitness =  "<<maxFit<<endl;
	cout<<"========================================================="<<endl;

	if (gen%10 == 0 && drawGen)
	    fit(GA.bestIndiv(), gen, screen);

	//cout<<"Generation "<<gen+1<<endl;
	//GA.printPopulation();
	
	//if (error == 0) break;
    }
    return 0;
}
