#ifndef __SIMULATION_H
#define __SIMULATION_H

#include <functional>
#include <string>
#include <vector>

#include "Creature.h"
#include "CreatureSpec.h"
#include "World.h"

namespace libconfig { class Config; }
class Renderer;

/**
 * What muscle energy costs: a weighted sum of positive work, negative work
 * (absorbed, as a positive amount) and the force-time integral.  A
 * metabolic model uses the inverse muscle efficiencies (about 4 and 0.83)
 * and a cost for holding force (J per N s, i.e. holding F costs like
 * moving it at that speed).  The defaults count positive work only.
 */
struct EnergyCost {
    double positiveWork = 1;	//J per J put into the body
    double negativeWork = 0;	//J per J absorbed
    double forceTime = 0;	//J per N s of muscle force

    double operator() (double posWork, double negWork, double forceTime)
	const;
};

/**
 * How an episode runs and when it ends.
 */
struct EpisodeOptions {
    int maxSteps = 1000;
    float hz = 60.0f;
    int subSteps = 4;
    double headFloor = 0.75;		//Ends when the head drops below this
    std::vector<std::string> groundLimbs;	//If set, ends when any other
						// limb touches the ground
    double energyBudget = 0;		//Ends when the energy cost reaches
					// this (J); 0 for no limit
    EnergyCost energyCost;
};

// Options from a config's global section (maxSteps, headFloor,
// groundLimbs, energyBudget, energyCost), on top of the given defaults
EpisodeOptions episodeOptionsFromConfig (const libconfig::Config &config,
					 EpisodeOptions defaults = {});

enum class EpisodeEnd {
    Running,
    MaxSteps,
    HeadBelowFloor,
    ForbiddenContact,	//A limb not in groundLimbs touched the ground
    EnergySpent,	//Energy cost reached the budget
    Stopped		//Stopped from outside (e.g. the window was closed)
};

struct EpisodeResult {
    int steps = 0;			//Physics steps taken
    double maxHeadX = 0, maxHeadY = 0;	//Over steps that didn't end the
					// episode, starting from 0
    double energy = 0;			//Energy cost (EnergyCost) so far
    double positiveWork = 0, negativeWork = 0;	//Muscle work (J; negative
						// work <= 0)
    double forceTime = 0;		//Integral of muscle |force| (N s)
    EpisodeEnd end = EpisodeEnd::Running;
};

/**
 * One creature in its own world, controlled through its sensors (network
 * inputs) and muscles (network outputs, two per muscle: strength then
 * length).  Tracks the head and ends the episode by the options' rules.
 */
class Simulation {
    public:
	// Throws std::runtime_error if the spec has no "head" point, or
	// groundLimbs names a limb the spec doesn't have
	Simulation (const CreatureSpec &spec, const EpisodeOptions &opt = {});

	int numInputs () const { return nInputs; }
	int numOutputs () const { return nOutputs; }

	void readSensors (double *in) const;
	void applyOutputs (const double *out);

	// Advance one physics step, then check the end rules.  afterPhysics
	// (e.g. drawing a frame) runs in between; returning false stops the
	// episode.  Returns whether the episode is still running.
	bool step (const std::function<bool ()> &afterPhysics = nullptr);

	void stop ();
	bool running () const { return res.end == EpisodeEnd::Running; }

	const EpisodeResult &result () const { return res; }
	Vec2 head () const;

	void draw (Renderer &r) const { world.draw(r); }
	Creature &creature () { return *body; }
	const Creature &creature () const { return *body; }

    private:
	EpisodeOptions opt;
	World world;
	CreatureP body;
	shapePos headPoint;
	std::vector<BodyId> mustNotTouch;
	int nInputs, nOutputs;
	EpisodeResult res;
};

/**
 * Fitness of an episode: (maxHeadX + base) * survival^survivalExponent,
 * where survival is the fraction of maxSteps survived.  Reaching max steps
 * or spending the energy budget counts as surviving the whole episode;
 * falling, a forbidden contact, or being stopped counts the steps taken.
 * The defaults give plain maxHeadX.
 *
 * Multiplying by survival means an early fall (e.g. diving forward) is
 * worth little, and base gives staying up without moving some credit
 * without making it better than moving.
 */
struct FitnessOptions {
    double base = 0;			//Metres added to the distance
    double survivalExponent = 0;	//0 ignores survival
};

// Options from a config's global section (fitnessBase, survivalExponent),
// on top of the given defaults
FitnessOptions fitnessOptionsFromConfig (const libconfig::Config &config,
					 FitnessOptions defaults = {});

// Fraction of the episode survived, in [0, 1]
double survival (const EpisodeResult &r, int maxSteps);

double episodeFitness (const EpisodeResult &r, int maxSteps,
		       const FitnessOptions &f);

// Network (or any policy): sensor inputs -> muscle outputs
typedef std::function<void (const double *in, double *out)> Controller;

// Called after each physics step, before the end rules; return false to
// stop the episode
typedef std::function<bool (const Simulation &)> FrameCallback;

// Run one episode from the start pose until it ends
EpisodeResult runEpisode (const CreatureSpec &spec, const EpisodeOptions &opt,
			  const Controller &controller,
			  const FrameCallback &frame = nullptr);

const char *toString (EpisodeEnd end);

#endif
