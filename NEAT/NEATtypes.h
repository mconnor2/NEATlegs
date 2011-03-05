#ifndef __NEAT_TYPES_H
#define __NEAT_TYPES_H

#include <boost/shared_ptr.hpp>
#include <list>
#include <vector>

class Specie;
class Genome;
struct ExpParameters;
class InnovationStore;

typedef boost::shared_ptr<Specie> SpecieP;
typedef std::list<SpecieP> specieVec;
typedef specieVec::iterator specie_it;
typedef specieVec::const_iterator specie_cit;

typedef boost::shared_ptr<Genome> GenomeP;
typedef std::vector<GenomeP> genomeVec;
typedef genomeVec::iterator genome_it;
typedef genomeVec::const_iterator genome_cit;

#endif
