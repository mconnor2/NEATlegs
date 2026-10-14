#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <ctime>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <atomic>
#include <functional>
#include <mutex>
#include <thread>

#include <atomic>
#include <functional>
#include <mutex>
#include <thread>
#include <memory>

#include <libconfig.h++>


#include "NEAT/random.h"
#include "NEAT/Network.h"
#include "NEAT/Genome.h"
#include "NEAT/GeneticAlgorithm.h"
#include "NEAT/RunLog.h"

#include "Display.h"
#include "StatsOverlay.h"
#include "BoxScreen.h"
#include "World.h"
#include "Creature.h"
#include "CreatureSpec.h"
#include "Simulation.h"

using namespace std;

const int Width  = 640;
const int Height = 480;

// Colors for the champions of the second, third, ... fittest species when
// several are shown together (the fittest keeps the normal colors), dim so
// the best stands out
static const Color SpeciesColors[] = {
    0x4F86C6FF, 0xC9A23FFF, 0xA35BA8FF, 0x3FA3A0FF, 0xC66A3FFF, 0x8C8C8CFF
};

static Color speciesColor (int rank) {
    if (rank == 0) return BodyColor;
    const int n = sizeof(SpeciesColors) / sizeof(SpeciesColors[0]);
    return SpeciesColors[(rank - 1) % n];
}

/**
 * Fitness for a creature: how far forward its head gets (max head x) in one
 * episode, optionally shaped by how long it stays up (FitnessOptions).
 * Runs are defined by the creature spec and episode options
 * (Simulation.h); this adds the network controller, the objective, and
 * optional real-time drawing.
 */
class hopper {
    public:
        hopper(const CreatureSpec *_spec, const EpisodeOptions &_opt,
               const FitnessOptions &_fopt) :
            spec(_spec), opt(_opt), fopt(_fopt) { }

        // With a display, the run is drawn in real time and overlay (if set)
        // is called each frame to draw on top of the simulation.  Any key
        // ends the episode; quit is left for the caller to see via
        // display->quitRequested().
        double operator()(const GenomeP &g, Display *display = nullptr,
                          const function<void(Display &)> &overlay = nullptr)
            const
        {
            EpisodeResult r = episode(g, display, overlay);
            g->steps = r.steps;
            g->energy = r.energy;
            g->distance = r.maxHeadX;
            return (g->fitness = fitness(r));
        }

        double fitness (const EpisodeResult &r) const {
            return episodeFitness(r, opt.maxSteps, fopt);
        }

        // One episode of g's network, drawn if there's a display
        EpisodeResult episode (const GenomeP &g, Display *display = nullptr,
                               const function<void(Display &)> &overlay =
                                   nullptr) const
        {
            unique_ptr<Network> N(g->createNewNetwork());
            Controller controller = [&](const double *in, double *out) {
                N->run(in, out);
            };

            // 100 pixels a meter
            BoxScreen s(display, 100.0f);
            FrameCallback frame;
            if (display) frame = [&](const Simulation &sim) {
                display->clear();
                if (overlay) overlay(*display);
                s.keepViewable(sim.head());
                s.drawGrid();
                sim.draw(s);
                display->present();
                if (display->poll() != DisplayEvent::None) return false;
                display->waitFrame();
                return true;
            };

            return runEpisode(*spec, opt, controller, frame);
        }

        // Episodes of several genomes at once, each in its own world, drawn
        // over each other: the first in the normal colors, followed by the
        // camera, the rest tinted (speciesColor) behind it.  Each steps
        // exactly as in a single episode, and one that has ended stays
        // where it stopped.  Runs until all have ended or a key is pressed.
        void together (const vector<GenomeP> &gs, Display &display,
                       const function<void(Display &)> &overlay) const
        {
            vector<unique_ptr<Network>> nets;
            vector<unique_ptr<Simulation>> sims;
            for (const GenomeP &g : gs) {
                nets.emplace_back(g->createNewNetwork());
                sims.emplace_back(new Simulation(*spec, opt));
            }
            vector<double> in(spec->numInputs()),
                           out(spec->numOutputs(), 0.0);
            BoxScreen s(&display, 100.0f);
            for (bool running = true; running; ) {
                running = false;
                for (size_t i = 0; i < sims.size(); ++i) {
                    Simulation &sim = *sims[i];
                    if (!sim.running()) continue;
                    sim.readSensors(in.data());
                    nets[i]->run(in.data(), out.data());
                    sim.applyOutputs(out.data());
                    running |= sim.step();
                }

                display.clear();
                if (overlay) overlay(display);
                s.keepViewable(sims[0]->head());
                s.drawGrid();
                for (size_t i = sims.size() - 1; i >= 1; --i) {
                    TintRenderer tint(s, speciesColor(i));
                    sims[i]->creature().draw(tint);
                }
                sims[0]->draw(s);
                display.present();
                if (display.poll() != DisplayEvent::None) return;
                display.waitFrame();
            }
        }

