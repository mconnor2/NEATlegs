#ifndef _GENETIC_ALGORITHM_H
#define _GENETIC_ALGORITHM_H

#include <functional>
#include <vector>

#include "Genome.h"


/**
 * Paramters for running genetic algorithm experiment.  
 *   ie population size, mutation rates, input/output sizes, etc.
 */
struct ExpParameters {
    int popSize;
    int nInput, nOutput;
}

class FitnessFunction : public unary_function<const Genome&, double> {
    public:
	virtual double operator()(const Genome *g);
}

/**
 * NEAT Genetic Algorithm
 *
 *  -Given an experiment (parameters + fitness function), store a population
 *   of individuals competing at this experiment
 *  -Divide individuals into species to try to foster innovations
 */
class GeneticAlgorithm {
    public:
	GeneticAlgorithm(ExpParameters *P, FitnessFunction *f);
	~GeneticAlgorithm();

	// Produce one generation of the genetic algorithm, returning
	// the max fitness of current generation.
	double nextGeneration();

    private:
	ExpParameters *P;
	FitnessFunction *fitnessF;
	int generation;

	vector<Genome *> population;

}


#endif
