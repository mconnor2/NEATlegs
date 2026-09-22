#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <ctime>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <atomic>
#include <functional>
#include <mutex>
#include <thread>

#include <atomic>
#include <functional>
#include <mutex>
#include <thread>
#include <memory>

#include <libconfig.h++>


#include "NEAT/random.h"
#include "NEAT/Network.h"
#include "NEAT/Genome.h"
#include "NEAT/GeneticAlgorithm.h"
#include "NEAT/RunLog.h"

#include "Display.h"
#include "StatsOverlay.h"
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
	{
	    //Run ends when the head drops below this (config global.headFloor)
	    headFloor = 0.75;
	    config->lookupValue("global.headFloor", headFloor);

	    //Optional list of limbs allowed to touch the ground; the run ends
	    // (as a fall) when any other limb does (global.groundLimbs)
	    if (config->exists("global.groundLimbs")) {
		const libconfig::Setting &gl = config->lookup("global.groundLimbs");
		for (int i = 0; i < gl.getLength(); ++i)
		    groundLimbs.push_back(gl[i].c_str());
	    }
	}

	// With a display, the run is drawn in real time and overlay (if set)
	// is called each frame to draw on top of the simulation.
	double operator()(const GenomeP &g, Display *display = nullptr,
			  const function<void(Display &)> &overlay = nullptr)
	    const
	{
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

	    if (!C->shapes.count("head")) {
		cerr<<"Creature must define a shape named 'head', exiting."
		    <<endl;
		exit(1);
	    }
	    const shapePos headPos = C->shapes["head"];

	    vector<BodyId> mustNotTouch;
	    if (!groundLimbs.empty()) {
		for (auto &kv : C->limbs)
		    if (find(groundLimbs.begin(), groundLimbs.end(), kv.first) ==
			groundLimbs.end())
			mustNotTouch.push_back(kv.second);
	    }
	   
	    C->reset();
/*	    {
		shapePos headPos = C->shapes["head"];
	        Vec2 headV = headPos.b->GetWorldPoint(headPos.localPos);
		cout<<"Head position: "<<headV.x<<", "<<headV.y<<endl;
	    }
*/
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
		    if (overlay) overlay(*display);

//		    cout<<"Head height: "<<headV.x<<", "<<headV.y<<endl;
		    
		    s.keepViewable(headV);
		    s.drawGrid();
		    w.draw(&s);
		    display->present();

		    //Space skips ahead, quit is left for the caller to see
		    // via display->quitRequested()
		    if (display->poll() != DisplayEvent::None) break;

		    display->waitFrame();
		}

		/*--- Check for failure.  If so, return steps ---*/
		// For hopper, failure is if creature's head drops below
		// some level.
		if (headV.y < headFloor) break;
		bool fell = false;
		for (BodyId b : mustNotTouch)
		    if (C->touchesOutside(b)) { fell = true; break; }
		if (fell) break;
		if (headV.x > maxX) maxX = headV.x;
		if (headV.y > maxY) maxY = headV.y;
	    }

	    delete [] in;
	    delete [] out;

//	cout<<"Made it "<<steps<<" steps..."
//	    <<static_cast<double>(steps)/MAX_STEPS<<endl;
  
	    g->steps = steps;
	    g->energy = C->positiveWork();

	    //return (g->fitness = static_cast<double>(steps)/(MAX_STEPS+1));
	    return (g->fitness = maxX);
	    //return (g->fitness = maxY);
	};

    private:
	const int MAX_STEPS;
	//const bool random_start;

	double headFloor;
	vector<string> groundLimbs;

	const libconfig::Config *config;
	const ExpParameters *P;
};

