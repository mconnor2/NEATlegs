#include "Genome.h"
#include "random.h"
#include "Network.h"
#include "GeneticAlgorithm.h"
#include "InnovationStore.h"

#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <algorithm>
#include <map>

using namespace std;


namespace {

//True with probability p, drawing a random number only when p is strictly
// between 0 and 1, so mirror rates of 0 or 1 leave the random sequence as
// it would be without them
bool chance (double p) {
    if (p <= 0) return false;
    if (p >= 1) return true;
    return rand_double() < p;
}

//Link with this innovation in links sorted by innovation, or null
Link *findSorted (Link *links, int n, int innov) {
    Link *l = lower_bound(links, links + n, innov,
                          [](const Link &a, int i) { return a.innov < i; });
    return l != links + n && l->innov == innov ? l : nullptr;
}

//Same, in links that may not be sorted (while mating adds structure)
Link *findAny (Link *links, int n, int innov) {
    for (int i = 0; i < n; ++i)
        if (links[i].innov == innov) return &links[i];
    return nullptr;
}

void untie (Link &a, Link *b) {
    a.mirror = -1;
    if (b) b->mirror = -1;
}

//Mirror of every neuron that has one: the configured map for inputs and
// outputs, and for hidden neurons the ends of their tied links' partners
std::map<int, int> neuronMirrors (Link *links, int n,
                                  const ExpParameters *P) {
    std::map<int, int> sigma;
    for (int i = 0; i < (int)P->mirror.size(); ++i) sigma[i] = P->mirror[i];
    std::map<int, const Link *> byInnov;
    for (int i = 0; i < n; ++i) byInnov[links[i].innov] = &links[i];
    for (int i = 0; i < n; ++i) {
        if (links[i].mirror < 0) continue;
        auto p = byInnov.find(links[i].mirror);
        if (p == byInnov.end()) continue;
        sigma.insert(std::make_pair(links[i].inID, p->second->inID));
        sigma.insert(std::make_pair(links[i].outID, p->second->outID));
    }
    return sigma;
}

//After a child's genes are copied from its parents, keep only ties whose
// links both made it, tied to each other, and give each such pair one
// state: crossover may average one link's weight and not its partner's,
// and copying may re-enable one of them.  The pair takes either side's.
void repairTies (Link *links, int n) {
    for (int i = 0; i < n; ++i) {
        Link &a = links[i];
        if (a.mirror < 0 || a.mirror == a.innov) continue;
        Link *b = findSorted(links, n, a.mirror);
        if (!b || b->mirror != a.innov) {
            a.mirror = -1;
            continue;
        }
        if (b->innov < a.innov) continue;       //Done at b
        if (a.weight == b->weight && a.enabled == b->enabled) continue;
        Link &from = rand_double() < 0.5 ? a : *b;
        Link &to = &from == &a ? *b : a;
        to.weight = from.weight;
        to.enabled = from.enabled;
    }
}

}

/**
 * Create default link setup based on number of inputs and outputs.
 *
 * With a mirror map, each initial link is tied to its mirror image with
 * probability mirrorInitTied, and a tied pair shares one random weight.
 */
