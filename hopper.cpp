#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <functional>
#include <memory>

#include <libconfig.h++>


#include "NEAT/random.h"
#include "NEAT/Network.h"
#include "NEAT/Genome.h"
#include "NEAT/GeneticAlgorithm.h"

#include "Display.h"
#include "BoxScreen.h"
#include "World.h"
#include "Creature.h"

using namespace std;

const int Width  = 640;
const int Height = 480;

/**
 * Test for creature hopping.  Most crap is hard coded at this point.
 *
 */
class hopper {
    public:
	hopper(const int max_steps, const libconfig::Config *_config, 
	       const ExpParameters *_P) :
	    MAX_STEPS(max_steps), config(_config), P(_P) 
	{ }

	double operator()(const GenomeP &g, 
			  int Generation = 0, Display *display = nullptr) const {
	    unique_ptr<Network> N(g->createNewNetwork());
	
	    int steps=0;

	    /* Initialize the World, take default hz and substeps */
	    World w(60.0f);

	    // Create a creature that is added to the world
	    CreatureP C = w.createCreature(*config);

	    if (!C) {
		cerr<<"Problem loading Creature, exiting."<<endl;
		exit(1);
	    }
		    
	    double *in = new double[P->nInput];  //Input loading array
	    //Output: thigh muscle length(%max), k; shin muscle length(%max), k
	    double *out = new double[P->nOutput];
	    int nMuscles = C->muscles.size();
	    
	    for (int i = 0; i<P->nOutput; ++i) out[i] = 0.0;

	    // 100 pixels a meter
	    BoxScreen s(display, 100.0f);

	    string genLabel = to_string(Generation);

	    if (!C->shapes.count("head")) {
		cerr<<"Creature must define a shape named 'head', exiting."
		    <<endl;
		exit(1);
	    }
	    const shapePos headPos = C->shapes["head"];
	   
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
		w.step();
		
	        Vec2 headV = b2Body_GetWorldPoint(headPos.b, headPos.localPos);

		if (display) {
		    display->clear();
		    display->text(10, 10, genLabel);

//		    cout<<"Head height: "<<headV.x<<", "<<headV.y<<endl;
		    
		    s.keepViewable(headV);
		    s.drawGrid();
		    w.draw(&s);
		    display->present();

		    if (display->poll() != DisplayEvent::None) exit(0);

		    display->waitFrame();
		}

		/*--- Check for failure.  If so, return steps ---*/
		// For hopper, failure is if creature's head drops below
		// some level.
		if (headV.y < HEAD_FLOOR) break;
		score += headV.y*headV.y;
		if (headV.x > maxX) maxX = headV.x;
		if (headV.y > maxY) maxY = headV.y;
	    }

	    delete [] in;
	    delete [] out;

//	cout<<"Made it "<<steps<<" steps..."
//	    <<static_cast<double>(steps)/MAX_STEPS<<endl;
  
#ifdef PROFILE
	    g->steps = steps;
#endif

	    //return (g->fitness = static_cast<double>(steps)/(MAX_STEPS+1));
	    //return (g->fitness = score/MAX_STEPS);
	    return (g->fitness = maxX);
	    //return (g->fitness = maxY);
	};

    private:
	const int MAX_STEPS;
	//const bool random_start;

	static constexpr double HEAD_FLOOR = 0.75;

	const libconfig::Config *config;
	const ExpParameters *P;
};

int main (int argc, char **argv) {
    //set random seed to come from udev random
    dev_seed_rand();

    bool drawGen = false;

    int maxGen = 1000;

    /* Process arguments */
    int opt;
    char *configFile = NULL;
    while ((opt = getopt(argc, argv, "VC:hN:")) != -1) {
	switch(opt) {
	    case 'V':
		drawGen = true;
	    break;
	    case 'C':
		configFile = optarg;
	    break;
	    case 'N':
		maxGen = atoi(optarg);
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

    libconfig::Config config;
    try {
	config.readFile(configFile);
    } catch (libconfig::ParseException &pe) {
	cerr<<"Config parse error"<<endl;
	cerr<<"   config file "<<configFile<<endl;
	cerr<<"   line number "<<pe.getLine()<<endl;
	cerr<<"   error: "<<pe.getError()<<endl;
	return 1;
    } catch (...) {
	cerr<<"Config error reading from file."<<endl;
	return 1;
    }
    config.setAutoConvert(true);

    ExpParameters P;
    
    if (!P.loadFromFile(config)) {
	cerr<<"Problem loading ExpParameters, exiting."<<endl;
	exit(1);
    }
   
    cout<<"Generating pop size of "<<P.popSize<<endl;
   
    try {
	P.nInput = config.lookup("sensors").getLength()+1;
	P.nOutput = config.lookup("muscles").getLength()*2;
    } catch (...) {
	cerr<<"Must specify sensors and muscles list in config file"<<endl;
	return 1;
    }

    hopper fit(1000, &config, &P);
    FitnessFunction f = fit;

    unique_ptr<Display> display;
    if (drawGen) {
	try {
	    display.reset(new Display("Hopper Test", Width, Height));
	} catch (exception &e) {
	    cerr<<e.what()<<endl;
	    return 1;
	}
    }
    
//    cout<<"Initial population size "<<P.startPopulationPercent
//	<<" * "<<P.popSize<<endl;

    GeneticAlgorithm GA(&P, &f);

    double maxFit = -1e9, curMaxFit = 0;
    
    //cout<<"Generation 0"<<endl;
    //GA->printPopulation();

    cout<<"Initialized.  Starting simulation."<<endl;

    for (int gen = 0; gen < maxGen; gen++) {
	//Each generation will receive a different input, so network
	// can't just memorize pattern
	//fit.regenerate();

	curMaxFit = GA.nextGeneration();
	if (curMaxFit > maxFit) maxFit = curMaxFit;
	cout<<"  After generation "<<gen<<", maximum fitness =  "<<maxFit<<endl;
	cout<<"========================================================="<<endl;

	if (gen%10 == 0 && drawGen)
	    fit(GA.bestIndiv(), gen, display.get());

	//cout<<"Generation "<<gen+1<<endl;
	//GA.printPopulation();
	
	//if (error == 0) break;
    }
    return 0;
}
