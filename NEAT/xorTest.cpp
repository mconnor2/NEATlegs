#include <iostream>

#include "random.h"
#include "Network.h"
#include "Genome.h"
#include "GeneticAlgorithm.h"

using namespace std;

/**
 * Rudimentary test of NEAT using XOR fitness function
 */

/* Test each network with variable number of applications of XOR. 
 * Standard 3 input (1 reserved for bias), 1 output.
 * For each generation precompute the random inputs so that all members
 * of the generation get same input, but overall can't just memorize
 * sequence.
 */
class xorTest : public unary_function<const Genome*, double> {
    public:
	xorTest (int N) {
	    Rounds = N;
	    xorInput.reserve(2*N);
	    xorOutput.reserve(N);
	    regenerate();

	}

	double operator()(const Genome *g) {
	    auto_ptr<Network> N(g->createNewNetwork());

	    double input[3] = {1,1,1};
	    double output[1] = {0};

	    double sqDiff = 0;

	    const int Passes = 32;

	    for (int i = 0; i<Rounds; ++i) {
		input[0] = xorInput[2*i];
		input[1] = xorInput[2*i+1];
		
		//Instead of making one pass through the network for
		// every input pattern, lets make some P passes (32?)
		// and average the output over each pass, allowing the
		// network to find a steady state.
		double sumOut = 0;
		for (int p = 0; p<Passes; ++p) {
		    N->run(input, output);
		    sumOut += output[0];
		}
		sumOut /= (double)Passes;

		sqDiff += (sumOut - xorOutput[i]) *
			  (sumOut - xorOutput[i]);
	    }

	    //Now return #Rounds - sqDiff, so a perfect xor will have
	    // 0 sqDiff, and hence largest fitness (#Rounds).
	    return (Rounds-sqDiff);
	}
    
	void regenerate () {
	    xorInput.clear();
	    xorOutput.clear();
	    for (int i = 0; i<Rounds; ++i) {
		int a = rand_int()%2,
		    b = rand_int()%2;
		xorInput.push_back(a);
		xorInput.push_back(b);
		xorOutput.push_back(a^b);
	    }
	}

    private:
	int Rounds;

	vector<int> xorInput;
	vector<int> xorOutput;

};

int main (int argc, char **argv) {
    //set random seed to come from udev random
    dev_seed_rand();

    ExpParameters P;
    //Setup experiment parameters:
    // Keep population small so we can watch the results at first.
    P.popSize = 100;
    
    // Given vector (1,1,1) want to see largest combination weights:
    P.nInput = 3; P.nOutput = 1;

    //Mating probabilities:
    P.inheritAllLinks = false;
    P.inheritDominant = 0.9;
    P.linkEnabledRate = 0.1;

    P.weightMutationRate   = 0.5;
    P.weightPerturbScale   = 0.01;
    P.weightPerturbNormal  = 0.4;
    P.weightPerturbUniform = 0.4;
    
    P.addLinkMutationRate = 0.1;
    P.addNodeMutationRate = 0.1;
    
    xorTest fit(100);

    GeneticAlgorithm<xorTest> *GA = 
	new GeneticAlgorithm<xorTest>(&P, &fit);

    double maxFit = -1e9, curMaxFit = 0;
    
    //cout<<"Generation 0"<<endl;
    //GA->printPopulation();

    for (int gen = 0; gen < 1000; gen++) {
	//Each generation will receive a different input, so network
	// can't just memorize pattern
	fit.regenerate();

	curMaxFit = GA->nextGeneration();
	if (curMaxFit > maxFit) maxFit = curMaxFit;
	cout<<"  After generation "<<gen<<", maximum fitness =  "<<maxFit<<endl;
	cout<<"========================================================="<<endl;
	//cout<<"Generation "<<gen+1<<endl;
	//GA->printPopulation();
    }
    return 0;
}
