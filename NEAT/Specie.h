#ifndef __SPECIE_H
#define __SPECIE_H

#include <list>
#include "Genome.h"
#include <boost/shared_ptr.hpp>

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

    double maxFitness() {
	//Representative is the most fit individual, per generation
	return members[0]->fitness;
    }
    
    double calculateFitness();

    bool cull(const int oldAge, double &totalFitness);

    void print_statistics ();
};

typedef boost::shared_ptr<Specie> SpecieP;
typedef std::list<SpecieP> specieVec;
typedef specieVec::iterator specie_it;
typedef specieVec::const_iterator specie_cit;

#endif
