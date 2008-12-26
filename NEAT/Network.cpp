#include "Network.h"

#include <math.h>
#include "random.h"

#include <iostream>
#include <map>

/**
 * When mating, create new links that are copies of its parents links.
 */
void Link::copy (const Link &l, const double enableRate) {
    innov = l.innov;
    inID = l.inID;
    outID = l.outID;
    inNode = outNode = NULL;
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
    inNode = outNode = NULL;
    weight = (l1.weight + l2.weight) / 2.0;
    enabled = l1.enabled; // && l2.enabled;
    //With some probability enable disabled genes
    if (!enabled && rand_double() < enableRate)
	enabled = true;
}
    
void Link::printLink() {
    std::cout<<"Innov "<<innov<<": "<<inID<<"->"<<outID
	     <<": weight "<<weight;
    if (!enabled) std::cout<<" (!)";
}

/**
 * Initialize a random base network with specified number of sensor input
 *  nodes and number of output nodes.  Each output node will be connected to 
 *  every sensor input with a randomly weighted connection.
 *
 *  In terms of innovation numbers of links, they will be constructed in a 
 *  fixed order so all networks created with this number of inputs and outputs
 *  will have same structure/innovation numbers
 */
/*
Network::Network (int _nInput, int _nOutput) {
    nInput = _nInput;
    nOutput = _nOutput;
    nHidden = 0;

    nNeurons = nInput + nOutput;
    nLinks = nInput * nOutput;

    neurons = new Neuron[nNeurons];
    links = new Link[nLinks];

    //Initialize neurons to sensor and output
    Neuron *n = neurons[0];
    for (int i = 0; i<nNeurons; ++i) {
	n->ID = i;
	if (i < nInput) {
	    n->type = SENSOR;
	    n->active = true;
	} else {
	    n->type = OUTPUT;
	    n->active = false;
	}

	++n;
    }

    //Create link structure between each input and output
    Link *link = &links[0];
    int linkID = 0;
    for (int inID = 0; inID < nInput; ++inID) {
	for (int outID = nInput; outID < nNeurons; ++outID) {
	    link->inID = inID;
	    link->inNode = &neurons[inID];
	    link->outID = outID;
	    link->outNode = &neurons[outID];
	    
	    //Give it an initial random weight (Gaussian?)
	    link->weight = rand_gauss();

	    //And it starts enabled
	    link->enabled = true;

	    link->innov = linkID++;
	    link++;
	}
    }
    
    initialized = false;
}
*/

/**
 * Create a network using a previously defined links genome.
 *
 * genome will be copied to links, and then the network structure will 
 * be built (pointers of links filled out).  First the number of hidden
 * nodes will need to be found from the link genome to calculate the total
 * number of neurons necessary (nInput + nOutput + nHidden)
 *
 * Since the links is basically the DNA of the Network, it is not created by
 * the network, but rather it creates the network.  Thus the network should
 * not destroy it when it is finished.
 */
Network::Network (int _nInput, int _nOutput, 
		  Link* genome, int geneLength) {
    nInput = _nInput;
    nOutput = _nOutput;

    links = genome;
    nLinks = geneLength;

    //Since neurons do not need to be sequential in the genome,
    // but we would rather process the network as if they were,
    // we'll create a mapping from each links neuron ID to real
    // neuron ID.
    int nextID = 0;
    std::map<int, int> neuronIDMap;
    
    //First make sure input and output IDs stay the same
    for (int i = 0; i< nInput; i++) 
	neuronIDMap.insert(make_pair(i, nextID++));
    
    for (int i = 0; i< nOutput; i++) 
	neuronIDMap.insert(make_pair(i+nInput, nextID++));
    
    //Now find the number of total neurons referred to in genome
    for (int i = 0; i<nLinks; ++i) {
	if (neuronIDMap.count(links[i].inID) == 0)
	    neuronIDMap.insert(make_pair(links[i].inID, nextID++));
	
	if (neuronIDMap.count(links[i].outID) == 0)
	    neuronIDMap.insert(make_pair(links[i].outID, nextID++));
    }

    nNeurons = nextID;
    nHidden = nNeurons - (nInput + nOutput);
    
    neurons = new Neuron[nNeurons];

    //Initialize sensor and output neurons (the rest should default to hidden
    Neuron *n = &neurons[0];
    for (int i = 0; i<nInput+nOutput; ++i) {
	n->ID = i;
	if (i < nInput) {
	    n->type = SENSOR;
	    n->active = true;
	} else {
	    n->type = OUTPUT;
	    n->active = false;
	}

	++n;
    }

    //Now go through links and fill in neuron pointers
    // use the neuronIDMap to map from link neuron ID to index into
    // neurons array.
    for (int i = 0; i<nLinks; ++i) {
	links[i].inNode = &neurons[ neuronIDMap[links[i].inID] ];
	links[i].outNode = &neurons[ neuronIDMap[links[i].outID] ];
    }
    
    initialized = false;
}

