#ifndef _GENETIC_ALGORITHM_H
#define _GENETIC_ALGORITHM_H

#include <functional>
#include <vector>
#include <boost/shared_ptr.hpp>

using namespace std;

#include "Genome.h"
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
};

//class FitnessFunction : public unary_function<const Genome*, double> {
//    public:
//	virtual double operator()(const Genome *g) = 0;
//};

typedef vector<GenomeP> genomeVec;
typedef genomeVec::iterator genome_it;
typedef genomeVec::const_iterator genome_cit;

struct Specie {
    int nMembers;
    double fitness;	//Average fitness of its members
    genomeVec members;

    Specie() : nMembers(0), fitness(-1) {}

    GenomeP representative () const {return members[0];}
    
    void addMember(const GenomeP &m) {
	members.push_back(m);
	nMembers++;
    };

    double maxFitness() {
	//Representative is the most fit individual, per generation
	return members[0]->fitness;
    }

    double calculateFitness() {
	fitness = 0;
	GenomeP maxFit = members[0];
	for (genome_it pi = members.begin(); pi != members.end(); ++pi) {
	    (*pi)->fitness /= nMembers;
	    fitness += (*pi)->fitness;

	    if ((*pi)->fitness > maxFit->fitness) {
		maxFit = *pi;
	    }
	}

	//Make sure the representative member is the most fit
	members[0].swap(maxFit);
    }
};

typedef boost::shared_ptr<Specie> SpecieP;
typedef vector<SpecieP> specieVec;


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

	void printPopulation() const;

	GenomeP bestIndiv() const {
	    return maxFitI;
	}

    private:
	ExpParameters *P;
	FitnessFunction *fitnessF;
	int generation;
	
	InnovationStore *IS;

	template<class FitnessStore>
	FitnessStore selectParent(const vector<FitnessStore> &pop, double rfit);
        
	int speciate(const GenomeP& g, specieVec &sv);

	void print_statistics(int gen, double maxFit, double meanFit) const;

	genomeVec population;

	specieVec species;

	GenomeP maxFitI;

};

#include "GeneticAlgorithm.cpp"

#endif
