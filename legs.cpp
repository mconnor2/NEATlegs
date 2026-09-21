#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>

#include <vector>

#include <SDL3/SDL_timer.h>

#include <libconfig.h++>

#include "Display.h"
#include "BoxScreen.h"
#include "World.h"
#include "Creature.h"

using namespace std;

const int Width = 640;
const int Height = 480;

void runSimulation (Display *display, World *world, CreatureP &C) {
    //100 pixels a meter
    BoxScreen s(display, 100.0f);

    int nFrames = 100;
    long frames = 0;
    uint64_t ticks = SDL_GetTicks(), nt;
    double sec;
    while (1) {
	display->clear();
	
	//Draw and update the world
	world->draw(&s);
	world->step();

	//Space resets the creature, anything else exits
	DisplayEvent e = display->poll();
	if (e == DisplayEvent::Quit) return;
	if (e == DisplayEvent::Space) C->reset();

	display->present();

	++frames;
	if (frames%nFrames == 0) {
	    nt = SDL_GetTicks();
	    sec = (nt-ticks)/1000.0;
	    printf("%d frames in %0.4f seconds, %0.4f fps\n", 
		    nFrames, sec, nFrames/sec);
	    ticks = nt;
	}

	display->waitFrame();
    }
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

    /* Initialize the World, take default hz and substeps */
    World w(60.0f);

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

    CreatureP walker = w.createCreature(config);
    if (!walker) {
	fprintf(stderr, "Couldn't init creature from file, exiting.\n");
	exit(1);
    }

    try {
	Display display("Walkabout!", Width, Height, 120);
	runSimulation(&display, &w, walker);
    } catch (exception &e) {
	cerr<<e.what()<<endl;
	return 1;
    }
    return 0;
}
