#include "GeneticAlgorithm.h"

#include <algorithm>
#include <iterator>
#include <iostream>

#include "Genome.h"
#include "InnovationStore.h"
#include "random.h"

#include <boost/mem_fn.hpp>
#include <boost/bind.hpp>

#include <libconfig.h++>

using namespace std;

int ExpParameters::loadFromFile(const char* configFile) {
    libconfig::Config config;
    try {
	config.readFile(configFile);
    } catch (libconfig::ParseException pe) {
	cerr<<"ExpParameters::loadFromFile parse error"<<endl;
	cerr<<"   config file "<<configFile<<endl;
	cerr<<"   line number "<<pe.getLine()<<endl;
	cerr<<"   error: "<<pe.getError()<<endl;

	return 0;
    } catch (...) {
	cerr<<"ExpParameters::loadFromFile error reading from file."<<endl;

	return 0;
    }

    if (!config.exists("global")) {
	cerr<<"ExpParameters::loadFromFile config file must have 'global' section."<<endl;

	return 0;
    }

    //Hard code in reading of values
    if (!(config.lookupValue("global.popSize", popSize)
	  && config.lookupValue("global.nInput", nInput)
	  && config.lookupValue("global.nOutput", nOutput)))
    {
	cerr<<"ExpParameters::loadFromFile missing one of popSize, nInput, nOutput"<<endl;
	return 0;
    }

    try {
	inheritAllLinks = config.lookup("global.inheritAllLinks");
	inheritDominant = config.lookup("global.inheritDominant");
	linkEnabledRate = config.lookup("global.linkEnabledRate");
	weightMutationRate = config.lookup("global.weightMutationRate");
	weightPerturbScale = config.lookup("global.weightPerturbScale");
	weightPerturbNormal = config.lookup("global.weightPerturbNormal");
	weightPerturbUniform = config.lookup("global.weightPerturbUniform");
	addLinkMutationRate = config.lookup("global.addLinkMutationRate");
	addNodeMutationRate = config.lookup("global.addNodeMutationRate");
	compatGDiff = config.lookup("global.compatGDiff");
	compatWDiff = config.lookup("global.compatWDiff");
	compatThresh = config.lookup("global.compatThresh");
	specieMate = config.lookup("global.specieMate");
	oldAge = config.lookup("global.oldAge");
    } catch (libconfig::SettingTypeException e) {
	cerr<<"ExpParameters::loadFromFile SettingTypeException while loading parameters"<<endl;
	return 0;
    } catch (libconfig::SettingNotFoundException e) {
	cerr<<"ExpParameters::loadFromFile SettingNotFoundException while loading parameters"<<endl;
	return 0;
    } catch (...) {
	cerr<<"ExpParameters::loadFromFile some other exception while loading parameters"<<endl;
	return 0;
    }
   
    return 1;
}

/**
 * Initialize the population of genomes using the appropriate parameters
 */
GeneticAlgorithm::GeneticAlgorithm (ExpParameters *_P, FitnessFunction* _f) : 
				    P(_P), fitnessF(_f)
{
    population.reserve(P->popSize);
    for (int i = 0; i<P->popSize; ++i) {
	GenomeP g(new Genome(P));
	population.push_back(g);
	
	speciate(g, species);
    }
    generation = 0;

    IS = new InnovationStore(P);
}

