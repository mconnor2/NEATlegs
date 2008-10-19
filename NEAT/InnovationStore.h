#ifndef _INNOVATION_STORE_H
#define _INNOVATION_STORE_H

#include <map>
#include <boost/shared_ptr.hpp>

struct ExpParameters;

typedef std::pair<int, int> newLink;

struct newNode {
    int preInnov;
    int postInnov;
    int newNeuron;

    newNode (int _preInnov, int _postInnov, int _newNeuron) :
	preInnov(_preInnov), postInnov(_postInnov), newNeuron(_newNeuron)
	{ }
};

class InnovationStore {
    public:
	InnovationStore (ExpParameters *_P);
	
	//If we only want to store innovations per generation,
	// then clear the innovation cache before creating new genome.
	void newGeneration();
	
	//New link is added between inNode and outNode.  Set newInnov to the
	// proper innovation number for the new link: nextInnov if this link
	// has not been added before, otherwise proper link.
	//
	// Return true if this innovation has been seen before
	bool addLink(int inNode, int outNode, 
		     int &newInnov);

	//New neuron is added, splitting one link into two.  This link is
	// identified by its innovation number, and function determines proper
	// innovation numbers for new pre and post links and neuronID for
	// new neuron.
	bool addNode(int linkInnov,
		     int &newPreInnov, int &newPostInnov, int &newNeuron);

    private:
	ExpParameters *P;

	//Map from link (inNode -> outNode) to innovation id
	std::map<newLink, int> newLinks;
	
	//Map from link (innovation id) to new Node info
	std::map<int, boost::shared_ptr<newNode> > newNodes;

	//For tracking added links and neurons
	int nextNeuronID;
	int nextInnov;
};

#endif
