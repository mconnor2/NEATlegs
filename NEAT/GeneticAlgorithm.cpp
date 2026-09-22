#include "GeneticAlgorithm.h"

#include <algorithm>
#include <iterator>
#include <iostream>

#include "Specie.h"
#include "Genome.h"
#include "InnovationStore.h"
#include "random.h"

#include <atomic>
#include <cmath>
#include <chrono>
#include <functional>
#include <thread>

using namespace std;

//Default parameters, but -1 population size
ExpParameters::ExpParameters() :
    popSize(-1), nInput(0), nOutput(0),
    inheritAllLinks(false), inheritDominant(0.9), linkEnabledRate(0.1),
    weightMutationRate(0.2), weightPerturbScale(0.1), weightPerturbNormal(0.6),
    weightPerturbUniform(0.39), addLinkMutationRate(0.2),
    addNodeMutationRate(0.1), compatGDiff(1.0), compatWDiff(0.4),
    compatThresh(7.0), targetSpecies(-1), threshAdapt(0.1), 
    singleMate(0.2), specieMate(0.99), oldAge(5), startPopulationPercent(0.5)
{ }

int ExpParameters::loadFromFile(const libconfig::Config &config) {
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
	config.lookupValue("global.inheritAllLinks",inheritAllLinks);
	config.lookupValue("global.inheritDominant",inheritDominant);
	config.lookupValue("global.linkEnabledRate",linkEnabledRate);
	config.lookupValue("global.weightMutationRate",weightMutationRate);
	config.lookupValue("global.weightPerturbScale",weightPerturbScale);
	config.lookupValue("global.weightPerturbNormal",weightPerturbNormal);
	config.lookupValue("global.weightPerturbUniform",weightPerturbUniform);
	config.lookupValue("global.addLinkMutationRate",addLinkMutationRate);
	config.lookupValue("global.addNodeMutationRate",addNodeMutationRate);
	config.lookupValue("global.compatGDiff",compatGDiff);
	config.lookupValue("global.compatWDiff",compatWDiff);
	config.lookupValue("global.compatThresh",compatThresh);
	config.lookupValue("global.targetSpecies",targetSpecies);
	config.lookupValue("global.threshAdapt",threshAdapt);
	config.lookupValue("global.singleMate",singleMate);
	config.lookupValue("global.specieMate",specieMate);
	config.lookupValue("global.oldAge",oldAge);
	config.lookupValue("global.startPopulationPercent",
			    startPopulationPercent);
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
    int initialPopulation = P->startPopulationPercent * P->popSize;
//    cout<<"Start population = "<<P->popSize<<"*"
//			       <<P->startPopulationPercent<<endl;
    population.reserve(initialPopulation);
    for (int i = 0; i<initialPopulation; ++i) {
	GenomeP g(new Genome(P));
	population.push_back(g);
	
	speciate(g, species);
    }
    generation = 0;

//    cout<<"Genetic Algorithm initialization: population "<<population.size()
//	<<", species "<<species.size()<<endl;

    if (P->targetSpecies > 0) adaptSpeciesThresh(species.size());

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
	SpecieP ns(new Specie(nextSpecieId++));
	sv.push_back(ns);
	sv.back()->addMember(g);
    }
}

void GeneticAlgorithm::adaptSpeciesThresh(const int curSpecieSize) {
    #ifdef _DEBUG_PRINT
	cerr<<"Adapting with "<<curSpecieSize<<" original speices."<<endl;
	cerr<<"   target species "<<P->targetSpecies<<endl;
    #endif
    if (curSpecieSize < P->targetSpecies) {
	P->compatThresh -= P->threshAdapt;
	#ifdef _DEBUG_PRINT
	    cout<<"  compat thresh now "<<P->compatThresh<<endl;
	#endif
    } else if (curSpecieSize > P->targetSpecies) {
	P->compatThresh += P->threshAdapt;
	#ifdef _DEBUG_PRINT
	    cout<<"  compat thresh now "<<P->compatThresh<<endl;
	#endif
    }
}

template<class FitnessIt>
FitnessIt selectParent(FitnessIt first, FitnessIt last, double rfit) {
    while (first != last && rfit > (*first)->fitness) {
	rfit -= (*first)->fitness;
	first++;
    }
    return first;
}

