#include "Network.h"

#include <cmath>
#include <iostream>
#include <map>

#include "random.h"

/**
 * When mating, create new links that are copies of its parents links.
 */
void Link::copy (const Link &l, const double enableRate) {
    innov = l.innov;
    inID = l.inID;
    outID = l.outID;
    weight = l.weight;
    enabled = l.enabled;
    //With some probability enable disabled genes
    if (!enabled && rand_double() < enableRate)
	enabled = true;
}
/**
 * Don't just create copy of parents links, but average the weights
 * of two parent's shared link.
 */
void Link::copy (const Link &l1, const Link &l2, const double enableRate) {
    innov = l1.innov; //l1 and l2 innov should match
    inID = l1.inID;
    outID = l1.outID;
    weight = (l1.weight + l2.weight) / 2.0;
    enabled = l1.enabled; // && l2.enabled;
    //With some probability enable disabled genes
    if (!enabled && rand_double() < enableRate)
	enabled = true;
}
    
void Link::printLink() const {
    std::cout<<"Innov "<<innov<<": "<<inID<<"->"<<outID
	     <<": weight "<<weight;
    if (!enabled) std::cout<<" (!)";
}

double Network::sigmoid (double x) {
    return 1.0/(1.0+exp(-3.0*x));
}

/**
 * Build the network from a genome's links.  Neuron IDs in the genome need
 * not be sequential, so they're mapped to indices: inputs first, then
 * outputs, then hidden neurons in order of first appearance.  Neurons only
 * referenced by disabled links still get an index (so settling treats
 * them as it always has); disabled links themselves are dropped.
 */
Network::Network (int _nInput, int _nOutput,
		  const Link *genome, int geneLength) :
		  nInput(_nInput), nOutput(_nOutput)
{
    std::map<int, int> index;
    int next = 0;
    for (int i = 0; i < nInput + nOutput; ++i) index[i] = next++;
    for (int i = 0; i < geneLength; ++i) {
	if (!index.count(genome[i].inID)) index[genome[i].inID] = next++;
	if (!index.count(genome[i].outID)) index[genome[i].outID] = next++;
    }

    neurons.resize(next);
    for (int i = 0; i < nInput; ++i) neurons[i].active = true;

    for (int i = 0; i < geneLength; ++i) {
	if (!genome[i].enabled) continue;
	conns.push_back({index[genome[i].inID], index[genome[i].outID],
			 genome[i].weight});
    }
}

bool Network::allActive () const {
    for (size_t i = nInput; i < neurons.size(); ++i)
	if (!neurons[i].active) return false;
    return true;
}

/**
 * One propagation step.  While settling, only neurons that have received
 * input pass it on and get an activation.
 */
void Network::propagate (bool settling) {
    for (const Connection &c : conns) {
	Neuron &from = neurons[c.from], &to = neurons[c.to];
	if (settling) {
	    if (!from.active) continue;
	    to.active = true;
	}
	to.inputSum += from.activation * c.weight;
    }
    for (size_t i = nInput; i < neurons.size(); ++i) {
	Neuron &n = neurons[i];
	if (settling && !n.active) continue;
	n.activation = sigmoid(n.inputSum);
	n.inputSum = 0;
    }
}

void Network::run (const double input[], double output[]) {
    for (int i = 0; i < nInput; ++i) neurons[i].activation = input[i];

    if (!initialized) {
	//Signals travel one link per step, so every reachable neuron is
	// active after at most (number of neurons) steps
	int steps = 0;
	do {
	    propagate(true);
	} while (!allActive() && ++steps < (int)neurons.size());
	initialized = true;
    } else {
	propagate(false);
    }

    for (int i = 0; i < nOutput; ++i)
	output[i] = neurons[nInput + i].activation;
}
