#ifndef _NEAT_NETWORK_H
#define _NEAT_NETWORK_H

#include <stdlib.h>

using namespace std;

enum NeuronType {
    SENSOR,
    HIDDEN,
    OUTPUT,
    NEURONTYPES
};

/**
 * Single neuron, sums weighted input activation in inputSum in one step,
 *  and then prepares activation = sigmoid(inputSum) for output of next
 *  step.
 */
struct Neuron {
    int ID;
    NeuronType type;
    bool active;
    double inputSum;
    double activation;

    Neuron (int _ID = -1, NeuronType _type=HIDDEN) :
	ID(_ID), type(_type), active(_type==SENSOR) 
    {
	inputSum = 0;
	activation = 0;
    }

    //For now lets just use standard sigmoid, perhaps a little stretched
    inline static double sigmoid (double x) {
	return 1.0/(1.0+exp(-3.0*x));
    }
};

/**
 * Weighted connection between two neurons.
 *
 * Stores both ID and pointer to neurons for potential sorting.
 */
struct Link {
    int innov;
    int inID, outID;
    Neuron* inNode, outNode;
    double weight;
    bool enabled;

    Link (int _inID = -1, Neuron* _in = NULL,
	  int _outID = -1, Neuron* _out = NULL,
	  int _innov = -1, double _w = 0, bool _enabled = false) :
	  inID(_inID), outID(_outID), inNode(_in), outNode(_out),
	  innov(_innov), weight(_w), enabled(_enabled) {}
};

class Network {
    public:
	Network (int nInput, int nOutput);
	Network (int nInput, int nOutput, Link* genome, int geneLength);

	~Network ();

	void run(double input[], double output[]);

    private:
	Link* links;
	Neuron* neurons;
	
	int nLinks, nNeurons;
	//Total number of neurons is sum of three below
	int nSensor, nOutput, nHidden;

	bool initialized;

}

#endif