    private:
        const CreatureSpec *spec;
        EpisodeOptions opt;
        FitnessOptions fopt;
};

static void usage () {
    printf("Usage: hopper -C config [options]\n"
           "  -N gens     number of generations (default 1000)\n"
           "  -V          watch evolution: a window replays the fittest\n"
           "              member while the GA keeps running (Space: jump to\n"
           "              newest)\n"
           "  -d gens     with -V, publish a new best to watch every this\n"
           "              many generations (default 10; implies -V)\n"
           "  -n count    with -V, show the champions of the fittest count\n"
           "              species together, the others tinted behind the\n"
           "              best (default 1; implies -V)\n"
           "  -o dir      run output directory (default runs/<config>-<time>)\n"
           "  -s gens     snapshot top genomes every this many generations\n"
           "              (default 10, 0 disables)\n"
           "  -k count    genomes per snapshot (default 3)\n"
           "  -S seed     random seed, to repeat a run exactly (default: from\n"
           "              /dev/urandom; the seed used is printed and saved)\n"
           "  -r genome   replay a saved genome in a window instead of\n"
           "              running the GA\n"
           "  -e genome   evaluate a saved genome once, headless, and print\n"
           "              its fitness, distance, steps, energy (and its\n"
           "              parts) and end reason\n");
}

// Default run directory: runs/<config name>-<YYYYmmdd-HHMMSS>
static string defaultRunDir (const char *configFile) {
    char stamp[32];
    time_t now = time(NULL);
    strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", localtime(&now));
    return "runs/" + std::filesystem::path(configFile).stem().string() +
           "-" + stamp;
}

static GenomeP loadGenome (const char *genomeFile, ExpParameters *P) {
    ifstream in(genomeFile);
    if (!in) {
        cerr<<"Can't open genome file "<<genomeFile<<endl;
        return nullptr;
    }
    return Genome::load(in, P);
}

// Evaluate a saved genome once without a window and print the result
static int evaluate (const hopper &fit, const char *genomeFile,
                     ExpParameters *P)
{
    GenomeP g = loadGenome(genomeFile, P);
    if (!g) return 1;
    EpisodeResult r = fit.episode(g);
    //Full precision, to compare with the fitness saved with the genome
    printf("fitness %.17g distance %.17g steps %d energy %.17g end %s"
           " (positive work %.6g J, negative work %.6g J,"
           " force-time %.6g N s)\n",
           fit.fitness(r), r.maxHeadX, r.steps, r.energy, toString(r.end),
           r.positiveWork, r.negativeWork, r.forceTime);
    return 0;
}

// Replay a saved genome until the window is closed
static int replay (const hopper &fit, const char *genomeFile,
                   ExpParameters *P)
{
    GenomeP g = loadGenome(genomeFile, P);
    if (!g) return 1;

    unique_ptr<Display> display;
    try {
        display.reset(new Display("Hopper Replay", Width, Height));
    } catch (exception &e) {
        cerr<<e.what()<<endl;
        return 1;
    }

    string title = "Replay " +
                   std::filesystem::path(genomeFile).filename().string();
    vector<GenerationStats> noHistory;
    for (int run = 1; !display->quitRequested(); ++run) {
        double f = fit(g, display.get(), [&](Display &d) {
            drawStatsOverlay(d, title, noHistory);
        });
        if (!display->quitRequested())
            printf("Replay %d: fitness %.4f (distance %.4f m, %d steps, "
                   "%.4g J)\n", run, f, g->distance, g->steps, g->energy);
    }
    return 0;
}

/**
 * Shared state between the GA thread and the display (main) thread when
 * watching a run.  The GA publishes a copy of its stats every generation
 * and clones of the best genomes every displayEvery generations: the best
 * one, or each of the fittest species' champions.  The display thread
 * replays the newest published ones over and over.
 */