Network::~Network () {
    delete [] neurons;
    //delete [] links;
}

bool Network::allActive () {
    for (int i = nInput; i<nNeurons; ++i) {
	if (!neurons[i].active) return false;
    }
    return true;
}

void Network::run (double inputs[], double outputs[]) {
    //setup sensor neurons with the inputs
#ifdef _DEBUG_PRINT
    std::cout<<"Running network on input:";
#endif
    for (int i = 0; i<nInput; i++) {
	neurons[i].activation = inputs[i];
#ifdef _DEBUG_PRINT
	std::cout<<" "<<inputs[i];
#endif
    }
#ifdef _DEBUG_PRINT
	std::cout<<endl;
#endif

    if (!initialized) {
	#ifdef _DEBUG_PRINT
	    std::cout<<"  Network not initialized, making sure all nodes are active."<<endl;
	#endif

	//First time through lets run it til all nodes receive at least
	// some active input
	do {
	    //At the start of each round, assume inputSum is zeroed
	    
	    #ifdef _DEBUG_PRINT
		std::cout<<"  Propogating forward..."<<endl;
	    #endif

	    //Go over links and if link is enabled and input node
	    // activated, propogate signal
	    for (int lid = 0; lid<nLinks; lid++) {
		if (!links[lid].enabled || 
		    !links[lid].inNode->active)
		    continue;
		links[lid].outNode->inputSum +=
		    links[lid].inNode->activation * links[lid].weight;
		links[lid].outNode->active = true;
	    	
		#ifdef _DEBUG_PRINT
		    std::cout<<"  Link #"<<lid<<": ("
			     <<links[lid].inID<<") "
			     <<links[lid].inNode->activation<<" * "
			     <<links[lid].weight<<" -> ("
			     <<links[lid].outID<<") "
			     <<links[lid].outNode->inputSum<<endl;
		#endif
	    }

	    #ifdef _DEBUG_PRINT
		std::cout<<"  Computing node activations:"<<endl;
	    #endif

	    //Now go over neurons to propogate input to sigmoid activation
	    // also clear inputSum for the next step
	    Neuron *n = &neurons[nInput];
	    for (int nid = nInput; nid < nNeurons; ++nid, ++n) {
		if (!n->active)
		    continue;
		n->activation = n->sigmoid(n->inputSum);
		
		#ifdef _DEBUG_PRINT
		    std::cout<<"  Node "<<nid<<": f("<<n->inputSum<<") = "
			     <<n->activation<<endl;
		#endif
		
		n->inputSum = 0;
	    }
	} while (!allActive());
	initialized = true;
    } else {
	//Just need to make one pass as above
	// and no need to check activation
	
	#ifdef _DEBUG_PRINT
	    std::cout<<"  Propogating forward one cycle..."<<endl;
	#endif
	
	//At the start of each round, assume inputSum is zeroed
	
	//Go over links and if link is enabled propogate signal
	for (int lid = 0; lid<nLinks; lid++) {
	    if (!links[lid].enabled)
		continue;
	    links[lid].outNode->inputSum +=
		links[lid].inNode->activation * links[lid].weight;
	    
	    #ifdef _DEBUG_PRINT
		std::cout<<"  Link #"<<lid<<": ("
			 <<links[lid].inID<<") "
			 <<links[lid].inNode->activation<<" * "
			 <<links[lid].weight<<" -> ("
			 <<links[lid].outID<<") "
			 <<links[lid].outNode->inputSum<<endl;
	    #endif
	}

	//Now go over neurons to propogate input to sigmoid activation
	// also clear inputSum for the next step
	#ifdef _DEBUG_PRINT
	    std::cout<<"  Computing node activations:"<<endl;
	#endif
	Neuron *n = &neurons[nInput];
	for (int nid = nInput; nid < nNeurons; ++nid, ++n) {
	    n->activation = n->sigmoid(n->inputSum);
	    
	    #ifdef _DEBUG_PRINT
		std::cout<<"  Node "<<nid<<": f("<<n->inputSum<<") = "
			 <<n->activation<<endl;
	    #endif
	    
	    n->inputSum = 0;
	}
    }
    
    #ifdef _DEBUG_PRINT
	std::cout<<"Network output:";
    #endif

    //Now copy output neurons activation to output array
    Neuron *n = &neurons[nInput];
    for (int i = 0; i<nOutput; ++i, ++n) {
	outputs[i] = n->activation;
	
	#ifdef _DEBUG_PRINT
	    std::cout<<" "<<n->activation;
	#endif
    }
    #ifdef _DEBUG_PRINT
	std::cout<<endl<<endl;
    #endif
}
