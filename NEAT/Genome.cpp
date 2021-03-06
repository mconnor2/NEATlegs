#include "Genome.h"
#include "random.h"
#include "Network.h"
#include "GeneticAlgorithm.h"
#include "InnovationStore.h"

#include <iostream>
#include <set>
#include <algorithm>
#include <boost/lambda/lambda.hpp>

using namespace boost::lambda;

/** 
 * Create default link setup based on number of inputs and outputs.
 */
Genome::Genome (ExpParameters *_P) : P(_P) {
    nLinks = P->nInput * P->nOutput;
    nNodes = P->nInput + P->nOutput;

//    cout<<"Genome: nLinks = "<<nLinks<<", nNodes = "<<nNodes<<endl;

    links = new Link[nLinks];
    int linkID = 0;
    for (int inID = 0; inID < P->nInput; ++inID) {
	for (int outID = P->nInput; outID < nNodes; ++outID) {
	    links[linkID].inID = inID;
	    links[linkID].inNode = NULL;
	    links[linkID].outID = outID;
	    links[linkID].outNode = NULL;
	    
	    //Give it an initial random weight (Gaussian?)
	    links[linkID].weight = rand_gauss();

	    //And it starts enabled
	    links[linkID].enabled = true;

	    links[linkID].innov = linkID++;
	}
    }
//    cout<<"Genome: "<<linkID<<" == "<<nLinks<<endl;
}

/**
 * Given link structure (from mating say), create new genome.
 *  
 */
Genome::Genome (Link *_links, int _nLinks, int _nNodes, 
		ExpParameters *_P) :
      links(_links), nLinks(_nLinks), nNodes(_nNodes), P(_P)
{ }
	
Genome::~Genome() {
    delete [] links;
}

void addNodeMutate (Link *childLinks, int &nLink, int &nNodes, 
		    InnovationStore *IS)
{
    //Randomly select a link to disable and replace by two links and a
    // new neuron inside.
    int splitLink = 0;
    do {
	splitLink = rand_int()%nLink;
    } while(!childLinks[splitLink].enabled);
    
    int newNeuronID = -1;

    IS->addNode(childLinks[splitLink].innov,
		childLinks[nLink].innov, //preInnov
		childLinks[nLink+1].innov, //postInnov
		newNeuronID);

    ++nNodes;

    #ifdef _DEBUG_PRINT
	cout<<"  Adding New node:"<<endl;
	cout<<"    Splitting link: ";
	childLinks[splitLink].printLink();
	cout<<endl;
	cout<<"    New neuron id: "<<newNeuronID<<endl;
	cout<<"    New pre link innovation: "<<childLinks[nLink].innov
	    <<", post link: "<<childLinks[nLink+1].innov<<endl;
    #endif

    //Now connect splitLink.in to new neuron
    childLinks[nLink].inID = childLinks[splitLink].inID;
    childLinks[nLink].outID = newNeuronID;
    childLinks[nLink].weight = 1.0;
    childLinks[nLink].enabled = true;

    //And connect new neuron to splitLink.out
    childLinks[nLink+1].inID = newNeuronID;
    childLinks[nLink+1].outID = childLinks[splitLink].outID;
    childLinks[nLink+1].weight = childLinks[splitLink].weight;
    childLinks[nLink+1].enabled = true;
    
    //Old connection is disabled
    childLinks[splitLink].enabled = false;

    nLink+=2;
}
   
