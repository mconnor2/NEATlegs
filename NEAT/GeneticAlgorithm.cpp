//#include "GeneticAlgorithm.h"

#include <algorithm>
#include <iterator>
#include <iostream>

#include "Genome.h"
#include "InnovationStore.h"
#include "random.h"

#include <boost/mem_fn.hpp>

/**
 * Initialize the population of genomes using the appropriate parameters
 */
template <class FitnessFunction>
GeneticAlgorithm<FitnessFunction>::GeneticAlgorithm
    (ExpParameters *_P, FitnessFunction *_f) : P(_P), fitnessF(_f)
{
    population.reserve(P->popSize);
    for (int i = 0; i<P->popSize; ++i) {
	GenomeP g(new Genome(P));
	population.push_back(g);
	
	speciate(g, species);
    }
    generation = 0;

    IS = new InnovationStore(P);
    
    maxFitI = -1;
}

template <class FitnessFunction>
GeneticAlgorithm<FitnessFunction>::~GeneticAlgorithm() {
    //Must clean up the population of genomes
    /*
    for (genomeVec_it i = population.begin();
	 i != population.end(); ++i) 
    {
	delete (*i);
    }
    */

    delete IS;
}

template <class FitnessFunction>
int GeneticAlgorithm<FitnessFunction>::speciate(const GenomeP& g, 
						specieVec &sv) 
{
    int s = 0;
    for (; s<sv.size(); ++s) {
	if (g->compat(sv[s]->representative()) < P->compatThresh) {
	    g->specie = s;
	    sv[s]->addMember(g);
	    break;
	}
    }
    if (s == sv.size()) {
	//No compatable species found, add one
	SpecieP ns(new Specie());
	sv.push_back(ns);
	sv[s]->addMember(g);
    }
    return s;
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

    //Just for fun, lets use for_each to find the fitness for
    // each individual, storing them in individual genome
    for_each(population.begin(), population.end(), *fitnessF);

    //Find true mean and max fitness of population, ignoring species size
    double maxFit = -1e20, sumFit = 0;
    maxFitI = -1;
    for (int i = 0; i<P->popSize; ++i) {
	sumFit += population[i]->fitness;
	if (population[i]->fitness > maxFit) {
	    maxFit = population[i]->fitness;
	    maxFitI = i;
	}
    }
    double avgFit = sumFit / (double)P->popSize;
    
    //Sum fitness of each species, and divide individuals by size of group
    for_each(species.begin(), species.end(), 
	     boost::mem_fn(&Specie::calculateFitness));
   
    //Since each individual's fitness is divided by size of the group
    // then sum of total fitness should be same as sum of the species average
    // fitness
    sumFit = 0;
    for (int i = 0; i<P->popSize; ++i)
	sumFit += population[i]->fitness;
    

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

    genomeVec nextGen;
    nextGen.reserve(P->popSize);

    specieVec nextGenSpecies;
    nextGenSpecies.reserve(species.size());

    IS->newGeneration();

    int nextGenPop = 0;

    //Save champions for next generation
    // stay representative member for next gen species
    //nextGen.push_back(population[maxFitI]);
    for (int s = 0; s<species.size(); s++) {
	nextGen.push_back(species[s]->representative());

	nextGenSpecies.push_back(SpecieP(new Specie()));
	nextGenSpecies[s]->addMember(species[s]->representative());
	
	nextGenPop++;
    }
    
    cout<<"Max fit network:"<<endl;
    population[maxFitI]->printDescription("  ");
    cout<<endl;

    cout<<"#Species "<<species.size()<<endl;

    //Now fill rest of population by mating random individuals, chosen by
    // distribution of fitness.
    for (; nextGenPop<P->popSize; ++nextGenPop) {
	GenomeP p1, p2;
	if (rand_double() < P->specieMate) {
	    //Select Species based on average fitness
	    double rfit = sumFit * rand_double();
	    SpecieP sp = selectParent(species, rfit);

	    if (sp == NULL) {
		cerr<<"Something wrong with specie selection..."<<endl;
		--nextGenPop;
		continue;
	    }

	    //Now choose both parents from this species
	    rfit = sp->fitness * rand_double();
	    p1 = selectParent(sp->members, rfit);
	
	    rfit = sp->fitness * rand_double();
	    p2 = selectParent(sp->members, rfit);
	} else {
	    //Interspecies mating, go over entire population
	    // although each members fitness scaled by species size
	    double rfit = sumFit * rand_double();
	    p1 = selectParent(population, rfit);
	
	    rfit = sumFit * rand_double();
	    p2 = selectParent(population, rfit);
	}
	if (p1 == NULL or p2 == NULL) {
	    cerr<<"Something wrong with selection of parents..."<<endl;
	    --nextGenPop;
	    continue;
	}

	#ifdef _DEBUG_PRINT
	    cout<<"  Creating child "<<i<<endl;
	    cout<<"    Parents p1: fit="<<p1->fitness<<", "
		<<"p2: fit="<<p2->fitness<<endl;
	#endif

	// Make sure p1 is dominant parent
	if (p1->fitness < p2->fitness) {
	    p1.swap(p2);
	}

	GenomeP child = p1->mate(p2, IS);

	child->mutate();

	nextGen.push_back(child);

	//Find the specie this child belongs to.
	speciate(child, nextGenSpecies);

	#ifdef _DEBUG_PRINT
	    cout<<endl;
	#endif
    }
    
    #ifdef _DEBUG_PRINT
	cout<<"Next generation has population "<<nextGen.size()<<endl;
    #endif

    //Don't need memory used by previous generation, so delete those genomes
    // and copy next generation to current.
    // Don't delete the champion, because that pointer is just copied
    // to the next generation.
    //for (int i = 0; i<P->popSize; ++i) {
    //	if (i != maxFitI)
    //	    delete population[i];
    //}
    
    //population.clear();
    //population.resize(P->popSize);
    
    //copy(nextGen.begin(), nextGen.end(), population.begin());
    
    population.swap(nextGen);

    species.swap(nextGenSpecies);

    ++generation;

    return maxFit;
}
	
template<class FitnessFunction> template<class FitnessStore>
FitnessStore GeneticAlgorithm<FitnessFunction>::
    selectParent(const vector<FitnessStore> &pop, double rfit)
{
    typedef vector<FitnessStore> fitVec;
    for (typename fitVec::const_iterator pi = pop.begin(); 
	 pi != pop.end(); ++pi) 
    {
	if (rfit < (*pi)->fitness) 
	    return *pi;
	
	rfit -= (*pi)->fitness;
    }
    return FitnessStore();
}

template <class FitnessFunction>
void GeneticAlgorithm<FitnessFunction>::printPopulation() const {
    cout<<"Population size: "<<population.size()<<endl;
    for (int i = 0; i<population.size(); ++i) {
	cout<<"Member "<<i<<":"<<endl;
	population[i]->printDescription("  ");
    }
}
