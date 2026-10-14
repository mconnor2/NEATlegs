#ifndef __SPECIE_H
#define __SPECIE_H

//#include "Genome.h"
#include "NEATtypes.h"

struct Specie {
    int id;             //Carried over when species continues next generation
    int nMembers;
    int age;
    double fitness;     //Average fitness of its members
    genomeVec members;

    double bestFitness = -1e300;    //Best raw fitness a member has had
    int staleness = 0;              //Generations since bestFitness improved

    Specie(int _id, int _age = 0) :
        id(_id), nMembers(0), age(_age), fitness(-1) {}

    GenomeP representative () const {return members[0];}

    void addMember(const GenomeP &m) {
        members.push_back(m);
        nMembers++;
    };


    //Update bestFitness and staleness; call while fitness is still raw,
    // before calculateFitness shares it
    void updateStagnation();

    bool contains(const GenomeP &g) const;

    double calculateFitness();

    double maxFitness() const;

    //champion is the population's best genome; its species is never culled
    bool cull(const int oldAge, double &totalFitness,
              const GenomeP &champion);

};


#endif
