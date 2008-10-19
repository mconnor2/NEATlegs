#ifndef _GENETIC_ALGORITHM_H
#define _GENETIC_ALGORITHM_H

#include <functional>
#include <vector>

using namespace std;

//#include "Genome.h"
class Genome;
class InnovationStore;

/**
 * Paramters for running genetic algorithm experiment.  
 *   ie population size, mutation rates, input/output sizes, etc.
 */
struct ExpParameters {
    int popSize;
    int nInput, nOutput;

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
};

//class FitnessFunction : public unary_function<const Genome*, double> {
//    public:
//	virtual double operator()(const Genome *g) = 0;
//};

/**
 * NEAT Genetic Algorithm
 *
 *  -Given an experiment (parameters + fitness function), store a population
 *   of individuals competing at this experiment
 *  -Divide individuals into species to try to foster innovations
 */
template <class FitnessFunction>
class GeneticAlgorithm {
    public:
	GeneticAlgorithm(ExpParameters *P, FitnessFunction *f);
	~GeneticAlgorithm();

	// Produce one generation of the genetic algorithm, returning
	// the max fitness of current generation.
	double nextGeneration();

	void printPopulation();

    private:
	ExpParameters *P;
	FitnessFunction *fitnessF;
	int generation;
	
	InnovationStore *IS;

	int selectParent(const vector<double> &fitVals, double rfit);

	vector<Genome *> population;

};


#include "GeneticAlgorithm.cpp"

#endif
