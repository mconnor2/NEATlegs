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

    Specie(int _id, int _age = 0) :
        id(_id), nMembers(0), age(_age), fitness(-1) {}

    GenomeP representative () const {return members[0];}

    void addMember(const GenomeP &m) {
        members.push_back(m);
        nMembers++;
    };


    double calculateFitness();

    double maxFitness() const;

    bool cull(const int oldAge, double &totalFitness);

};


#endif