static void usage () {
    printf("Usage: hopper -C config [options]\n"
	   "  -N gens     number of generations (default 1000)\n"
	   "  -V          watch evolution: a window replays the fittest member\n"
	   "              while the GA keeps running (Space: jump to newest)\n"
	   "  -d gens     with -V, publish a new best to watch every this many\n"
	   "              generations (default 10; implies -V)\n"
	   "  -o dir      run output directory (default runs/<config>-<time>)\n"
	   "  -s gens     snapshot top genomes every this many generations\n"
	   "              (default 10, 0 disables)\n"
	   "  -k count    genomes per snapshot (default 3)\n"
	   "  -S seed     random seed, to repeat a run exactly (default: from\n"
	   "              /dev/urandom; the seed used is printed and saved)\n"
	   "  -r genome   replay a saved genome in a window instead of\n"
	   "              running the GA\n");
}

// Default run directory: runs/<config name>-<YYYYmmdd-HHMMSS>
static string defaultRunDir (const char *configFile) {
    char stamp[32];
    time_t now = time(NULL);
    strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", localtime(&now));
    return "runs/" + std::filesystem::path(configFile).stem().string() +
	   "-" + stamp;
}

// Replay a saved genome until the window is closed
static int replay (const hopper &fit, const char *genomeFile,
		   ExpParameters *P)
{
    ifstream in(genomeFile);
    if (!in) {
	cerr<<"Can't open genome file "<<genomeFile<<endl;
	return 1;
    }
    GenomeP g = Genome::load(in, P);
    if (!g) return 1;

    unique_ptr<Display> display;
    try {
	display.reset(new Display("Hopper Replay", Width, Height));
    } catch (exception &e) {
	cerr<<e.what()<<endl;
	return 1;
    }

    string title = "Replay " +
		   std::filesystem::path(genomeFile).filename().string();
    vector<GenerationStats> noHistory;
    for (int run = 1; !display->quitRequested(); ++run) {
	double f = fit(g, display.get(), [&](Display &d) {
	    drawStatsOverlay(d, title, noHistory);
	});
	if (!display->quitRequested())
	    printf("Replay %d: fitness %.4f\n", run, f);
    }
    return 0;
}

/**
 * Shared state between the GA thread and the display (main) thread when
 * watching a run.  The GA publishes a copy of its stats every generation
 * and a clone of the best genome every displayEvery generations; the
 * display thread replays the newest published genome over and over.
 */
struct Monitor {
    std::mutex m;
    GenomeP best;		//Clone, never touched by the GA thread
    double bestFitness = 0;
    int bestGeneration = -1;
    std::shared_ptr<const vector<GenerationStats>> history;

    std::atomic<bool> stop{false}, done{false};

    void publishStats (const GeneticAlgorithm &GA) {
	//Overlay doesn't need the per-species breakdown
	auto h = std::make_shared<vector<GenerationStats>>(GA.history());
	for (GenerationStats &st : *h) st.species.clear();
	std::lock_guard<std::mutex> lock(m);
	history = h;
    }

    void publishBest (const GeneticAlgorithm &GA) {
	const GeneticAlgorithm::RankedGenome &top = GA.topGenomes().at(0);
	GenomeP g = top.second->clone();
	std::lock_guard<std::mutex> lock(m);
	best = g;
	bestFitness = top.first;
	bestGeneration = GA.history().back().generation;
    }
};

