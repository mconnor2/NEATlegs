#include "InnovationStore.h"

#include "GeneticAlgorithm.h"


InnovationStore::InnovationStore (ExpParameters *_P) : P(_P) {
    //These will both be based on the default Genome construction with fully
    // connected input->output network.
    nextNeuronID = P->nInput+P->nOutput;
    nextInnov = P->nInput*P->nOutput;
}
	
void InnovationStore::newGeneration() {
   newLinks.clear();
   newNodes.clear();
}
	
bool InnovationStore::addLink(int inNode, int outNode, 
			      int &newInnov) 
{
    newLink l = make_pair(inNode, outNode);
    if (newLinks.count(l) > 0) {
	newInnov = newLinks[l];
	return true;
    }
    newInnov = nextInnov++;
    newLinks.insert(make_pair(l, newInnov));
    return false;
}

bool InnovationStore::addNode(int linkInnov,
		     int &newPreInnov, int &newPostInnov, int &newNeuron) 
{
    if (newNodes.count(linkInnov) > 0) {
	boost::shared_ptr<newNode> nodeInfo = newNodes[linkInnov];
	
	newPreInnov = nodeInfo->preInnov;
	newPostInnov = nodeInfo->postInnov;
	newNeuron = nodeInfo->newNeuron;
	return true;
    }
    newPreInnov = nextInnov++;
    newPostInnov = nextInnov++;
    newNeuron = nextNeuronID++;
    
    newNodes.insert( make_pair(linkInnov, 
	boost::shared_ptr<newNode>(new newNode(newPreInnov, newPostInnov,
					       newNeuron))));
    return false;
}
