#ifndef _NEAT_STATS_H
#define _NEAT_STATS_H

#include <vector>

/**
 * Per-species summary for one generation.  Fitness values are raw
 * (before fitness sharing divides them by species size).
 */
struct SpecieStats {
    int id;			//Stable across generations
    int age;
    int size;
    double meanFitness;
    double maxFitness;
};

/**
 * Summary of one evaluated generation, recorded by GeneticAlgorithm.
 * Fitness values are raw (before fitness sharing).
 */
struct GenerationStats {
    int generation = 0;
    int populationSize = 0;

    double maxFitness = 0, meanFitness = 0, minFitness = 0;
    double stdevFitness = 0;

    //Diversity
    int nSpecies = 0;
    double meanCompat = 0;	//Mean pairwise compatibility distance
    double compatThresh = 0;	//Species threshold in use this generation

    //Network structure
    double meanHiddenNodes = 0, meanEnabledLinks = 0;
    int bestHiddenNodes = 0, bestEnabledLinks = 0;

    //Evaluation cost.  stepsPerSec is 0 if the fitness function doesn't
    // record Genome::steps.
    double evalSeconds = 0;
    double stepsPerSec = 0;

    std::vector<SpecieStats> species;
};

#endif
