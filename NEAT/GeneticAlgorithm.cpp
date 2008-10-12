//#include "GeneticAlgorithm.h"

#include <algorithm>
#include <iterator>
#include <iostream>

#include "Genome.h"
#include "random.h"

typedef vector<Genome *>::iterator population_iterator;

/**
 * Initialize the population of genomes using the appropriate parameters
 */
template <class FitnessFunction>
GeneticAlgorithm<FitnessFunction>::GeneticAlgorithm
    (ExpParameters *_P, FitnessFunction *_f) : P(_P), fitnessF(_f)
{
    population.reserve(P->popSize);
    for (int i = 0; i<P->popSize; ++i) {
	population.push_back(new Genome(P));
    }
    generation = 0;
}

template <class FitnessFunction>
GeneticAlgorithm<FitnessFunction>::~GeneticAlgorithm() {
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
template <class FitnessFunction>
double GeneticAlgorithm<FitnessFunction>::nextGeneration() {

    //First find the fitness for all members of current generation
    vector<double> fitVals(P->popSize, 0);

    //Just for fun, lets use transform to find the fitness for
    // each individual, and fill the fitVals 
    transform(population.begin(), population.end(),
	      fitVals.begin(), *fitnessF);

    double maxFit = -1e20, sumFit = 0;
    int maxFiti = -1;
    for (int i = 0; i<P->popSize; ++i) {
	sumFit += fitVals[i];
	if (fitVals[i] > maxFit) {
	    maxFit = fitVals[i];
	    maxFiti = i;
	}
    }
    double avgFit = sumFit / (double)P->popSize;

#ifdef _DEBUG_PRINT
    cout<<"Fitness values: ";
    for (int i = 0; i<P->popSize; ++i) {
	cout<<fitVals[i]<<", ";
    }
    cout<<endl;
#endif

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
	Genome *p1 = NULL, *p2 = NULL;
	//Choose first parent:
	double rfit = maxFit * rand_double();
	p1 = selectParent(fitVals, rfit);
	p2 = selectParent(fitVals, rfit);
	if (p1 == NULL or p2 == NULL) {
	    cerr<<"Something wrong with selection of parents..."<<endl;
	    i--;
	    continue;
	}
	Genome *child = p1->mate(p2);

	child->mutate();
	
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

    ++generation;

    return maxFit;
}

template <class FitnessFunction>
inline Genome *GeneticAlgorithm<FitnessFunction>::selectParent
	(const vector<double> &fitVals, double rfit)
{
    for (int p = 0; p < P->popSize-1; ++p) {
	if (rfit < fitVals[p]) {
	    return population[p];
	}
	rfit -= fitVals[p];
    }
    return NULL;
}

template <class FitnessFunction>
void GeneticAlgorithm<FitnessFunction>::printPopulation() {
    cout<<"Population size: "<<population.size()<<endl;
    for (int i = 0; i<population.size(); ++i) {
	cout<<"Member "<<i<<":"<<endl;
	population[i]->printDescription("  ");
    }
}