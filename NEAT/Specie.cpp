#include "Specie.h"
#include "Genome.h"

#include <iostream>

double Specie::calculateFitness() {
    fitness = 0;
    //GenomeP maxFit = members[0];
    genome_it maxFiti = members.begin();
    double maxFitness = -1e9;
    for (genome_it pi = members.begin(); pi != members.end(); ++pi) {
	(*pi)->fitness /= nMembers;
	fitness += (*pi)->fitness;

	if ((*pi)->fitness > maxFitness) {
	    maxFiti = pi;
	    maxFitness = (*pi)->fitness;
	}
    }

    //Make sure the representative member is the most fit
    members[0].swap(*maxFiti);
}
    
double Specie::maxFitness() const {
    //Representative is the most fit individual, per generation
    return members[0]->fitness;
}

/**
 * Decide whether this species should be removed or not.
 *   A species should be removed if:
 *   	-it is above a certain age and:
 *   	  -There is only one viable member
 *
 * When removing, subtract fitness from totalFitness and return true
 *
 * May also decide to remove weak members of the species, in that case 
 *  subtract fitness from totalFitness and specie fitness, then set member's 
 *  fitness to 0 so won't be selected to mate.
 */
bool Specie::cull(const int oldAge, double &totalFitness) {
    if (age >= oldAge and nMembers <= 1) {
	totalFitness -= fitness;
	members[0]->fitness = 0;
	return true;
    }

    return false;
}

void Specie::print_statistics () const {
    std::cerr<<"\t"<<members.size()
	     <<"\t"<<fitness
	     <<"\t"<<maxFitness();
}
