#include <iostream>

#include "random.h"
#include "Network.h"
#include "Genome.h"
#include "GeneticAlgorithm.h"

using namespace std;

/**
 * Rudimentary test of NEAT using MAX fitness function, just trying to
 *  create the network that gives the largest output to all 1 input.  
 *  If all goes correctly, should find networks keep increasing weights.
 */

class maxOutputTest : public unary_function<const GenomeP, double> {
    public:
	double operator()(const GenomeP &g) {
	    auto_ptr<Network> N(g->createNewNetwork());

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
   
    maxOutputTest fit;

    GeneticAlgorithm<maxOutputTest> *GA = 
	new GeneticAlgorithm<maxOutputTest>(&P, &fit);

    double maxFit = -1e9, curMaxFit = 0;
    
    cout<<"Generation 0"<<endl;
    GA->printPopulation();

    for (int gen = 0; gen < 10; gen++) {
	curMaxFit = GA->nextGeneration();
	if (curMaxFit > maxFit) maxFit = curMaxFit;
	cout<<"  After generation "<<gen<<", maximum fitness =  "<<maxFit<<endl;
	cout<<"========================================================="<<endl;
	cout<<"Generation "<<gen+1<<endl;
	GA->printPopulation();
    }
    return 0;
}
