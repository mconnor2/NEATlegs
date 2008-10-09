#include "Genome.h"
#include "random.h"


/** 
 * Create default link setup based on number of inputs and outputs.
 */
Genome::Genome (ExpParameters *_P) : P(_P) {
    nLinks = P->nInput * P->nOutput;
    int nNeurons = nInput + nOutput;
    
    links = new Link[nLinks];
    Link *link = &links[0];
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
	    link++;
	}
    }
}

/**
 * Given link structure (from mating say), create new genome.
 *  
 */
Genome (Link *_links, int _nLinks, ExpParameters *_P) :
      links(_links), nLinks(_nLinks), P(_P)
{ }
	
Genome::~Genome() {
    delete [] links;
}

/**
 * Go through two parents genomes and form a new genome that is a combination
 *  of the parent's links.  
 *
 *  Assumes the links are sorted by innovation number, which should be the case
 *  if added as mutation occurs.  Also assumes the calling parent (this) is the
 *  more fit parent.
 *
 * For links that occur in only one parent there are three options:
 *  -Take only genes from more fit parent
 *  	if equally fit, randomly from one or the other
 *  -Take all genes from both
 *  -Randomly select each individual gene, including with some high probability
 *
 * 10/08/08, Currently implemented to take all genes from both, should test
 * 	     other options if we see this as totally not working.
 * 
 *
 */
Genome *Genome::mate(const Genome* parent2) const {
    vector<Link *> childLinks;

    int p1i = 0, p2i = 0;
    while (p1i < nLinks or p2i < parent2->nLinks) {
	if (p1i >= nLinks) {
	    //There are still some of parent2's links:
	    childLinks.push_back(new Link(parent2->links[p2i],
					  P->linkEnableRate));
	    ++p2i;
	    continue;
	}
	if (p2i >= parent2->nLinks) {
	    //There are still some of parent1's (this's) links:
	    childLinks.push_back(new Link(links[p1i],
					  P->linkEnableRate));
	    ++p1i;
	    continue;
	}
	//Both parents have links, so lets see if innovation number matches
	if (links[p1i].innov == parent2->links[p2i].innov) {
	    //With some probability take from dominant parent (assume p1)
	    // otherwise create new link that is average of the two.
	    if (rand_double() < P->inheritDominant) {
		childLinks.push_back(new Link(links[p1i],
					      P->linkEnableRate));
	    } else {
		childLinks.push_back(new Link(links[p1i],
					      parent2->links[p2i],
					      P->linkEnableRate));
	    }
	    ++p1i;
	    ++p2i;
	}
	else if (links[p1i].innov < parent2->links[p2i].innov) {
	    childLinks.push_back(new Link(links[p1i],
					  P->linkEnableRate));
	    ++p1i;
	} else {
	    childLinks.push_back(new Link(parent2->links[p2i],
					  P->linkEnableRate));
	    ++p2i;
	}
    }

    return new Genome(childLinks.c_str(), childLinks.size(), P);
}

/**
 * Two types of mutations: weight and structural.
 *
 * Weight mutation: 
 *  For each link, with some probability change the weight:
 * 	-Perturb weight by uniform random amount
 * 	-Perturb weight by normal distributed amount
 * 	-Randomly reset weight to some amount
 *
 * Structural mutation:
 *  -With some probability a node will be added
 *  	-One link is chosen and a new node is added between it.
 *  -With some probability a link between existing nodes will be added
 *  	
 * 10/08/08 For now only implementing weight mutation.
 */
void Genome::mutate() {
    //Go over each link, see if we mutate the weight
    for (int i = 0; i<nLinks; ++i) {
	if (rand_double() > P->weightMutationRate)
	    continue;
	double mutateType = rand_double();
	
	if (mutateType < P->weightPerturbNormal) {
	    //Add random amount sampled from normal distribution with
	    // P->weightPerturbScale variance
	    links[i]->weight += P->weightPerturbScale*rand_gauss();
	} else if (mutateType < weightPerturbNormal+weightPerturbUniform) {
	    //Add random amount sampled from uniform distribution over
	    //[-weightPerturbScale, weightPerturbScale]
	    links[i]->weight += 2*P->weightPerturbScale*rand_double() -
				  P->weightPerturbScale;
	} else {
	    //Randomly set the weight to some random amount in range:
	    //[-weightPerturbScale, weightPerturbScale]
	    links[i]->weight = 2*P->weightPerturbScale*rand_double() -
				 P->weightPerturbScale;
	    //XXX Could also try guassian
	}
    }
}

Network *Genome::createNewNetwork() const {

}
