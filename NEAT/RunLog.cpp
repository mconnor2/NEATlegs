#include "RunLog.h"

#include <cstdio>
#include <filesystem>
#include <iostream>
#include <stdexcept>

#include "GeneticAlgorithm.h"
#include "Genome.h"

using namespace std;
namespace fs = std::filesystem;

RunLog::RunLog (const Options &_opt) : opt(_opt), bestEver(-1e300)
{
    if (opt.outputDir.empty()) return;

    fs::path dir(opt.outputDir);
    error_code ec;
    fs::create_directories(dir / "snapshots", ec);
    if (ec)
        throw runtime_error("RunLog: can't create " + dir.string() + ": " +
                            ec.message());

    statsCsv.open(dir / "stats.csv");
    speciesCsv.open(dir / "species.csv");
    if (!statsCsv || !speciesCsv)
        throw runtime_error("RunLog: can't write CSV files in " + dir.string());

    statsCsv<<"generation,population,species,max_fitness,mean_fitness,"
              "min_fitness,stdev_fitness,diversity,compat_thresh,"
              "mean_hidden_nodes,mean_enabled_links,best_hidden_nodes,"
              "best_enabled_links,eval_seconds,steps_per_sec,mean_energy,"
              "best_energy,mean_steps,best_steps,mean_distance,best_distance\n";
    speciesCsv<<"generation,species_id,age,size,mean_fitness,max_fitness\n";

    ofstream(dir / "seed.txt")<<opt.seed<<"\n";

    if (!opt.configPath.empty()) {
        fs::copy_file(opt.configPath, dir / "config.cfg",
                      fs::copy_options::overwrite_existing, ec);
        if (ec)
            cerr<<"RunLog: couldn't copy config: "<<ec.message()<<endl;
    }
}

void RunLog::printHeader () {
    printf("%6s %5s %4s %11s %11s %9s %7s %6s %6s %9s  %s\n",
           "gen", "pop", "spc", "max", "mean", "stdev", "divers",
           "hidden", "links", "energy", "notes");
}

void RunLog::record (const GeneticAlgorithm &GA) {
    if (GA.history().empty()) return;
    const GenerationStats &st = GA.history().back();

    string notes;
    bool newBest = st.maxFitness > bestEver;
    if (newBest) {
        bestEver = st.maxFitness;
        notes += "*best ";
    }

    if (!opt.outputDir.empty()) {
        statsCsv<<st.generation<<","<<st.populationSize<<","<<st.nSpecies<<","
                <<st.maxFitness<<","<<st.meanFitness<<","<<st.minFitness<<","
                <<st.stdevFitness<<","<<st.meanCompat<<","<<st.compatThresh<<","
                <<st.meanHiddenNodes<<","<<st.meanEnabledLinks<<","
                <<st.bestHiddenNodes<<","<<st.bestEnabledLinks<<","
                <<st.evalSeconds<<","<<st.stepsPerSec<<","<<st.meanEnergy<<","
                <<st.bestEnergy<<","<<st.meanSteps<<","<<st.bestSteps<<","
                <<st.meanDistance<<","<<st.bestDistance<<"\n";
        statsCsv.flush();

        for (const SpecieStats &ss : st.species) {
            speciesCsv<<st.generation<<","<<ss.id<<","<<ss.age<<","
                      <<ss.size<<","<<ss.meanFitness<<","<<ss.maxFitness<<"\n";
        }
        speciesCsv.flush();

        if (newBest && !GA.topGenomes().empty()) {
            const GeneticAlgorithm::RankedGenome &b = GA.topGenomes()[0];
            saveGenome((fs::path(opt.outputDir) / "best.genome").string(),
                       b.second, b.first, st.generation, 1);
        }

        if (opt.snapshotEvery > 0 && st.generation % opt.snapshotEvery == 0) {
            snapshot(GA);
            notes += "snap ";
        }
    }

    if (st.stepsPerSec > 0) {
        char rate[32];
        snprintf(rate, sizeof(rate), "%.2fM steps/s", st.stepsPerSec / 1e6);
        notes += rate;
    } else {
        char secs[32];
        snprintf(secs, sizeof(secs), "%.2fs eval", st.evalSeconds);
        notes += secs;
    }

    if (lines++ % 25 == 0) printHeader();
    printf("%6d %5d %4d %11.4f %11.4f %9.4f %7.2f %6.1f %6.1f %9.4g  %s\n",
           st.generation, st.populationSize, st.nSpecies, st.maxFitness,
           st.meanFitness, st.stdevFitness, st.meanCompat,
           st.meanHiddenNodes, st.meanEnabledLinks, st.bestEnergy,
           notes.c_str());
    fflush(stdout);
}

void RunLog::finish (const GeneticAlgorithm &GA) {
    if (GA.history().empty()) return;
    if (!opt.outputDir.empty() && opt.snapshotEvery > 0 &&
        lastSnapshotGen != GA.history().back().generation)
    {
        snapshot(GA);
    }
    printf("Best fitness: %.4f", bestEver);
    if (!opt.outputDir.empty())
        printf("  (run saved to %s)", opt.outputDir.c_str());
    printf("\n");
}

void RunLog::snapshot (const GeneticAlgorithm &GA) {
    int gen = GA.history().back().generation;
    const vector<GeneticAlgorithm::RankedGenome> &top = GA.topGenomes();
    int n = min((int)top.size(), opt.snapshotTop);
    for (int r = 0; r < n; ++r) {
        char name[64];
        snprintf(name, sizeof(name), "gen%05d_rank%d.genome", gen, r+1);
        saveGenome((fs::path(opt.outputDir) / "snapshots" / name).string(),
                   top[r].second, top[r].first, gen, r+1);
    }
    lastSnapshotGen = gen;
}

void RunLog::saveGenome (const string &path, const GenomeP &g,
                         double fitness, int generation, int rank) const
{
    ofstream out(path);
    if (!out) {
        cerr<<"RunLog: can't write "<<path<<endl;
        return;
    }
    out.precision(17);
    out<<"# fitness "<<fitness<<"\n"
       <<"# generation "<<generation<<"\n"
       <<"# rank "<<rank<<"\n";
    g->save(out);
}