Genome::Genome (ExpParameters *_P) : P(_P) {
    nLinks = P->nInput * P->nOutput;
    nNodes = P->nInput + P->nOutput;

//    cout<<"Genome: nLinks = "<<nLinks<<", nNodes = "<<nNodes<<endl;

    //Initial links are numbered by their ends
    auto innovOf = [&](int inID, int outID) {
        return inID * P->nOutput + (outID - P->nInput);
    };

    links = new Link[nLinks];
    int linkID = 0;
    for (int inID = 0; inID < P->nInput; ++inID) {
        for (int outID = P->nInput; outID < nNodes; ++outID) {
            links[linkID].inID = inID;
            links[linkID].outID = outID;

            //And it starts enabled
            links[linkID].enabled = true;

            links[linkID].innov = linkID;

            if (!P->mirror.empty()) {
                int partner = innovOf(P->mirror[inID], P->mirror[outID]);
                if (partner < linkID) {
                    //Already drawn: copy it if they're tied
                    if (links[partner].mirror == linkID) {
                        links[linkID].mirror = partner;
                        links[linkID].weight = links[partner].weight;
                        ++linkID;
                        continue;
                    }
                } else if (chance(P->mirrorInitTied)) {
                    links[linkID].mirror = partner;
                }
            }

            //Give it an initial random weight (Gaussian?)
            links[linkID].weight = rand_gauss();

            ++linkID;
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

//Replace link s by a new neuron and two links, appended at nLink
void splitLink (Link *childLinks, int s, int &nLink, int &nNodes,
                InnovationStore *IS)
{
    int newNeuronID = -1;

    IS->addNode(childLinks[s].innov,
                childLinks[nLink].innov, //preInnov
                childLinks[nLink+1].innov, //postInnov
                newNeuronID);

    ++nNodes;

    #ifdef _DEBUG_PRINT
        cout<<"  Adding New node:"<<endl;
        cout<<"    Splitting link: ";
        childLinks[s].printLink();
        cout<<endl;
        cout<<"    New neuron id: "<<newNeuronID<<endl;
        cout<<"    New pre link innovation: "<<childLinks[nLink].innov
            <<", post link: "<<childLinks[nLink+1].innov<<endl;
    #endif

    //Now connect splitLink.in to new neuron
    childLinks[nLink].inID = childLinks[s].inID;
    childLinks[nLink].outID = newNeuronID;
    childLinks[nLink].weight = 1.0;
    childLinks[nLink].enabled = true;

    //And connect new neuron to splitLink.out
    childLinks[nLink+1].inID = newNeuronID;
    childLinks[nLink+1].outID = childLinks[s].outID;
    childLinks[nLink+1].weight = childLinks[s].weight;
    childLinks[nLink+1].enabled = true;

    //Old connection is disabled
    childLinks[s].enabled = false;

    nLink+=2;
}

//Splitting a tied link splits its mirror too (with probability
// mirrorStructure, otherwise the pair is untied), and ties the new links
// to their mirrors, so the new neurons mirror each other.  Splitting a link
// that is its own mirror gives a neuron that is its own mirror.
void addNodeMutate (Link *childLinks, int &nLink, int &nNodes,
                    InnovationStore *IS, const ExpParameters *P)
{
    //Randomly select a link to disable and replace by two links and a
    // new neuron inside.
    int s = 0;
    do {
        s = rand_int()%nLink;
    } while(!childLinks[s].enabled);

    Link &split = childLinks[s];
    bool self = split.mirror == split.innov;
    Link *partner = split.mirror >= 0 && !self ?
        findAny(childLinks, nLink, split.mirror) : nullptr;
    bool both = false;
    if (self || partner) {
        both = chance(P->mirrorStructure);
        if (!both) untie(split, partner);
    }

    int first = nLink;
    splitLink(childLinks, s, nLink, nNodes, IS);
    if (both && self) {
        childLinks[first].mirror = childLinks[first].innov;
        childLinks[first+1].mirror = childLinks[first+1].innov;
    } else if (both) {
        int second = nLink;
        splitLink(childLinks, (int)(partner - childLinks), nLink, nNodes, IS);
        for (int k = 0; k < 2; ++k) {
            childLinks[first+k].mirror = childLinks[second+k].innov;
            childLinks[second+k].mirror = childLinks[first+k].innov;
        }
    }
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
    //Output neuron cannot be an input node.  The two must not already be
    // connected (by an enabled or disabled link), so try a few pairs and
    // give up if they're all taken.
    const int maxTries = 20;
    int inID = -1, outID = -1;
    bool found = false;
    for (int t = 0; t < maxTries && !found; ++t) {
        inID = neurons[rand_int()%nNeurons];
        outID = neurons[rand_int()%(nNeurons-P->nInput) + P->nInput];
        found = true;
        for (int i = 0; i < nLinks; ++i) {
            if (childLinks[i].inID == inID && childLinks[i].outID == outID) {
                found = false;
                break;
            }
        }
    }
    if (!found) return;

    childLinks[nLinks].inID = inID;
    childLinks[nLinks].outID = outID;

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

    //If both ends have mirrors, with probability mirrorStructure add the
    // mirror image too, tied to it (or tie the link to itself if it is its
    // own mirror).  Needs room for one more link.
    if (P->mirror.empty() || P->mirrorStructure <= 0) return;
    std::map<int, int> sigma = neuronMirrors(childLinks, nLinks - 1, P);
    auto mi = sigma.find(inID), mo = sigma.find(outID);
    if (mi == sigma.end() || mo == sigma.end()) return;
    Link &added = childLinks[nLinks-1];
    if (mi->second == inID && mo->second == outID) {
        if (chance(P->mirrorStructure)) added.mirror = added.innov;
        return;
    }
    for (int i = 0; i < nLinks; ++i)
        if (childLinks[i].inID == mi->second &&
            childLinks[i].outID == mo->second) return;
    if (!chance(P->mirrorStructure)) return;

    Link &image = childLinks[nLinks];
    image.inID = mi->second;
    image.outID = mo->second;
    IS->addLink(image.inID, image.outID, image.innov);
    image.weight = added.weight;
    image.enabled = true;
    image.mirror = added.innov;
    added.mirror = image.innov;
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
 *      if equally fit, randomly from one or the other
 *  -Take all genes from both
 *  -Randomly select each individual gene, including with some high probability
 *
 *
 * Note that mating creates a new genome, which must be disposed of later.
 *
 * 10/08/08, Currently implemented to take all genes from both, should test
 *           other options if we see this as totally not working.
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

    //A mirrored mutation adds the mirror image's links too
    int copies = P->mirror.empty() ? 1 : 2;
    nChildLinks += copies * (addLink + 2*addNode);

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

    if (!P->mirror.empty()) repairTies(childLinks, i);

    if (addNode) addNodeMutate(childLinks, i, nChildNodes, IS, P);

    //May add no link if it can't find an unconnected pair, so from here
    // on i, not nChildLinks, is the number of links
    if (addLink) addLinkMutate(neuronIDs, childLinks, i, IS, P);

    //XXX May have to sort childLinks because innovation store adds innovation
    //    IDs out of order, possibly?  If erasing IS every generation, then
    //    only the added links from mutation are a problem, otherwise new
    //    innovation numbers can come from anywhere.
    stable_sort(childLinks,childLinks + i);

    GenomeP child(new Genome(childLinks, i, nChildNodes, P));
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

    //A mirrored mutation adds the mirror image's links too
    int copies = P->mirror.empty() ? 1 : 2;
    nChildLinks += copies * (addLink + 2*addNode);

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

    if (!P->mirror.empty()) repairTies(childLinks, i);

    if (addNode) addNodeMutate(childLinks, i, nChildNodes, IS, P);

    //May add no link if it can't find an unconnected pair, so from here
    // on i, not nChildLinks, is the number of links
    if (addLink) addLinkMutate(neuronIDs, childLinks, i, IS, P);

    //XXX May have to sort childLinks because innovation store adds innovation
    //    IDs out of order, possibly?  If erasing IS every generation, then
    //    only the added links from mutation are a problem, otherwise new
    //    innovation numbers can come from anywhere.
    stable_sort(childLinks,childLinks + i);

    GenomeP child(new Genome(childLinks, i, nChildNodes, P));
    return child;
}

/**
 * Two types of mutations: weight and structural.
 *
 * Weight mutation:
 *  For each link, with some probability change the weight:
 *      -Perturb weight by uniform random amount
 *      -Perturb weight by normal distributed amount
 *      -Randomly reset weight to some amount
 *
 * Structural mutation:
 *  -With some probability a node will be added
 *      -One link is chosen and a new node is added between it.
 *  -With some probability a link between existing nodes will be added
 *
 * 10/08/08 For now only implementing weight mutation.
 */
void Genome::mutate() {
    #ifdef _DEBUG_PRINT
        cout<<"  Mutating weights:"<<endl;
    #endif

    //Symmetry breaking: untie pairs for good
    if (P->untieRate > 0) {
        for (int i = 0; i<nLinks; ++i) {
            Link &l = links[i];
            if (l.mirror < l.innov) continue;   //Untied, or done at partner
            if (chance(P->untieRate))
                untie(l, l.mirror == l.innov ? nullptr :
                         findSorted(links, nLinks, l.mirror));
        }
    }

    //Go over each link, see if we mutate the weight.  A tied pair is
    // mutated once, at its first link, and the second copies it.
    for (int i = 0; i<nLinks; ++i) {
        if (links[i].mirror >= 0 && links[i].mirror < links[i].innov)
            continue;
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

        if (links[i].mirror > links[i].innov)
            findSorted(links, nLinks, links[i].mirror)->weight =
                links[i].weight;
    }
}

/**
 * Find compatability between two genomes, a rough measure of distance
 *  of one individual from another, just looking at genes.
 *
 * As in Stanley & Miikkulainen (2002), eq. 1: the number of mismatching
 *  (disjoint or excess) genes plus the mean weight difference of matching
 *  genes, each weighted.  Like Stanley's own implementation, the gene count
 *  isn't divided by genome size (the paper's N = 1), so a new node counts
 *  the same in a large genome as in a small one.
 *
 * A tied pair of links (see ExpParameters::mirror) is one gene expressed
 *  twice, so its mismatching links count half each: a mirrored structural
 *  mutation moves a genome as far as a plain one.
 */
double Genome::compat(const GenomeP &g2) const {
    int nMatching = 0;
    double gDiff = 0, wDiff = 0.0;
    auto gene = [](const Link &l) { return l.mirror >= 0 ? 0.5 : 1.0; };

    int p1i = 0, p2i = 0;
    while (p1i < nLinks or p2i < g2->nLinks) {
        if (p1i >= nLinks) {
            gDiff += gene(g2->links[p2i]);
            p2i++;
        } else if (p2i >= g2->nLinks) {
            gDiff += gene(links[p1i]);
            p1i++;
        } else if (links[p1i].innov < g2->links[p2i].innov) {
            gDiff += gene(links[p1i]);
            p1i++;
        } else if (links[p1i].innov > g2->links[p2i].innov) {
            gDiff += gene(g2->links[p2i]);
            p2i++;
        } else { //innov equal
            wDiff += fabs(links[p1i].weight - g2->links[p2i].weight);
            nMatching++;
            p1i++;
            p2i++;
        }
    }

    double meanWDiff = nMatching ? wDiff / nMatching : 0.0;
    return P->compatGDiff * gDiff + P->compatWDiff * meanWDiff;
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

GenomeP Genome::clone() const {
    Link *copy = new Link[nLinks];
    for (int i = 0; i<nLinks; ++i) {
        copy[i] = links[i];
    }
    GenomeP g(new Genome(copy, nLinks, nNodes, P));
    g->fitness = fitness;
    g->specie = specie;
    return g;
}

int Genome::numEnabledLinks() const {
    int n = 0;
    for (int i = 0; i<nLinks; ++i)
        if (links[i].enabled) ++n;
    return n;
}

int Genome::numTiedLinks() const {
    int n = 0;
    for (int i = 0; i<nLinks; ++i)
        if (links[i].mirror >= 0) ++n;
    return n;
}

int Genome::numHiddenNodes() const {
    return nNodes - P->nInput - P->nOutput;
}

void Genome::save(std::ostream &out) const {
    out<<"genome inputs "<<P->nInput<<" outputs "<<P->nOutput
       <<" nodes "<<nNodes<<" links "<<nLinks<<"\n";
    //The mirror column only with a mirror map, so other runs' files are
    // unchanged
    bool mirrored = !P->mirror.empty();
    out<<"# link innovation in out weight enabled"
       <<(mirrored ? " mirror" : "")<<"\n";
    std::streamsize prec = out.precision(17);
    for (int i = 0; i<nLinks; ++i) {
        const Link &l = links[i];
        out<<"link "<<l.innov<<" "<<l.inID<<" "<<l.outID<<" "<<l.weight
           <<" "<<(l.enabled ? 1 : 0);
        if (mirrored) out<<" "<<l.mirror;
        out<<"\n";
    }
    out.precision(prec);
}

GenomeP Genome::load(std::istream &in, ExpParameters *P) {
    std::string line;
    int nIn = -1, nOut = -1, nodes = -1, count = -1;

    //Header, skipping comments
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream hs(line);
        std::string tag, kIn, kOut, kNodes, kLinks;
        hs>>tag>>kIn>>nIn>>kOut>>nOut>>kNodes>>nodes>>kLinks>>count;
        if (!hs || tag != "genome") {
            cerr<<"Genome::load expected 'genome' header, got: "<<line<<endl;
            return GenomeP();
        }
        break;
    }
    if (count < 0) {
        cerr<<"Genome::load missing genome header"<<endl;
        return GenomeP();
    }
    if (nIn != P->nInput || nOut != P->nOutput) {
        cerr<<"Genome::load genome has "<<nIn<<" inputs, "<<nOut
            <<" outputs but experiment expects "<<P->nInput<<", "
            <<P->nOutput<<endl;
        return GenomeP();
    }

    Link *links = new Link[count];
    int n = 0;
    while (n < count && std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream ls(line);
        std::string tag;
        int enabled;
        Link &l = links[n];
        ls>>tag>>l.innov>>l.inID>>l.outID>>l.weight>>enabled;
        if (!ls || tag != "link") {
            cerr<<"Genome::load bad link line: "<<line<<endl;
            delete [] links;
            return GenomeP();
        }
        l.enabled = enabled != 0;
        int mirror;
        if (ls>>mirror) l.mirror = mirror;      //Optional column
        ++n;
    }
    if (n != count) {
        cerr<<"Genome::load expected "<<count<<" links, found "<<n<<endl;
        delete [] links;
        return GenomeP();
    }

    return GenomeP(new Genome(links, count, nodes, P));
}
