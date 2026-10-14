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

    double compatGDiff;                 //Weight of the number of genes
                                        // that don't match (c1, c2)
    double compatWDiff;                 //Weight of the mean weight
                                        // difference of matching genes (c3)

    double compatThresh;                //Compatability threshold for members
                                        // of the same specie

    int targetSpecies;                  //Desired number of species for
                                        // adaptation, -1 turn off adaptation
    double threshAdapt;                 //Amount to change compatThresh with
                                        // specie number adaptation

    double singleMate;                  //Chance a child comes from one
                                        // parent, by mutation alone

    double specieMate;                  //Chance a crossover's second parent
                                        // is from the same specie (otherwise
                                        // from another)

    int oldAge;                         //Age after which young species are
                                        // not safe from culling

    int dropoffAge;                     //A species whose best fitness hasn't
                                        // improved for this many generations
                                        // stops reproducing (0 = never)

    double survivalThreshold;           //Fraction of each species (its
                                        // fittest) that breeds

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

        // The fittest member of each species of the most recently
        // evaluated generation, with its raw fitness, fittest species first
        struct SpecieChampion {
            int specieId;
            double fitness;
            GenomeP genome;
        };
        const std::vector<SpecieChampion> &speciesChampions() const {
            return champions;
        }


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
        std::vector<SpecieChampion> champions;
        int keepTop = 10;

        genomeVec population;

        specieVec species;

        GenomeP maxFitI;

};

#endif