struct Monitor {
    struct Shown {
        GenomeP genome;         //Clone: replaying writes fitness/steps/energy
                                // onto the genome, which the GA thread reads
        double fitness;
        int specieId;           //-1 if unknown
    };

    std::mutex m;
    vector<Shown> shown;        //Fittest first
    int bestGeneration = -1;
    std::shared_ptr<const vector<GenerationStats>> history;

    std::atomic<bool> stop{false}, done{false};

    void publishStats (const GeneticAlgorithm &GA) {
        //Overlay doesn't need the per-species breakdown
        auto h = std::make_shared<vector<GenerationStats>>(GA.history());
        for (GenerationStats &st : *h) st.species.clear();
        std::lock_guard<std::mutex> lock(m);
        history = h;
    }

    // The best genome, or with nSpecies > 1 the champions of the fittest
    // nSpecies species
    void publishBest (const GeneticAlgorithm &GA, int nSpecies) {
        vector<Shown> s;
        if (nSpecies <= 1) {
            const GeneticAlgorithm::RankedGenome &top = GA.topGenomes().at(0);
            s.push_back({top.second->clone(), top.first, -1});
        } else {
            for (const auto &c : GA.speciesChampions()) {
                if ((int)s.size() == nSpecies) break;
                s.push_back({c.genome->clone(), c.fitness, c.specieId});
            }
        }
        std::lock_guard<std::mutex> lock(m);
        shown = s;
        bestGeneration = GA.history().back().generation;
    }
};

// Evolve on a background thread while the main thread shows the latest
// published best, until the window is closed.  Closing the window stops
// the GA after its current generation.
static void watchEvolution (const hopper &fit, Display &display,
                            GeneticAlgorithm &GA, RunLog &log, int maxGen,
                            int displayEvery, int showSpecies)
{
    Monitor mon;

    std::thread ga([&]() {
        for (int gen = 0; gen < maxGen && !mon.stop; ++gen) {
            GA.nextGeneration();
            log.record(GA);
            mon.publishStats(GA);
            if (gen % displayEvery == 0 || gen == maxGen-1)
                mon.publishBest(GA, showSpecies);
        }
        log.finish(GA);
        mon.done = true;
    });

    while (!display.quitRequested()) {
        vector<Monitor::Shown> shown;
        int shownGen;
        {
            std::lock_guard<std::mutex> lock(mon.m);
            shown = mon.shown;
            shownGen = mon.bestGeneration;
        }

        auto overlay = [&](Display &d) {
            std::shared_ptr<const vector<GenerationStats>> h;
            {
                std::lock_guard<std::mutex> lock(mon.m);
                h = mon.history;
            }
            char title[128];
            const char *status = mon.done ? "run finished, close to exit"
                                          : "evolving";
            if (!shown.empty()) {
                snprintf(title, sizeof(title),
                         "Gen %d best (%.3f)   now: gen %d, %s",
                         shownGen, shown[0].fitness,
                         h && !h->empty() ? h->back().generation : 0, status);
            } else {
                snprintf(title, sizeof(title), "Waiting for generation 0...");
            }
            static const vector<GenerationStats> none;
            drawStatsOverlay(d, title, h ? *h : none);

            //Legend for several species, below the stats
            if (shown.size() > 1) {
                for (size_t i = 0; i < shown.size(); ++i) {
                    char line[64];
                    snprintf(line, sizeof(line), "species %d  %.3f",
                             shown[i].specieId, shown[i].fitness);
                    d.text(10, 96 + 14 * i, line, speciesColor(i));
                }
            }
        };

        if (shown.size() > 1) {
            vector<GenomeP> gs;
            for (const Monitor::Shown &s : shown) gs.push_back(s.genome);
            fit.together(gs, display, overlay);
        } else if (!shown.empty()) {
            fit(shown[0].genome, &display, overlay);
        } else {
            display.clear();
            overlay(display);
            display.present();
            display.poll();
            display.waitFrame();
        }
    }

    mon.stop = true;
    if (!mon.done)
        printf("Window closed, stopping after the current generation...\n");
    ga.join();
}

