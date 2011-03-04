#ifndef __SPECIE_H
#define __SPECIE_H

//#include "Genome.h"
#include "NEATtypes.h"

struct Specie {
    int nMembers;
    int age;
    double fitness;	//Average fitness of its members
    genomeVec members;

    Specie(int _age = 0) : nMembers(0), fitness(-1), age(_age) {}

    GenomeP representative () const {return members[0];}
    
    void addMember(const GenomeP &m) {
	members.push_back(m);
	nMembers++;
    };

    
    double calculateFitness();

    double maxFitness() const;

    bool cull(const int oldAge, double &totalFitness);

    void print_statistics () const;
};


#endif
