#include <iostream>
#include <stdlib.h>
#include <stdio.h>

#include <functional>
#include <memory>

#include "random.h"
#include "Network.h"
#include "Genome.h"
#include "GeneticAlgorithm.h"
#include "RunLog.h"

#include <unistd.h>

#include "Display.h"
#include "StatsOverlay.h"

#include <functional>

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
class poleBalance {
    public:
	const int MAX_STEPS;
	const bool random_start;
	const float TAU;   /* seconds between state updates */


	poleBalance(int max_steps, float _tau = 0.02,
		    bool _random_start = true) :
	    MAX_STEPS(max_steps), TAU(_tau), random_start(_random_start) 
	{ }

	double operator()(const GenomeP &g, 
			  Display *display = nullptr,
			  const function<void(Display &)> &overlay = nullptr) {
	    unique_ptr<Network> N(g->createNewNetwork());
	
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
	       
	       if (display) {
		    display->clear();
		    if (overlay) overlay(*display);

		    display_cart(steps,x,theta,display);
		    display->present();

		    if (display->poll() != DisplayEvent::None) exit(0);

		    display->waitFrame();
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
	  const float MASSPOLE=0.2;
	  const float TOTAL_MASS=(MASSPOLE + MASSCART);
	  const float LENGTH=1.0;	  /* actually half the pole's length */
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
	void display_cart(int steps, float x, float theta,
			  Display *display) const
	{
	    const int zeroX = Width>>1;
	    const int zeroY = Height-1;
    
	    int bx = x*100 + zeroX, by = zeroY;
	    int tx = bx + sin(theta)*200;
	    int ty = by - cos(theta)*200;

	    //Give a progress bar of sorts near the top
	    display->line(0,0, Width*((float)steps)/MAX_STEPS,0,0xFF0000FF);

	    //Display edges of the board
	    // Center
	    display->line(zeroX, zeroY, zeroX, zeroY-10, 0xFF0000FF);
	    // Edges
	    display->line(zeroX-240,zeroY, zeroX-240,zeroY-10, 0x0000FFFF);
	    display->line(zeroX+240,zeroY, zeroX+240,zeroY-10, 0x0000FFFF);

	    //cout<<bx<<", "<<by<<" - "<<tx<<", "<<ty<<endl;

	    display->line(bx, by, tx, ty, 0x00FF00FF);
	};
};

int main (int argc, char **argv) {
    //set random seed to come from udev random
    dev_seed_rand();

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
    
    unique_ptr<Display> display;
    if (drawGen) {
	try {
	    //Draw at twice simulation speed (TAU = 0.01)
	    display.reset(new Display("Single Pole Balance", Width, Height,
				      200));
	} catch (exception &e) {
	    cerr<<e.what()<<endl;
	    return 1;
	}
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

    poleBalance fit(10000, 0.01);
    FitnessFunction f = fit;

    GeneticAlgorithm GA(&P, &f);

    RunLog log((RunLog::Options()));

    for (int gen = 0; gen < 1000; gen++) {
	//Each generation will receive a different input, so network
	// can't just memorize pattern
	//fit.regenerate();

	GA.nextGeneration();
	log.record(GA);

	if (drawGen) {
	    string title = "Generation " + to_string(gen) + " best";
	    fit(GA.bestIndiv(), display.get(), [&](Display &d) {
		drawStatsOverlay(d, title, GA.history());
	    });
	}

	//cout<<"Generation "<<gen+1<<endl;
	//GA.printPopulation();
	
	//if (error == 0) break;
    }
    log.finish(GA);
    return 0;
}
