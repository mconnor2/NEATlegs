#include "GeneticAlgorithm.h"

#include <algorithm>
#include <iterator>
#include <iostream>
#include <stdexcept>

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
    compatThresh(3.0), targetSpecies(-1), threshAdapt(0.1),
    singleMate(0.2), specieMate(0.99), oldAge(5), dropoffAge(15),
    survivalThreshold(0.2), startPopulationPercent(0.5),
    mirrorInitTied(1.0), mirrorStructure(1.0), untieRate(0.0)
{ }

int ExpParameters::loadFromFile(const libconfig::Config &config) {
    if (!config.exists("global")) {
        cerr<<"ExpParameters::loadFromFile config file must have "
              "'global' section."<<endl;

        return 0;
    }

    //Hard code in reading of values
    if (!(config.lookupValue("global.popSize", popSize)
          && config.lookupValue("global.nInput", nInput)
          && config.lookupValue("global.nOutput", nOutput)))
    {
        cerr<<"ExpParameters::loadFromFile missing one of popSize, "
              "nInput, nOutput"<<endl;
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
        config.lookupValue("global.dropoffAge",dropoffAge);
        config.lookupValue("global.survivalThreshold",survivalThreshold);
        config.lookupValue("global.startPopulationPercent",
                            startPopulationPercent);
        config.lookupValue("global.mirrorInitTied",mirrorInitTied);
        config.lookupValue("global.mirrorStructure",mirrorStructure);
        config.lookupValue("global.untieRate",untieRate);
    } catch (libconfig::SettingTypeException e) {
        cerr<<"ExpParameters::loadFromFile SettingTypeException "
              "while loading parameters"<<endl;
        return 0;
    } catch (libconfig::SettingNotFoundException e) {
        cerr<<"ExpParameters::loadFromFile SettingNotFoundException "
              "while loading parameters"<<endl;
        return 0;
    } catch (...) {
        cerr<<"ExpParameters::loadFromFile some other exception "
              "while loading parameters"<<endl;
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
    if (!P->mirror.empty()) {
        int nNeurons = P->nInput + P->nOutput;
        bool ok = (int)P->mirror.size() == nNeurons;
        for (int i = 0; ok && i < nNeurons; ++i) {
            int m = P->mirror[i];
            ok = m >= 0 && m < nNeurons && P->mirror[m] == i &&
                 (m < P->nInput) == (i < P->nInput);
        }
        if (!ok)
            throw invalid_argument("GeneticAlgorithm: mirror map must pair "
                                   "inputs with inputs and outputs with "
                                   "outputs, and be its own inverse");
    }

    int initialPopulation = P->startPopulationPercent * P->popSize;
//    cout<<"Start population = "<<P->popSize<<"*"
//                             <<P->startPopulationPercent<<endl;
    population.reserve(initialPopulation);
    for (int i = 0; i<initialPopulation; ++i) {
        GenomeP g(new Genome(P));
        population.push_back(g);

        speciate(g, species);
    }
    generation = 0;

//    cout<<"Genetic Algorithm initialization: population "<<population.size()
//      <<", species "<<species.size()<<endl;

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

/**
 * Split total offspring between species in proportion to their fitness
 * (summed shared fitness, i.e. mean raw fitness), rounding by largest
 * remainder so the counts add up to total.  The species holding the
 * champion gets at least one, so the champion carries over.
 */
static vector<int> allocateOffspring(const vector<SpecieP> &sv, int total,
                                     const GenomeP &champion)
{
    size_t n = sv.size();
    vector<int> counts(n, 0);
    if (n == 0) return counts;

    double sum = 0;
    for (const SpecieP &sp : sv) sum += max(sp->fitness, 0.0);

    vector<pair<double, size_t> > remainders;
    int given = 0;
    for (size_t i = 0; i < n; ++i) {
        double exact = sum > 0 ? total * max(sv[i]->fitness, 0.0) / sum
                               : (double)total / n;
        counts[i] = (int)exact;
        given += counts[i];
        remainders.push_back(make_pair(exact - counts[i], i));
    }
    stable_sort(remainders.begin(), remainders.end(),
                [](const pair<double, size_t> &a,
                   const pair<double, size_t> &b) {
                    return a.first > b.first;
                });
    for (size_t k = 0; given < total; k = (k+1) % n) {
        ++counts[remainders[k].second];
        ++given;
    }

    for (size_t i = 0; i < n; ++i) {
        if (counts[i] == 0 && sv[i]->contains(champion)) {
            --*max_element(counts.begin(), counts.end());
            ++counts[i];
        }
    }
    return counts;
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

    double sum = 0, sumSq = 0, sumHidden = 0, sumLinks = 0, sumEnergy = 0,
           sumDistance = 0, sumTied = 0;
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
        sumDistance += g->distance;
        if (g->numLinks() > 0)
            sumTied += (double)g->numTiedLinks() / g->numLinks();
    }
    st.meanFitness = sum / n;
    st.stdevFitness = sqrt(max(0.0, sumSq / n - st.meanFitness*st.meanFitness));
    st.meanHiddenNodes = sumHidden / n;
    st.meanEnabledLinks = sumLinks / n;
    st.meanEnergy = sumEnergy / n;
    st.meanSteps = (double)totalSteps / n;
    st.meanDistance = sumDistance / n;
    st.tiedFraction = sumTied / n;
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

    champions.clear();
    for (const SpecieP &sp : species) {
        SpecieStats ss;
        ss.id = sp->id;
        ss.age = sp->age;
        ss.size = sp->members.size();
        ss.maxFitness = -1e300;
        double specieSum = 0;
        GenomeP champ;
        for (const GenomeP &g : sp->members) {
            specieSum += g->fitness;
            if (!champ || g->fitness > ss.maxFitness) champ = g;
            ss.maxFitness = max(ss.maxFitness, g->fitness);
        }
        ss.meanFitness = ss.size ? specieSum / ss.size : 0;
        st.species.push_back(ss);
        if (champ) champions.push_back({sp->id, champ->fitness, champ});
    }
    stable_sort(champions.begin(), champions.end(),
                [](const SpecieChampion &a, const SpecieChampion &b) {
                    return a.fitness > b.fitness;
                });

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
    st.bestSteps = top[0].second->steps;
    st.bestDistance = top[0].second->distance;

    stats.push_back(st);
}


/**
 * Run one iteration of genetic algorithm, creating new generation and
 *  deleting previous.
 *
 * Process, following Stanley & Miikkulainen (2002), section 3.3:
 * -Evaluate every genome, and record statistics
 * -Share fitness within each species (divide by species size)
 * -Drop species that have stagnated (no improvement in dropoffAge
 *  generations) or that are old and down to one member, unless they hold
 *  the population's champion
 * -Give each species offspring in proportion to its summed shared fitness
 * -Each species' champion carries over unchanged; its other offspring are
 *  bred from its fittest survivalThreshold members, by mutation alone
 *  (singleMate) or crossover, occasionally with another species
 * -Speciate the offspring against the carried-over champions
 * -Return max fitness
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
    for_each(species.begin(), species.end(),
             mem_fn(&Specie::updateStagnation));

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
    sumFit = 0;
    for (genome_it gi = population.begin(); gi != population.end(); ++gi)
        sumFit += (*gi)->fitness;

    species.remove_if([&](const SpecieP &s) {
        if (P->dropoffAge > 0 && s->staleness >= P->dropoffAge &&
            !s->contains(maxFitI))
            return true;
        return s->cull(P->oldAge, sumFit, maxFitI);
    });

    #ifdef _DEBUG_PRINT
        cout<<"Fitness values: ";
        for (genome_it gi = population.begin(); gi != population.end(); ++gi) {
            cout<<(*gi)->fitness<<", ";
        }
        cout<<endl;
    #endif

    vector<SpecieP> breeding(species.begin(), species.end());
    vector<int> nOffspring = allocateOffspring(breeding, P->popSize, maxFitI);

    //Fittest first, so the parents are the first nParents members
    for (const SpecieP &sp : breeding) {
        stable_sort(sp->members.begin(), sp->members.end(),
                    [](const GenomeP &a, const GenomeP &b) {
                        return a->fitness > b->fitness;
                    });
    }
    auto nParents = [&](const SpecieP &sp) {
        return max(1, (int)ceil(P->survivalThreshold * sp->members.size()));
    };
    auto parent = [&](const SpecieP &sp) {
        return sp->members[rand_int() % nParents(sp)];
    };

    genomeVec nextGen;
    nextGen.reserve(P->popSize);

    specieVec nextGenSpecies;

    IS->newGeneration();

    //Champions carry over, and represent their species next generation.
    // All are placed before any child is speciated.
    for (size_t si = 0; si < breeding.size(); ++si) {
        if (nOffspring[si] == 0) continue;
        const SpecieP &sp = breeding[si];
        nextGen.push_back(sp->members[0]);

        SpecieP ns(new Specie(sp->id, sp->age + 1));
        ns->bestFitness = sp->bestFitness;
        ns->staleness = sp->staleness;
        ns->addMember(sp->members[0]);
        nextGenSpecies.push_back(ns);
    }

    for (size_t si = 0; si < breeding.size(); ++si) {
        const SpecieP &sp = breeding[si];
        for (int k = 1; k < nOffspring[si]; ++k) {
            GenomeP child;
            if (rand_double() < P->singleMate) {
                child = parent(sp)->singleMate(IS);
            } else {
                GenomeP p1 = parent(sp), p2;
                if (breeding.size() > 1 && rand_double() >= P->specieMate) {
                    //Interspecies mating
                    size_t other = rand_int() % (breeding.size() - 1);
                    if (other >= si) ++other;
                    p2 = parent(breeding[other]);
                } else {
                    p2 = parent(sp);
                }

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
        }
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
