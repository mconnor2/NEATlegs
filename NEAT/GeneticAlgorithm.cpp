#include "GeneticAlgorithm.h"

#include <algorithm>
#include <iterator>

using namespace std;

typedef vector<Genome *>::iterator population_iterator;

/**
 * Initialize the population of genomes using the appropriate parameters
 */
GeneticAlgorithm::GeneticAlgorithm(ExpParameters *_P, FitnessFunction *_f) :
	*P(_P), fitnessF(_f)
{
    population.reserve(P->popSize);
    for (int i = 0; i<P->popSize; ++i) {
	population.push_back(new Genome(P));
    }
    generation = 0;
}

GeneticAlgorithm::~GeneticAlgorithm() {
    //Must clean up the population of genomes
    for (population_iterator i = population.begin();
	 i != population.end(); ++i) 
    {
	delete (*i);
    }

}


/**
 * Run one iteration of genetic algorithm, creating new generation and
 *  deleting previous.
 *
 * Process:
 * -Go over all individuals in the population, find each of their fitness
 * 	*Calculate average and max fitness, storing champion with best
 * -Create next generation
 *  	-Store champion
 *  	-For rest of population size:
 *  		-Randomly Choose two members of current generation based on 
 *  		 fitness
 *  		-Mate and mutate child, add to new generation.
 *  	-Delete previous generation (except champion...)
 *  	-Next generation is now current generation
 *  -Return max fitness
 */
double GeneticAlgorithm::nextGeneration() {

    //First find the fitness for all members of current generation
    vector<double> fitVals(P->popSize);

    //Just for fun, lets use transform to find the fitness for
    // each individual, and fill the fitVals 
    transform(population.begin(), population.end(),
	      fitVals, *f);

    double maxFit = -1e20, sumFit = 0;
    int maxFiti = -1;
    for (int i = 0; i<P->popSize; ++i) {
	avgFit += fitVals[i];
	if (fitVals[i] > maxFit) {
	    maxFit = fitVals[i];
	    maxFiti = i;
	}
    }
    avgFit = sumFit / (double)P->popSize;

    cout<<"Generation "<<generation
	<<": Max fitness = "<<maxFit
	<<", mean fitness = "<<avgFit<<endl;

    vector<Genome *> nextGen;
    nextGen.reserve(P->popSize);

    //Save champion for next generation
    nextGen.push_back(population[maxFiti]);
    
    //Now fill rest of population by mating random individuals, chosen by
    // distribution of fitness.
    for (int i = 0; i<P->popSize-1; ++i) {
	Genome *p1, *p2;
	//Choose first parent:
	double rfit = maxFit * rand_double();
	p1 = selectParent(fitVals, rfit);
	p2 = selectParent(fitVals, rfit);
	if (p1 == NULL or p2 == NULL) {
	    cerr<<"Something wrong with selection of parents..."<<endl;
	    i--;
	    continue;
	}
	Genome *child = p1.mate(p2);

	child.mutate();
	
	nextGen.push_back(child);
    }

    //Don't need memory used by previous generation, so delete those genomes
    // and copy next generation to current.
    // Don't delete the champion, because that pointer is just copied
    // to the next generation.
    for (int i = 0; i<P->popSize; ++i) {
	if (i != maxFiti)
	    delete population[i];
    }
    copy(nextGen.begin(), nextGen.end(), population.begin());

    return maxFit;
}

Genome *GeneticAlgorithm::selectParent(const vector<double> &fitVals,
				       double rfit)
{
    for (int p = 0; p < P->popSize-1; ++p) {
	if (rfit < fitVals[p]) {
	    return population[p];
	}
	rfit -= fitVals[p];
    }
    return NULL;
}
