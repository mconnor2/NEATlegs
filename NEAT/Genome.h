#ifndef _GENOME_H
#define _GENOME_H

//#include "Network.h"
struct Link;
class Network;

//#include "GeneticAlgorithm.h"
struct ExpParameters;

class InnovationStore;

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

	Genome *mate(const Genome* parent2, InnovationStore *IS) const;

	void mutate();

	Network *createNewNetwork() const;

	void printDescription(const char *prefix = "") const;

	//void save(file)
	//void load(file)
    private:
	Link* links;
	int nLinks;
	int nNodes;

	ExpParameters *P;
};


#endif
