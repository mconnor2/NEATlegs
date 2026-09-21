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
 * Test for creature creation and simulation.  Is it deterministic? 
 *
 */
class hopper {
    public:
	const int MAX_STEPS;
	//const bool random_start;

	static constexpr double HEAD_FLOOR = 0.75;

	hopper(int max_steps, libconfig::Config *_config, ExpParameters *_P) :
	    MAX_STEPS(max_steps), config(_config), P(_P) 
	{ }

	double operator()(const GenomeP &g, 
			  int Generation = 0, Display *display = nullptr) const {
	    unique_ptr<Network> N(g->createNewNetwork());
	
	    int steps=0;
	    
	    // 100 pixels a meter
	    BoxScreen s(display, 100.0f);

	    string genLabel = to_string(Generation);

	    /* Initialize the World, extra substeps for accuracy */
	    World w(60.0f, 8);

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
	    
	    for (int i = 0; i<P->nInput; ++i) in[i] = 0.0;
	    for (int i = 0; i<P->nOutput; ++i) out[i] = 0.0;

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

	    C->setInput(in);
	    cout<<"Initialized test..."<<endl;
	    cout<<"in ->";
	    for (int i = 0; i<P->nInput; ++i) {
		cout<<" "<<in[i];
	    }
	    cout<<endl;
	    N->run(in, out);
	    cout<<"out ->";
	    for (int i = 0; i<P->nOutput; ++i) {
		cout<<" "<<out[i];
	    }
	    cout<<endl;

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
	   
		if (steps <= 2) {
		    cout<<"STEP "<<steps<<endl;
		    cout<<"  in ->";
		    for (int i = 0; i<P->nInput; ++i) {
			cout<<" "<<in[i];
		    }
		    cout<<endl;
		    cout<<"  out ->";
		    for (int i = 0; i<P->nOutput; ++i) {
			cout<<" "<<out[i];
		    }
		    cout<<endl;
		}

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

	    //return (g->fitness = static_cast<double>(steps)/(MAX_STEPS+1));
	    //return (g->fitness = score/MAX_STEPS);
	    return (g->fitness = maxX);
	    //return (g->fitness = maxY);
	};

    private:
	libconfig::Config *config;
	const ExpParameters *P;
};

int main (int argc, char **argv) {
    //set random seed to come from udev random
    dev_seed_rand();


    /* Process arguments */
    int opt;
    char *configFile = NULL;
    while ((opt = getopt(argc, argv, "C:h")) != -1) {
	switch(opt) {
	    case 'C':
		configFile = optarg;
	    break;
	    default:
		printf("Usage: testMult -C config file [-h this]\n");
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

    GenomeP g(new Genome(&P));
    hopper fit(1000, &config, &P);

    double fitness = 0.0;
    for (int i = 0; i<10; ++i) {
	fitness = fit(g);
	cout<<i<<": "<<fitness<<endl;
    }

    double maxFit = -1e9, curMaxFit = 0;
    
    return 0;
}
