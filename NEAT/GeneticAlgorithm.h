#ifndef _GENETIC_ALGORITHM_H
#define _GENETIC_ALGORITHM_H

#include <functional>
#include <vector>
#include <boost/shared_ptr.hpp>
#include <boost/function.hpp>

//Really only need these for typedefs, can put those in one header to
// keep in one place
#include "Genome.h"
#include "Specie.h"
//class Genome;
//class InnovationStore;

/**
 * Paramters for running genetic algorithm experiment.  
 *   ie population size, mutation rates, input/output sizes, etc.
 */
struct ExpParameters {
    int popSize;
    int nInput, nOutput;

    bool inheritAllLinks;		//Should child get links (excess, etc)
					// from both parents, or just dominant

    //Mating probabilities:
    double inheritDominant;		//For shared genes, probability weight
					//  comes only from dominant parent,
					//  otherwise averaged.
    double linkEnabledRate;		//If link disabled in a parent, then
					//  with some probability enable it

    double weightMutationRate;		//Percent of links that will have
					//  their weights mutated
    double weightPerturbScale;		//Scale that weights should be mutated
    double weightPerturbNormal;		//Chance that weight will get some
					//  guassian noise added to it
    double weightPerturbUniform;	//Chance that weight will have some
					//  uniform noise added to it
					//Otherwise weight will be reset to
					//  some random amount

    double addLinkMutationRate;		//Percent of matings that will have
					// a link added
    double addNodeMutationRate;		//Percent of matings that will have
					// a new node and two links added

    double compatGDiff;			//How much gene difference is
					//weighted for compatability
    double compatWDiff;			//How much link weight difference is
					//weighed for compatability
    
    double compatThresh;		//Compatability threshold for members
					// of the same specie

    double specieMate;			//Number of children whose parents
					// are selected from one specie

    int oldAge;				//Age after which young species are
					// not safe from culling
    
    int loadFromFile (const char* configFile);
};

typedef boost::function<double (const GenomeP)> FitnessFunction;
//typedef std::unary_function<const GenomeP, double> FitnessFunction;
//typedef double FitnessFunction(const GenomeP);
//class FitnessFunction : public std::unary_function<const GenomeP, double>{
//    public:
//	virtual double operator() (const GenomeP) = 0;
//};

/**
 * NEAT Genetic Algorithm
 *
 *  -Given an experiment (parameters + fitness function), store a population
 *   of individuals competing at this experiment
 *  -Divide individuals into species to try to foster innovations
 */
class GeneticAlgorithm {
    public:
	GeneticAlgorithm(ExpParameters *P, FitnessFunction* f);
	~GeneticAlgorithm();

	// Produce one generation of the genetic algorithm, returning
	// the max fitness of current generation.
	double nextGeneration();

	void printPopulation() const;

	GenomeP bestIndiv() const {
	    return maxFitI;
	}

    private:
	ExpParameters *P;
	FitnessFunction* fitnessF;
	int generation;
	
	InnovationStore *IS;

	template<class FitnessIt>
	FitnessIt selectParent(FitnessIt first, FitnessIt last, 
			       double rfit);
        
	void speciate(const GenomeP& g, specieVec &sv);

	void print_statistics(int gen, double maxFit, double meanFit) const;

	genomeVec population;

	specieVec species;

	GenomeP maxFitI;

};

#endif
