#ifndef _NEAT_RUNLOG_H
#define _NEAT_RUNLOG_H

#include <fstream>
#include <string>

#include "NEATtypes.h"

class GeneticAlgorithm;

/**
 * Reports on a GeneticAlgorithm run after every generation:
 *  - one summary line per generation on stdout (header repeated
 *    periodically)
 *  - if an output directory is given:
 *      stats.csv       one row per generation (GenerationStats)
 *      species.csv     one row per species per generation
 *      best.genome     all-time best genome, rewritten when beaten
 *      snapshots/      top genomes every snapshotEvery generations and at
 *                      the end, as genNNNNN_rankK.genome
 *      config.cfg      copy of the experiment config, if given
 */
class RunLog {
    public:
	struct Options {
	    std::string outputDir;	//Empty: console only
	    std::string configPath;	//Copied into outputDir if set
	    int snapshotEvery = 10;	//Generations; 0 disables
	    int snapshotTop = 3;	//Genomes per snapshot
	};

	// Throws std::runtime_error if the output directory can't be used
	explicit RunLog (const Options &opt);

	// Call after each GeneticAlgorithm::nextGeneration()
	void record (const GeneticAlgorithm &GA);

	// Call once after the last generation to snapshot the final top
	// genomes (if the last record() didn't already)
	void finish (const GeneticAlgorithm &GA);

	double bestFitness () const { return bestEver; }

    private:
	void printHeader ();
	void snapshot (const GeneticAlgorithm &GA);
	void saveGenome (const std::string &path, const GenomeP &g,
			 double fitness, int generation, int rank) const;

	Options opt;
	std::ofstream statsCsv, speciesCsv;

	double bestEver;
	int lines = 0;
	int lastSnapshotGen = -1;
};

#endif
