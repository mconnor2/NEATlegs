#include "Genome.h"


/** 
 * Create default link setup based on number of inputs and outputs.
 */
Genome::Genome (ExpParameters *_P) : P(_P) {
    nLinks = P->nInput * P->nOutput;
    int nNeurons = nInput + nOutput;
    
    links = new Link[nLinks];
    int linkID = 0;
    for (int inID = 0; inID < P->nInput; ++inID) {
	for (int outID = P->nInput; outID < nNeurons; ++outID) {
	    link->inID = inID;
	    link->inNode = NULL;
	    link->outID = outID;
	    link->outNode = NULL;
	    
	    //Give it an initial random weight (Gaussian?)
	    link->weight = rand_gauss();

	    //And it starts enabled
	    link->enabled = true;

	    link->innov = linkID++;
	}
    }
}
	
Genome::~Genome() {
    delete [] links;
}

Genome *Genome::mate(const Genome* parent2) const {

}

void Genome::mutate() {

}

Network *Genome::createNewNetwork() const {

}
