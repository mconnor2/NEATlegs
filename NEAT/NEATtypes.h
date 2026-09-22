#ifndef __NEAT_TYPES_H
#define __NEAT_TYPES_H

#include <memory>
#include <list>
#include <vector>

struct Specie;
class Genome;
struct ExpParameters;
class InnovationStore;

typedef std::shared_ptr<Specie> SpecieP;
typedef std::list<SpecieP> specieVec;
typedef specieVec::iterator specie_it;
typedef specieVec::const_iterator specie_cit;

typedef std::shared_ptr<Genome> GenomeP;
typedef std::vector<GenomeP> genomeVec;
typedef genomeVec::iterator genome_it;
typedef genomeVec::const_iterator genome_cit;

#endif
