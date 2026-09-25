#ifndef _GENETIC_ALGORITHM_H
#define _GENETIC_ALGORITHM_H

#include <functional>
#include <vector>
#include <memory>
#include <utility>
#include <libconfig.h++>

#include "NEATtypes.h"
#include "Stats.h"
//class Genome;
//class InnovationStore;


/**
 * Paramters for running genetic algorithm experiment.
 *   ie population size, mutation rates, input/output sizes, etc.
 */
struct ExpParameters {
    int popSize;
    int nInput, nOutput;

    bool inheritAllLinks;               //Should child get links (excess, etc)
                                        // from both parents, or just dominant

    //Mating probabilities:
    double inheritDominant;             //For shared genes, probability weight
                                        //  comes only from dominant parent,
                                        //  otherwise averaged.
    double linkEnabledRate;             //If link disabled in a parent, then
                                        //  with some probability enable it

    double weightMutationRate;          //Percent of links that will have
                                        //  their weights mutated
    double weightPerturbScale;          //Scale that weights should be mutated
    double weightPerturbNormal;         //Chance that weight will get some
                                        //  guassian noise added to it
    double weightPerturbUniform;        //Chance that weight will have some
                                        //  uniform noise added to it
                                        //Otherwise weight will be reset to
                                        //  some random amount

    double addLinkMutationRate;         //Percent of matings that will have
                                        // a link added
    double addNodeMutationRate;         //Percent of matings that will have
                                        // a new node and two links added

    double compatGDiff;                 //How much gene difference is
                                        //weighted for compatability
    double compatWDiff;                 //How much link weight difference is
                                        //weighed for compatability

    double compatThresh;                //Compatability threshold for members
                                        // of the same specie

    int targetSpecies;                  //Desired number of species for
                                        // adaptation, -1 turn off adaptation
    double threshAdapt;                 //Amount to change compatThresh with
                                        // specie number adaptation

    double singleMate;                  //Chance of single specie mating, only
                                        // mutating one parent

    double specieMate;                  //Number of children whose parents
                                        // are selected from one specie

    int oldAge;                         //Age after which young species are
                                        // not safe from culling

    //double championFitnessRate;       //A species champion will continue to
                                        // next generation only if its fitness
                                        // is within this percent of max fit

    double startPopulationPercent;      //How many members to start with, as
                                        // percent of total desired population

    int loadFromFile (const libconfig::Config &config);
    ExpParameters();
};

// Evaluates a genome, storing and returning its fitness.  Called
// concurrently from several threads, so it must be thread safe.
typedef std::function<double (const GenomeP)> FitnessFunction;

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

        // Evaluate the current generation, record its statistics, then
        // breed the next one.  Returns max fitness of the evaluated
        // generation.
        double nextGeneration();

        void printPopulation() const;

        GenomeP bestIndiv() const {
            return maxFitI;
        }

        // Statistics for every generation evaluated so far (back() is the
        // most recent)
        const std::vector<GenerationStats> &history() const {
            return stats;
        }

        // Best genomes of the most recently evaluated generation, highest
        // raw fitness first, paired with that fitness (genome->fitness is
        // rescaled by fitness sharing, so don't rely on it).
        typedef std::pair<double, GenomeP> RankedGenome;
        const std::vector<RankedGenome> &topGenomes() const {
            return top;
        }

        // How many genomes topGenomes() keeps (default 10)
        void setKeepTop(int n) { keepTop = n; }


    private:
        ExpParameters *P;
        FitnessFunction* fitnessF;
        int generation;

        InnovationStore *IS;

        void runFitness(double &seconds) const;
        void speciate(const GenomeP& g, specieVec &sv);
        void adaptSpeciesThresh(const int specieSize);

        void recordStatistics(double evalSeconds);

        int nextSpecieId = 0;

        std::vector<GenerationStats> stats;
        std::vector<RankedGenome> top;
        int keepTop = 10;

        genomeVec population;

        specieVec species;

        GenomeP maxFitI;

};

#endif