// Evolve on a background thread while the main thread shows the latest
// published best, until the window is closed.  Closing the window stops
// the GA after its current generation.
static void watchEvolution (const hopper &fit, Display &display,
			    GeneticAlgorithm &GA, RunLog &log, int maxGen,
			    int displayEvery)
{
    Monitor mon;

    std::thread ga([&]() {
	for (int gen = 0; gen < maxGen && !mon.stop; ++gen) {
	    GA.nextGeneration();
	    log.record(GA);
	    mon.publishStats(GA);
	    if (gen % displayEvery == 0 || gen == maxGen-1)
		mon.publishBest(GA);
	}
	log.finish(GA);
	mon.done = true;
    });

    while (!display.quitRequested()) {
	GenomeP g;
	int shownGen;
	double shownFit;
	{
	    std::lock_guard<std::mutex> lock(mon.m);
	    g = mon.best;
	    shownGen = mon.bestGeneration;
	    shownFit = mon.bestFitness;
	}

	auto overlay = [&](Display &d) {
	    std::shared_ptr<const vector<GenerationStats>> h;
	    {
		std::lock_guard<std::mutex> lock(mon.m);
		h = mon.history;
	    }
	    char title[128];
	    const char *status = mon.done ? "run finished, close to exit"
					  : "evolving";
	    if (g) {
		snprintf(title, sizeof(title),
			 "Gen %d best (%.3f)   now: gen %d, %s",
			 shownGen, shownFit,
			 h && !h->empty() ? h->back().generation : 0, status);
	    } else {
		snprintf(title, sizeof(title), "Waiting for generation 0...");
	    }
	    static const vector<GenerationStats> none;
	    drawStatsOverlay(d, title, h ? *h : none);
	};

	if (g) {
	    fit(g, &display, overlay);
	} else {
	    display.clear();
	    overlay(display);
	    display.present();
	    display.poll();
	    display.waitFrame();
	}
    }

    mon.stop = true;
    if (!mon.done)
	printf("Window closed, stopping after the current generation...\n");
    ga.join();
}

int main (int argc, char **argv) {
    bool drawGen = false;
    bool haveSeed = false;
    uint64_t seed = 0;
    int displayEvery = 10;

    int maxGen = 1000;

    RunLog::Options logOpt;
    const char *replayFile = NULL;

    /* Process arguments */
    int opt;
    char *configFile = NULL;
    while ((opt = getopt(argc, argv, "VC:hN:o:s:k:r:d:S:")) != -1) {
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
	    case 'o':
		logOpt.outputDir = optarg;
	    break;
	    case 's':
		logOpt.snapshotEvery = atoi(optarg);
	    break;
	    case 'k':
		logOpt.snapshotTop = atoi(optarg);
	    break;
	    case 'r':
		replayFile = optarg;
	    break;
	    case 'S':
		seed = strtoull(optarg, NULL, 10);
		haveSeed = true;
	    break;
	    case 'd':
		displayEvery = atoi(optarg);
		drawGen = true;
		if (displayEvery < 1) {
		    fprintf(stderr, "-d needs a positive generation count\n");
		    exit(1);
		}
	    break;
	    default:
		usage();
		exit(1);
	}
    }
    
    if (haveSeed) seed_rand(seed);
    else seed = dev_seed_rand();

    if (!configFile) {
	fprintf(stderr, "Must specify config file.\n");
	usage();
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
   
    try {
	P.nInput = config.lookup("sensors").getLength()+1;
	P.nOutput = config.lookup("muscles").getLength()*2;
    } catch (...) {
	cerr<<"Must specify sensors and muscles list in config file"<<endl;
	return 1;
    }

    hopper fit(1000, &config, &P);

    if (replayFile) return replay(fit, replayFile, &P);

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
    
    if (logOpt.outputDir.empty()) logOpt.outputDir = defaultRunDir(configFile);
    logOpt.configPath = configFile;
    logOpt.seed = seed;

    GeneticAlgorithm GA(&P, &f);
    GA.setKeepTop(max(logOpt.snapshotTop, 1));

    unique_ptr<RunLog> log;
    try {
	log.reset(new RunLog(logOpt));
    } catch (exception &e) {
	cerr<<e.what()<<endl;
	return 1;
    }

    printf("Population %d, %d inputs, %d outputs, seed %llu.  Writing run to %s\n",
	   P.popSize, P.nInput, P.nOutput, (unsigned long long)seed,
	   logOpt.outputDir.c_str());

    if (display) {
	watchEvolution(fit, *display, GA, *log, maxGen, displayEvery);
	return 0;
    }

    for (int gen = 0; gen < maxGen; gen++) {
	GA.nextGeneration();
	log->record(GA);
    }
    log->finish(GA);
    return 0;
}
