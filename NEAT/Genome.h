#ifndef _GENOME_H
#define _GENOME_H

//#include "Network.h"
struct Link;
class Network;

#include <iosfwd>
#include <memory>
#include <vector>

//#include "GeneticAlgorithm.h"
#include "NEATtypes.h"

/**
 * NEAT Genome: vector of Links that define a networks structure.
 *
 * Implements mating as crossover of links (when innovation number matches),
 *  and mutation as modifying weights and structure.
 */
class Genome {
    public:
	Genome (ExpParameters *_P);
	~Genome();

	GenomeP mate(const GenomeP &parent2, InnovationStore *IS) const;
	GenomeP singleMate(InnovationStore *IS) const;

	void mutate();

	double compat(const GenomeP &g2) const;

	Network *createNewNetwork() const;

	void printDescription(const char *prefix = "") const;

	//Individual Genome will store current fitness.
	double fitness;
	int specie;

	//How many steps of simulation were run during objective, if the
	// fitness function records it (used for steps/sec statistics)
	int steps = 0;

	//Energy the controller spent (e.g. positive muscle work, in joules)
	// during the objective, if the fitness function records it
	double energy = 0;

	//Task score before shaping into fitness (e.g. distance travelled),
	// if the fitness function records it
	double distance = 0;

	// Independent deep copy (safe to evaluate on another thread while
	// the original is in use)
	GenomeP clone() const;

	int numLinks() const { return nLinks; }
	int numEnabledLinks() const;
	int numHiddenNodes() const;

	//Plain text format: '#' comment lines, a "genome" header line, then
	// one "link" line per gene.  load() returns an empty pointer (and
	// prints why) if the file is malformed or doesn't match P's
	// input/output counts.
	void save(std::ostream &out) const;
	static GenomeP load(std::istream &in, ExpParameters *P);

    private:
	Genome (Link *_links, int _nLinks, int _nNodes, ExpParameters *_P);
	
	Link* links;
	int nLinks;
	int nNodes;

	ExpParameters *P;
};

#endif
