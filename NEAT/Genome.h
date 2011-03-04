#ifndef _GENOME_H
#define _GENOME_H

//#include "Network.h"
struct Link;
class Network;

#include <vector>
#include <boost/shared_ptr.hpp>

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
	Genome (Link *_links, int _nLinks, int _nNodes, ExpParameters *_P);
	~Genome();

	GenomeP mate(const GenomeP &parent2, InnovationStore *IS) const;

	void mutate();

	double compat(const GenomeP &g2);

	Network *createNewNetwork() const;

	void printDescription(const char *prefix = "") const;

	//Individual Genome will store current fitness.
	double fitness;
	int specie;

	//void save(file)
	//void load(file)
    private:
	Link* links;
	int nLinks;
	int nNodes;


	ExpParameters *P;
};



#endif
