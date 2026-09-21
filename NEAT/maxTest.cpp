#include <iostream>

#include "random.h"
#include "Network.h"
#include "Genome.h"
#include "GeneticAlgorithm.h"
#include "RunLog.h"

using namespace std;

/**
 * Rudimentary test of NEAT using MAX fitness function, just trying to
 *  create the network that gives the largest output to all 1 input.  
 *  If all goes correctly, should find networks keep increasing weights.
 */

class maxOutputTest {
    public:
	double operator()(const GenomeP &g) {
	    unique_ptr<Network> N(g->createNewNetwork());

	    double input[3] = {1,1,1};
	    double output[1] = {0};

	    N->run(input, output);

	    return (g->fitness = output[0]);
	}
};

int main (int argc, char **argv) {
    //set random seed to come from udev random
    dev_seed_rand();

    ExpParameters P;
    //Setup experiment parameters:
    // Keep population small so we can watch the results at first.
    P.popSize = 5;
    
    // Given vector (1,1,1) want to see largest combination weights:
    P.nInput = 3; P.nOutput = 1;

    //Mating probabilities:
    P.inheritAllLinks = false;
    P.inheritDominant = 0.9;
    P.linkEnabledRate = 0.5;

    P.weightMutationRate   = 0.5;
    P.weightPerturbScale   = 0.1;
    P.weightPerturbNormal  = 0.4;
    P.weightPerturbUniform = 0.4;
    
    P.addLinkMutationRate = 0.1;
    P.addNodeMutationRate = 0.1;
   
    FitnessFunction fit = maxOutputTest();

    GeneticAlgorithm GA(&P, &fit);

    RunLog log((RunLog::Options()));

    cout<<"Generation 0"<<endl;
    GA.printPopulation();

    for (int gen = 0; gen < 10; gen++) {
	GA.nextGeneration();
	log.record(GA);
	cout<<"Generation "<<gen+1<<endl;
	GA.printPopulation();
    }
    return 0;
}
