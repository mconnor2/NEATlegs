#ifndef _GENETIC_ALGORITHM_H
#define _GENETIC_ALGORITHM_H

#include <functional>
#include <vector>
#include <boost/shared_ptr.hpp>
#include <boost/function.hpp>
#include <libconfig.h++>

#include "NEATtypes.h"
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

    int targetSpecies;			//Desired number of species for
					// adaptation, -1 turn off adaptation
    double threshAdapt;			//Amount to change compatThresh with
					// specie number adaptation
    
    double singleMate;			//Chance of single specie mating, only
					// mutating one parent
    
    double specieMate;			//Number of children whose parents
					// are selected from one specie

    int oldAge;				//Age after which young species are
					// not safe from culling
   
    //double championFitnessRate;	//A species champion will continue to
					// next generation only if its fitness
					// is within this percent of max fit
    
    double startPopulationPercent;	//How many members to start with, as
					// percent of total desired population

    int loadFromFile (const libconfig::Config &config);
    ExpParameters();
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
	void runFitness() const;
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

	void speciate(const GenomeP& g, specieVec &sv);
	void adaptSpeciesThresh(const int specieSize);

	void print_statistics(int gen, double maxFit, double meanFit) const;

	genomeVec population;

	specieVec species;

	GenomeP maxFitI;

};

#endif
