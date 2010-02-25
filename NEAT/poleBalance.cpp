#include <iostream>
#include <stdlib.h>
#include <stdio.h>

#include "random.h"
#include "Network.h"
#include "Genome.h"
#include "GeneticAlgorithm.h"

#include <SDL/SDL.h>
#include <SDL/SDL_gfxPrimitives.h>
#include <SDL/SDL_framerate.h>

using namespace std;

const int Width  = 640;
const int Height = 320;

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
class poleBalance : public unary_function<const GenomeP, double> {
    public:
	const int MAX_STEPS;
	const bool random_start;
	const float TAU;   /* seconds between state updates */


	poleBalance(int max_steps, float _tau = 0.02,
		    bool _random_start = true) :
	    MAX_STEPS(max_steps), TAU(_tau), random_start(_random_start) 
	{ }

	double operator()(const GenomeP &g, SDL_Surface *screen = NULL) {
	    auto_ptr<Network> N(g->createNewNetwork());
	
	   float x,			/* cart position, meters */
		 x_dot,			/* cart velocity */
		 theta,			/* pole angle, radians */
		 theta_dot;		/* pole angular velocity */
	   int steps=0,y;
	    
	#ifdef ALLIN
	   double in[5];  //Input loading array
	#else
	   double in[3];
	#endif
	   double out[2] = {0,0}; //Output, L or R

	   double one_degree= 0.0174532;	/* 2pi/360 */
	   double six_degrees=0.1047192;
	   double twelve_degrees=0.2094384;
	   double thirty_six_degrees= 0.628329;
	   double fifty_degrees=0.87266;

	   if (random_start) {
	     /*set up random start state*/
	     x = rand_double()*4.8 - 2.4;
	     x_dot = rand_double()*2.0 - 1;
	     theta = rand_double()*0.4 - .2;
	     theta_dot = rand_double()*3.0 - 1.5;
	    }
	   else 
	     x = x_dot = theta = theta_dot = 0.0;
	     
	     
	   FPSmanager fpsm;

	   if (screen) {
	     int rate = static_cast<int>(2.0/TAU);
	     SDL_initFramerate(&fpsm);
	     SDL_setFramerate(&fpsm,rate);
	   }
	   
	   /*--- Iterate through the action-learn loop. ---*/
	   while (steps++ < MAX_STEPS)
	     {
	       
	       /*-- setup the input layer based on the four iputs --*/
	       in[0]=1.0;  //Bias
	       
	    #ifdef ALLIN
	       in[1]=(x + 2.4) / 4.8;;
	       in[2]=(x_dot + .75) / 1.5;
	       in[3]=(theta + twelve_degrees) / .41;
	       in[4]=(theta_dot + 1.0) / 2.0;
	    #else
		//Just X position and angle, no velocity
	       in[1]=(x + 2.4) / 4.8;;
	       in[2]=(theta + twelve_degrees) / .41;
	    #endif

	       //Run input through network
	       N->run(in, out);

	       //Did the network decide left or right?
	       if (out[0] > out[1])
		 y = 0;
	       else
		 y = 1;
	       
	       /*--- Apply action to the simulated cart-pole ---*/
	       cart_pole(y, &x, &x_dot, &theta, &theta_dot);
	       
	       if (screen) {
		   ClearScreen(screen);
		   display_cart(x,theta,screen);
		   SDL_Flip(screen);

		   HandleEvent();

		   SDL_framerateDelay(&fpsm);
	       }

	       /*--- Check for failure.  If so, return steps ---*/
	       if (x < -2.4 || x > 2.4  || theta < -twelve_degrees ||
		   theta > twelve_degrees) 
		   break;
	     }

//	cout<<"Made it "<<steps<<" steps..."
//	    <<static_cast<double>(steps)/MAX_STEPS<<endl;

	    return (g->fitness = static_cast<double>(steps)/MAX_STEPS);
	};

    private:

	//     cart_and_pole() was take directly from the pole simulator written
	//     by Richard Sutton and Charles Anderson.
	//     This simulator uses normalized, continous inputs instead of 
	//    discretizing the input space.
	/*----------------------------------------------------------------------
	   cart_pole:  Takes an action (0 or 1) and the current values of the
	 four state variables and updates their values by estimating the state
	 TAU seconds later.
	----------------------------------------------------------------------*/
	void cart_pole(int action, float *x,float *x_dot, 
		      float *theta, float *theta_dot) 
	{
	  float xacc,thetaacc,force,costheta,sintheta,temp;
	  
	  const float GRAVITY=9.8;
	  const float MASSCART=1.0;
	  const float MASSPOLE=0.1;
	  const float TOTAL_MASS=(MASSPOLE + MASSCART);
	  const float LENGTH=0.5;	  /* actually half the pole's length */
	  const float POLEMASS_LENGTH=(MASSPOLE * LENGTH);
	  const float FORCE_MAG=10.0;
	  const float FOURTHIRDS=1.3333333333333;

	  force = (action>0)? FORCE_MAG : -FORCE_MAG;
	  costheta = cos(*theta);
	  sintheta = sin(*theta);
	  
	  temp = (force + POLEMASS_LENGTH * *theta_dot * *theta_dot * sintheta)
	    / TOTAL_MASS;
	  
	  thetaacc = (GRAVITY * sintheta - costheta* temp)
	    / (LENGTH * (FOURTHIRDS - MASSPOLE * costheta * costheta
			 / TOTAL_MASS));
	  
	  xacc  = temp - POLEMASS_LENGTH * thetaacc* costheta / TOTAL_MASS;
	  
	  /*** Update the four state variables, using Euler's method. ***/
	  
	  *x  += TAU * *x_dot;
	  *x_dot += TAU * xacc;
	  *theta += TAU * *theta_dot;
	  *theta_dot += TAU * thetaacc;

	};

	/**
	 * Draw display balance cart, 
	 *
	 *  Convert 640 to 320 into [-3.2 : 3.2], [0 : 3.2] for display
	 *  purposes.
	 */
	void display_cart(float x, float theta, SDL_Surface *screen) {
	    const int zeroX = Width>>1;
	    const int zeroY = Height-1;
    
	    int bx = x*100 + zeroX, by = zeroY;
	    int tx = bx + sin(theta)*100;
	    int ty = by - cos(theta)*100;

	    //cout<<bx<<", "<<by<<" - "<<tx<<", "<<ty<<endl;

	    lineColor(screen, bx, by, tx, ty, 0x00FF00FF);
	};

	void HandleEvent()
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
		printf("Usage: poleBalance [-V for video] [-h this]\n");
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

    poleBalance fit(100000, 0.01);
    
    GeneticAlgorithm<poleBalance> GA(&P, &fit);

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
	    fit(GA.bestIndiv(), screen);

	//cout<<"Generation "<<gen+1<<endl;
	//GA.printPopulation();
	
	//if (error == 0) break;
    }
    return 0;
}