int main (int argc, char **argv) {
    bool drawGen = false;
    bool haveSeed = false;
    uint64_t seed = 0;
    int displayEvery = 10;
    int showSpecies = 1;

    int maxGen = 1000;

    RunLog::Options logOpt;
    const char *replayFile = NULL;
    const char *evalFile = NULL;

    /* Process arguments */
    int opt;
    char *configFile = NULL;
    while ((opt = getopt(argc, argv, "VC:hN:o:s:k:r:e:d:n:S:")) != -1) {
        switch(opt) {
            case 'V':
                drawGen = true;
            break;
            case 'C':
                configFile = optarg;
            break;
            case 'N':
                maxGen = atoi(optarg);
            break;
            case 'o':
                logOpt.outputDir = optarg;
            break;
            case 's':
                logOpt.snapshotEvery = atoi(optarg);
            break;
            case 'k':
                logOpt.snapshotTop = atoi(optarg);
            break;
            case 'r':
                replayFile = optarg;
            break;
            case 'e':
                evalFile = optarg;
            break;
            case 'S':
                seed = strtoull(optarg, NULL, 10);
                haveSeed = true;
            break;
            case 'd':
                displayEvery = atoi(optarg);
                drawGen = true;
                if (displayEvery < 1) {
                    fprintf(stderr, "-d needs a positive generation count\n");
                    exit(1);
                }
            break;
            case 'n':
                showSpecies = atoi(optarg);
                drawGen = true;
                if (showSpecies < 1) {
                    fprintf(stderr, "-n needs a positive species count\n");
                    exit(1);
                }
            break;
            default:
                usage();
                exit(1);
        }
    }

    if (haveSeed) seed_rand(seed);
    else seed = dev_seed_rand();

    if (!configFile) {
        fprintf(stderr, "Must specify config file.\n");
        usage();
        exit(1);
    }

    libconfig::Config config;
    try {
        config.readFile(configFile);
    } catch (libconfig::ParseException &pe) {
        cerr<<"Config parse error"<<endl;
        cerr<<"   config file "<<configFile<<endl;
        cerr<<"   line number "<<pe.getLine()<<endl;
        cerr<<"   error: "<<pe.getError()<<endl;
        return 1;
    } catch (...) {
        cerr<<"Config error reading from file."<<endl;
        return 1;
    }
    config.setAutoConvert(true);

    ExpParameters P;

    if (!P.loadFromFile(config)) {
        cerr<<"Problem loading ExpParameters, exiting."<<endl;
        exit(1);
    }

    CreatureSpec spec;
    try {
        spec = parseCreatureSpec(config);
    } catch (exception &e) {
        cerr<<configFile<<": "<<e.what()<<endl;
        return 1;
    }
    EpisodeOptions episode;
    FitnessOptions fitnessOpt;
    try {
        episode = episodeOptionsFromConfig(config);
        fitnessOpt = fitnessOptionsFromConfig(config);
        Simulation check(spec, episode);        //Validates head, groundLimbs
    } catch (exception &e) {
        cerr<<configFile<<": "<<e.what()<<endl;
        return 1;
    }
    P.nInput = spec.numInputs();
    P.nOutput = spec.numOutputs();
    P.mirror = spec.neuronMirror();

    hopper fit(&spec, episode, fitnessOpt);

    if (evalFile) return evaluate(fit, evalFile, &P);
    if (replayFile) return replay(fit, replayFile, &P);

    FitnessFunction f = fit;

    unique_ptr<Display> display;
    if (drawGen) {
        try {
            display.reset(new Display("Hopper Test", Width, Height));
        } catch (exception &e) {
            cerr<<e.what()<<endl;
            return 1;
        }
    }

    if (logOpt.outputDir.empty()) logOpt.outputDir = defaultRunDir(configFile);
    logOpt.configPath = configFile;
    logOpt.seed = seed;

    GeneticAlgorithm GA(&P, &f);
    GA.setKeepTop(max(logOpt.snapshotTop, 1));

    unique_ptr<RunLog> log;
    try {
        log.reset(new RunLog(logOpt));
    } catch (exception &e) {
        cerr<<e.what()<<endl;
        return 1;
    }

    printf("Population %d, %d inputs, %d outputs, seed %llu."
           "  Writing run to %s\n",
           P.popSize, P.nInput, P.nOutput, (unsigned long long)seed,
           logOpt.outputDir.c_str());

    if (display) {
        watchEvolution(fit, *display, GA, *log, maxGen, displayEvery,
                       showSpecies);
        return 0;
    }

    for (int gen = 0; gen < maxGen; gen++) {
        GA.nextGeneration();
        log->record(GA);
    }
    log->finish(GA);
    return 0;
}