GeneticAlgorithm::~GeneticAlgorithm() {
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

void GeneticAlgorithm::speciate(const GenomeP& g, specieVec &sv) 
{
    specie_it s;
    for (s = sv.begin(); s != sv.end(); ++s) {
	if (g->compat((*s)->representative()) < P->compatThresh) {
	    (*s)->addMember(g);
	    break;
	}
    }
    if (s == sv.end()) {
	//No compatable species found, add one
	SpecieP ns(new Specie());
	sv.push_back(ns);
	sv.back()->addMember(g);
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

    //Just for fun, lets use for_each to find the fitness for
    // each individual, storing them in individual genome
    for_each(population.begin(), population.end(), *fitnessF);

    //Find true mean and max fitness of population, ignoring species size
    double maxFit = -1e20, sumFit = 0;
    for (int i = 0; i<P->popSize; ++i) {
	sumFit += population[i]->fitness;
	if (population[i]->fitness > maxFit) {
	    maxFit = population[i]->fitness;
	    maxFitI = population[i];
	}
    }
    double avgFit = sumFit / (double)P->popSize;
    
    //Sum fitness of each species, and divide individuals by size of group
    for_each(species.begin(), species.end(), 
	     boost::mem_fn(&Specie::calculateFitness));

    //Since each individual's fitness is divided by size of the group
    // then sum of total fitness should be same as sum of the species average
    // fitness
    //XXX May be (slightly?) faster to sum over fewer species than entire
    //    population.  Sum should be the same
    sumFit = 0;
    for (int i = 0; i<P->popSize; ++i)
	sumFit += population[i]->fitness;
    
    species.remove_if(boost::bind(&Specie::cull, _1, 
				  P->oldAge, boost::ref(sumFit)));

    print_statistics(generation,maxFit,avgFit);

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
    
    cout<<"Max fit network:"<<endl;
    maxFitI->printDescription("  ");

    cout<<"#Species "<<species.size()<<endl;
    cout<<endl;

    genomeVec nextGen;
    nextGen.reserve(P->popSize);

    specieVec nextGenSpecies;

    IS->newGeneration();

    int nextGenPop = 0;

    //Save champions for next generation
    // stay representative member for next gen species
    //nextGen.push_back(population[maxFitI]);
    for (specie_it s = species.begin(); s != species.end(); s++) {
	nextGen.push_back((*s)->representative());

	nextGenSpecies.push_back(SpecieP(new Specie((*s)->age + 1)));
	nextGenSpecies.back()->addMember((*s)->representative());
	
	nextGenPop++;
    }
    

    //Now fill rest of population by mating random individuals, chosen by
    // distribution of fitness.
    while (nextGenPop<P->popSize) {
	GenomeP p1, p2;
	if (rand_double() < P->specieMate) {
	    //Select Species based on average fitness
	    double rfit = sumFit * rand_double();
	    specie_it sit = selectParent(species.begin(), species.end(), rfit);

	    if (sit == species.end()) {
		cerr<<"Something wrong with specie selection..."<<endl;
		continue;
	    }

	    //Now choose both parents from this species
	    SpecieP sp = *sit;
	    genome_it p1t, p2t;
	    rfit = sp->fitness * rand_double();
	    p1t = selectParent(sp->members.begin(), sp->members.end(), rfit);
	    	
	    rfit = sp->fitness * rand_double();
	    p2t = selectParent(sp->members.begin(), sp->members.end(), rfit);
	    
	    if (p1t == sp->members.end() or p2t == sp->members.end()) {
		cerr<<"Something wrong with specie selection of parents..."<<endl;
		continue;
	    }
	    p1 = *p1t;
	    p2 = *p2t;
	} else {
	    //Interspecies mating, go over entire population
	    // although each members fitness scaled by species size
	    genome_it p1t, p2t;
	    double rfit = sumFit * rand_double();
	    p1t = selectParent(population.begin(), population.end(), rfit);
	
	    rfit = sumFit * rand_double();
	    p2t = selectParent(population.begin(), population.end(), rfit);
	    
	    if (p1t == population.begin() or p2t == population.end()) {
		cerr<<"Something wrong with population selection of parents..."<<endl;
		continue;
	    }
	    
	    p1 = *p1t;
	    p2 = *p2t;
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

	++nextGenPop;
	
	#ifdef _DEBUG_PRINT
	    cout<<endl;
	#endif
    }
    
    #ifdef _DEBUG_PRINT
	cout<<"Next generation has population "<<nextGen.size()<<endl;
    #endif

    population.swap(nextGen);

    species.swap(nextGenSpecies);

    ++generation;

    return maxFit;
}

template<class FitnessIt>
FitnessIt GeneticAlgorithm::selectParent(FitnessIt first, FitnessIt last, 
					 double rfit)
{
    while (first != last && rfit > (*first)->fitness) {
	rfit -= (*first)->fitness;
	first++;
    }
    return first;
}

void GeneticAlgorithm::printPopulation() const {
    cout<<"Population size: "<<population.size()<<endl;
    for (int i = 0; i<population.size(); ++i) {
	cout<<"Member "<<i<<":"<<endl;
	population[i]->printDescription("  ");
    }
}

void GeneticAlgorithm::print_statistics(int gen, double maxFit, 
					double meanFit) const
{
    //Print out overall fitness statistics of current gen
    cerr<<gen<<"\t"<<meanFit<<"\t"<<maxFit;

    //Print out number of species, and for each species give stats
    cerr<<"\t"<<species.size();
    for_each(species.begin(), species.end(), 
	     boost::mem_fn(&Specie::print_statistics));
    
    cerr<<endl;
}
