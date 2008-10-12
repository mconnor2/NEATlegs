#ifndef _GENOME_H
#define _GENOME_H

//#include "Network.h"
struct Link;
class Network;

//#include "GeneticAlgorithm.h"
struct ExpParameters;


/**
 * NEAT Genome: vector of Links that define a networks structure.
 *
 * Implements mating as crossover of links (when innovation number matches),
 *  and mutation as modifying weights and structure.
 */
class Genome {
    public:
	Genome (ExpParameters *_P);
	Genome (Link *_links, int _nLinks, ExpParameters *_P);
	~Genome();

	Genome *mate(const Genome* parent2) const;

	void mutate();

	Network *createNewNetwork() const;

	void printDescription(const char *prefix = "") const;

	//void save(file)
	//void load(file)
    private:
	Link* links;
	int nLinks;
	ExpParameters *P;	
};


#endif