void addLinkMutate (std::set<int> &neuronIDs, Link *childLinks,
		    int &nLinks, InnovationStore *IS, 
		    const ExpParameters *P)
{
    //Randomly select input neuron and output neuron from those seen
    // in parents
    vector<int> neurons;
    neurons.insert(neurons.begin(), neuronIDs.begin(), neuronIDs.end());

    int nNeurons = neurons.size();

    //Input neuron can be any node,
    //Output neuron cannot be an input node.
    childLinks[nLinks].inID  = neurons[rand_int()%nNeurons];
    childLinks[nLinks].outID = neurons[rand_int()%(nNeurons-P->nInput)+
				  P->nInput];

    //Use innovation store to find if this innovation has been done before
    // and set the innovation appropriately.
    IS->addLink(childLinks[nLinks].inID, childLinks[nLinks].outID, 
		childLinks[nLinks].innov);
    
    //And set the weight to random gaussian, mean 0, std dev 1
    childLinks[nLinks].weight = rand_gauss();
    childLinks[nLinks].enabled = true;
    
    #ifdef _DEBUG_PRINT
	cout<<"  Adding New link:";
	childLinks[nLinks].printLink();
	cout<<endl;
    #endif
    
    ++nLinks;
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
 *
 * Note that mating creates a new genome, which must be disposed of later.
 *
 * 10/08/08, Currently implemented to take all genes from both, should test
 * 	     other options if we see this as totally not working.
 * 
 *
 */
GenomeP Genome::mate(const GenomeP &parent2, InnovationStore *IS) const {

    int p1i = 0, p2i = 0;
    //Lets do two pass at the mating.  The first pass figures out the size
    // of the child genome, the second creates a new array of links and fills
    // it in with copies of parent links. 

    int nChildLinks = 0;
    while (p1i < nLinks or p2i < parent2->nLinks) {
	if (p1i >= nLinks) {
	    //There are still some of parent2's links:
	    if (P->inheritAllLinks) {
		nChildLinks++;
		++p2i;
		continue;
	    } else {
		break;
	    }
	}
	if (p2i >= parent2->nLinks) {
	    //There are still some of parent1's (this's) links:
	    nChildLinks++;
	    ++p1i;
	    continue;
	}
	//Both parents have links, so lets see if innovation number matches
	if (links[p1i].innov == parent2->links[p2i].innov) {
	    //With some probability take from dominant parent (assume p1)
	    // otherwise create new link that is average of the two.
	    nChildLinks++;
	    ++p1i;
	    ++p2i;
	}
	else if (links[p1i].innov < parent2->links[p2i].innov) {
	    nChildLinks++;
	    ++p1i;
	} else {
	    if (P->inheritAllLinks)
		nChildLinks++;
	    ++p2i;
	}
    }
    //Since we need to figure out the number of links before hand, we will
    // do structural mutation during mating, and need to calculate if these
    // mutations will occur, and add them to nChildLinks
    int addLink = 0, addNode = 0;
    if (rand_double() < P->addLinkMutationRate) addLink = 1;
    if (rand_double() < P->addNodeMutationRate) addNode = 1;


    //For add link mutation, we'll need to know the set of neurons to try
    // to connect
    std::set<int> neuronIDs;
    
    nChildLinks += addLink + 2*addNode;

    #ifdef _DEBUG_PRINT
	cout<<"  Mating, creating network with "<<nChildLinks<<" links."<<endl;
    #endif

    Link* childLinks = new Link[nChildLinks];    

    int i = 0;
    p1i = p2i = 0;
    while (p1i < nLinks or p2i < parent2->nLinks) {
	if (p1i >= nLinks) {
	    //There are still some of parent2's links:
	    if (P->inheritAllLinks) {
		#ifdef _DEBUG_PRINT
		    cout<<"     p1 has no more links, taking from p2: ";
		    parent2->links[p2i].printLink();
		    cout<<endl;
		#endif
		childLinks[i++].copy(parent2->links[p2i], P->linkEnabledRate);
		
		neuronIDs.insert(parent2->links[p2i].inID);
		neuronIDs.insert(parent2->links[p2i].outID);

		++p2i;
		continue;
	    } else {
		break;
	    }
	}
	if (p2i >= parent2->nLinks) {
	    //There are still some of parent1's (this's) links:
	    #ifdef _DEBUG_PRINT
		cout<<"     p1 has no more links, taking from p2: ";
		links[p1i].printLink();
		cout<<endl;
	    #endif
	    childLinks[i++].copy(links[p1i], P->linkEnabledRate);
	    
	    neuronIDs.insert(links[p1i].inID);
	    neuronIDs.insert(links[p1i].outID);

	    ++p1i;
	    continue;
	}
	//Both parents have links, so lets see if innovation number matches
	if (links[p1i].innov == parent2->links[p2i].innov) {
	    #ifdef _DEBUG_PRINT
		cout<<"    link both p1 and p2 share.\n";
	    #endif
	    //With some probability take from dominant parent (assume p1)
	    // otherwise create new link that is average of the two.
	    if (rand_double() < P->inheritDominant) {
		#ifdef _DEBUG_PRINT
		    cout<<"      Taking from dominant parent:";
		    links[p1i].printLink();
		    cout<<endl;
		#endif
		childLinks[i++].copy(links[p1i], P->linkEnabledRate);
	    } else {
		#ifdef _DEBUG_PRINT
		    cout<<"      Averaging from both parents:\n";
		    cout<<"      ";
		    links[p1i].printLink();
		    cout<<" .. ";
		    parent2->links[p2i].printLink();
		    cout<<endl;
		#endif
		childLinks[i++].copy(links[p1i], parent2->links[p2i],
				     P->linkEnabledRate);
	    }
	    
	    neuronIDs.insert(links[p1i].inID);
	    neuronIDs.insert(links[p1i].outID);
	    
	    ++p1i;
	    ++p2i;
	}
	else if (links[p1i].innov < parent2->links[p2i].innov) {
	    #ifdef _DEBUG_PRINT
		cout<<"     p2 does not have this link: ";
		links[p1i].printLink();
		cout<<endl;
	    #endif
	    childLinks[i++].copy(links[p1i], P->linkEnabledRate);
	    
	    neuronIDs.insert(links[p1i].inID);
	    neuronIDs.insert(links[p1i].outID);

	    ++p1i;
	} else {
	    if (P->inheritAllLinks) {
		#ifdef _DEBUG_PRINT
		    cout<<"     p1 does not have this link: ";
		    parent2->links[p2i].printLink();
		    cout<<endl;
		#endif
		childLinks[i++].copy(parent2->links[p2i], P->linkEnabledRate);
		
		neuronIDs.insert(parent2->links[p2i].inID);
		neuronIDs.insert(parent2->links[p2i].outID);
	    }
	    ++p2i;
	}
    }
   
    int nChildNodes = neuronIDs.size();

    if (addNode) addNodeMutate(childLinks, i, nChildNodes, IS);

    if (addLink) addLinkMutate(neuronIDs, childLinks, i, IS, P);

    //XXX May have to sort childLinks because innovation store adds innovation
    //    IDs out of order, possibly?  If erasing IS every generation, then
    //    only the added links from mutation are a problem, otherwise new
    //    innovation numbers can come from anywhere.
    stable_sort(childLinks,childLinks + nChildLinks);
    
    //return new Genome(childLinks, nChildLinks, nChildNodes, P);
    GenomeP child(new Genome(childLinks, nChildLinks, nChildNodes, P));
    return child;
}

/**
 * Single organism reproduction, only a single parent,
 * Form a new genome that is a copy of the parent's links.  
 *
 *  Assumes the links are sorted by innovation number, which should be the case
 *  if added as mutation occurs.  Also assumes the calling parent (this) is the
 *  more fit parent.
 * 
 * Also applies structural mutation.
 *
 */
GenomeP Genome::singleMate(InnovationStore *IS) const {

    //Lets do two pass at the mating.  The first pass figures out the size
    // of the child genome, the second creates a new array of links and fills
    // it in with copies of parent links. 

    int nChildLinks = nLinks;
    
    //Since we need to figure out the number of links before hand, we will
    // do structural mutation during mating, and need to calculate if these
    // mutations will occur, and add them to nChildLinks
    int addLink = 0, addNode = 0;
    if (rand_double() < P->addLinkMutationRate) addLink = 1;
    if (rand_double() < P->addNodeMutationRate) addNode = 1;

    //For add link mutation, we'll need to know the set of neurons to try
    // to connect
    std::set<int> neuronIDs;
    
    nChildLinks += addLink + 2*addNode;

    #ifdef _DEBUG_PRINT
	cout<<"  Mating, creating network with "<<nChildLinks<<" links."<<endl;
    #endif

    Link* childLinks = new Link[nChildLinks];    

    int i = 0;
    for (int p1i = 0; p1i < nLinks; ++p1i) {
	childLinks[i++].copy(links[p1i], P->linkEnabledRate);
	    
	neuronIDs.insert(links[p1i].inID);
	neuronIDs.insert(links[p1i].outID);
    }
   
    int nChildNodes = neuronIDs.size();

    if (addNode) addNodeMutate(childLinks, i, nChildNodes, IS);

    if (addLink) addLinkMutate(neuronIDs, childLinks, i, IS, P);

    //XXX May have to sort childLinks because innovation store adds innovation
    //    IDs out of order, possibly?  If erasing IS every generation, then
    //    only the added links from mutation are a problem, otherwise new
    //    innovation numbers can come from anywhere.
    stable_sort(childLinks,childLinks + nChildLinks);
    
    //return new Genome(childLinks, nChildLinks, nChildNodes, P);
    GenomeP child(new Genome(childLinks, nChildLinks, nChildNodes, P));
    return child;
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
    #ifdef _DEBUG_PRINT
	cout<<"  Mutating weights:"<<endl;
    #endif

    //Go over each link, see if we mutate the weight
    for (int i = 0; i<nLinks; ++i) {
	if (rand_double() > P->weightMutationRate)
	    continue;
	double mutateType = rand_double();
	
	#ifdef _DEBUG_PRINT
	    cout<<"    Mutating link #"<<i
		<<": w_orig = "<<links[i].weight<<endl;
	#endif

	if (mutateType < P->weightPerturbNormal) {
	    //Add random amount sampled from normal distribution with
	    // P->weightPerturbScale variance
	    links[i].weight += P->weightPerturbScale*rand_gauss();
	    #ifdef _DEBUG_PRINT
		cout<<"      Perturb normal -> w_new = "
		    <<links[i].weight<<endl;
	    #endif
	} else if (mutateType < P->weightPerturbNormal+
				P->weightPerturbUniform) {
	    //Add random amount sampled from uniform distribution over
	    //[-weightPerturbScale, weightPerturbScale]
	    links[i].weight += 2*P->weightPerturbScale*rand_double() -
				 P->weightPerturbScale;
	    #ifdef _DEBUG_PRINT
		cout<<"      Perturb uniform -> w_new = "
		    <<links[i].weight<<endl;
	    #endif
	} else {
	    //Randomly set the weight to some random amount in range:
	    //[-weightPerturbScale, weightPerturbScale]
	    links[i].weight = 2*P->weightPerturbScale*rand_double() -
				P->weightPerturbScale;
	    //XXX Could also try guassian
	    #ifdef _DEBUG_PRINT
		cout<<"      Randomly reset -> w_new = "
		    <<links[i].weight<<endl;
	    #endif
	}
    }
}

/**
 * Find compatability between two genomes, a rough measure of distance
 *  of one individual from another, just looking at genes.
 *
 * Compatability is weighted sum of #mismatching genes and weight difference
 *  for matching genes.
 */
double Genome::compat(const GenomeP &g2) {
    int gDiff = 0;
    double wDiff = 0.0;

    int p1i = 0, p2i = 0;
    while (p1i < nLinks or p2i < g2->nLinks) {
	if (p1i >= nLinks) {
	    gDiff++;
	    p2i++;
	} else if (p2i >= g2->nLinks) {
	    gDiff++;
	    p1i++;
	} else if (links[p1i].innov < g2->links[p2i].innov) {
	    gDiff++;
	    p1i++;
	} else if (links[p1i].innov > g2->links[p2i].innov) {
	    gDiff++;
	    p2i++;
	} else { //innov equal
	    wDiff += fabs(links[p1i].weight - g2->links[p2i].weight);
	    p1i++;
	    p2i++;
	}
    }
    
    return (P->compatGDiff * gDiff + P->compatWDiff * wDiff);
}

Network *Genome::createNewNetwork() const {
    return new Network(P->nInput, P->nOutput, links, nLinks);
}
	
void Genome::printDescription(const char *prefix) const {
    std::cout<<prefix<<"Genome has "<<nLinks<<" links, "
	     <<nNodes<<" nodes."<<endl;

/*
    for (int i = 0; i<nLinks; ++i) {
	std::cout<<prefix<<"  Link "<<i<<": ";
	links[i].printLink();
	std::cout<<endl;
    }
*/
}