/**
 * Evaluate fitness of every member of the population.  Evaluations are
 * independent, so they're spread across all hardware threads.
 */
void GeneticAlgorithm::runFitness(double &seconds) const {
    auto t0 = chrono::steady_clock::now();

    atomic<size_t> next(0);
    auto worker = [&]() {
	for (size_t i = next++; i < population.size(); i = next++) {
	    (*fitnessF)(population[i]);
	}
    };

    unsigned nThreads = max(1u, thread::hardware_concurrency());
    vector<thread> threads;
    for (unsigned t = 1; t < nThreads; ++t) threads.emplace_back(worker);
    worker();
    for (auto &t : threads) t.join();

    chrono::duration<double> elapsed = chrono::steady_clock::now() - t0;
    seconds = elapsed.count();
}

/**
 * Summarize the just-evaluated population into stats.back(), and remember
 * its best genomes.  Must run before fitness sharing rescales fitness.
 */
void GeneticAlgorithm::recordStatistics(double evalSeconds) {
    GenerationStats st;
    st.generation = generation;
    st.populationSize = population.size();
    st.nSpecies = species.size();
    st.compatThresh = P->compatThresh;
    st.evalSeconds = evalSeconds;

    const size_t n = population.size();
    if (n == 0) {
	stats.push_back(st);
	return;
    }

    double sum = 0, sumSq = 0, sumHidden = 0, sumLinks = 0, sumEnergy = 0;
    long totalSteps = 0;
    st.maxFitness = -1e300;
    st.minFitness = 1e300;
    for (const GenomeP &g : population) {
	sum += g->fitness;
	sumSq += g->fitness * g->fitness;
	st.maxFitness = max(st.maxFitness, g->fitness);
	st.minFitness = min(st.minFitness, g->fitness);
	sumHidden += g->numHiddenNodes();
	sumLinks += g->numEnabledLinks();
	totalSteps += g->steps;
	sumEnergy += g->energy;
    }
    st.meanFitness = sum / n;
    st.stdevFitness = sqrt(max(0.0, sumSq / n - st.meanFitness*st.meanFitness));
    st.meanHiddenNodes = sumHidden / n;
    st.meanEnabledLinks = sumLinks / n;
    st.meanEnergy = sumEnergy / n;
    if (totalSteps > 0 && evalSeconds > 0)
	st.stepsPerSec = totalSteps / evalSeconds;

    //Diversity: mean compatibility distance over all pairs.  Deterministic
    // (no RNG draws, so it doesn't perturb the run) and cheap next to
    // fitness evaluation for populations of a few hundred.
    if (n > 1) {
	double sumCompat = 0;
	for (size_t i = 0; i < n; ++i)
	    for (size_t j = i+1; j < n; ++j)
		sumCompat += population[i]->compat(population[j]);
	st.meanCompat = sumCompat / (n*(n-1)/2.0);
    }

    for (const SpecieP &sp : species) {
	SpecieStats ss;
	ss.id = sp->id;
	ss.age = sp->age;
	ss.size = sp->members.size();
	ss.maxFitness = -1e300;
	double specieSum = 0;
	for (const GenomeP &g : sp->members) {
	    specieSum += g->fitness;
	    ss.maxFitness = max(ss.maxFitness, g->fitness);
	}
	ss.meanFitness = ss.size ? specieSum / ss.size : 0;
	st.species.push_back(ss);
    }

    //Best genomes by raw fitness
    top.clear();
    for (const GenomeP &g : population) top.push_back(make_pair(g->fitness, g));
    size_t k = min(top.size(), (size_t)max(keepTop, 1));
    partial_sort(top.begin(), top.begin() + k, top.end(),
		 [](const RankedGenome &a, const RankedGenome &b) {
		     return a.first > b.first;
		 });
    top.resize(k);

    st.bestHiddenNodes = top[0].second->numHiddenNodes();
    st.bestEnabledLinks = top[0].second->numEnabledLinks();
    st.bestEnergy = top[0].second->energy;

    stats.push_back(st);
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
    
    double evalSeconds;
    runFitness(evalSeconds);

    //Find true mean and max fitness of population, ignoring species size
    double maxFit = -1e20, sumFit = 0;
    for (genome_it gi = population.begin(); gi != population.end(); ++gi) {
	sumFit += (*gi)->fitness;
	if ((*gi)->fitness > maxFit) {
	    maxFit = (*gi)->fitness;
	    maxFitI = *gi;
	}
    }

    recordStatistics(evalSeconds);

/*
    //What is max fitness of first specie
    double firstSpecieMax = 1e-9;
    for (genome_it gi = species.front()->members.begin();
	 gi != species.front()->members.end(); ++gi) 
    {
	if ((*gi)->fitness > firstSpecieMax) {
	    firstSpecieMax = (*gi)->fitness;
	}
    }
    cout<<"Specie 0 max fitness (before calculateFitness) = "
	<<firstSpecieMax<<endl;
*/
    //Sum fitness of each species, and divide individuals by size of group
    for_each(species.begin(), species.end(), 
	     mem_fn(&Specie::calculateFitness));
    
/*
    cout<<"Specie 0 max fitness (after calculateFitness) = "
	<<species.front()->maxFitness()<<endl;
    species.front()->members.front()->printDescription("--");
*/

    //Since each individual's fitness is divided by size of the group
    // then sum of total fitness should be same as sum of the species average
    // fitness
    //XXX May be (slightly?) faster to sum over fewer species than entire
    //    population.  Sum should be the same
    sumFit = 0;
    for (genome_it gi = population.begin(); gi != population.end(); ++gi) 
	sumFit += (*gi)->fitness;
    
    species.remove_if([&](const SpecieP &s) {
	return s->cull(P->oldAge, sumFit);
    });

    #ifdef _DEBUG_PRINT
	cout<<"Fitness values: ";
	for (genome_it gi = population.begin(); gi != population.end(); ++gi) {
	    cout<<(*gi)->fitness<<", ";
	}
	cout<<endl;
    #endif

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

	nextGenSpecies.push_back(SpecieP(new Specie((*s)->id,
						    (*s)->age + 1)));
	nextGenSpecies.back()->addMember((*s)->representative());
	
	nextGenPop++;
    }
    

    //Now fill rest of population by mating random individuals, chosen by
    // distribution of fitness.
    while (nextGenPop<P->popSize) {
	GenomeP child;
	if (rand_double() < P->singleMate) {
	    //Single parent mating, go over entire population
	    // and select a single parent, although each members fitness
	    // scaled by species size
	    genome_it p1t;
	    double rfit = sumFit * rand_double();
	    p1t = selectParent(population.begin(), population.end(), rfit);

	    child = (*p1t)->singleMate(IS);
	} else {
	    GenomeP p1, p2;
	    if (rand_double() < P->specieMate) {
		//Select Species based on average fitness
		double rfit = sumFit * rand_double();
		specie_it sit = selectParent(species.begin(), species.end(), 
					     rfit);
		if (sit == species.end()) {
		    cerr<<"Something wrong with specie selection..."<<endl;
		    continue;
		}

		//Now choose both parents from this species
		SpecieP sp = *sit;
		genome_it p1t, p2t;
		rfit = sp->fitness * rand_double();
		p1t = selectParent(sp->members.begin(), sp->members.end(), 
				   rfit);
		rfit = sp->fitness * rand_double();
		p2t = selectParent(sp->members.begin(), sp->members.end(), 
				   rfit);
		if (p1t == sp->members.end() or p2t == sp->members.end()) {
		    cerr<<"Something wrong with specie selection of parents..."
			<<endl;
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
		
		if (p1t == population.end() or p2t == population.end()) {
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

	    child = p1->mate(p2, IS);
	}

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
    
    if (P->targetSpecies > 0) adaptSpeciesThresh(species.size());

    ++generation;
    
/*    
    cout<<"Specie 0 max fitness (after full mating) = "
	<<species.front()->maxFitness()<<endl;
    species.front()->members.front()->printDescription("--");
*/
    return maxFit;
}

void GeneticAlgorithm::printPopulation() const {
    cout<<"Population size: "<<population.size()<<endl;
    for (size_t i = 0; i<population.size(); ++i) {
	cout<<"Member "<<i<<":"<<endl;
	population[i]->printDescription("  ");
    }
}
