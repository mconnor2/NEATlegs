#ifndef __RANDOM_HELPERS_H
#define __RANDOM_HELPERS_H

#include <math.h>
#include <stdint.h>

/**
 * Some random number generating code that should be better than stdlib rand
 *
 * Generator state is per thread, so fitness functions can draw random
 * numbers while the population is evaluated in parallel.
 * dev_seed_rand() / seed_rand() seed the calling thread, and every thread
 * started afterwards gets its own seed derived from it.  Only draws made on
 * the seeding thread are reproducible: worker threads are numbered in the
 * order they start, which varies between runs.
 */


//KISS RNG for generating random integer
unsigned int rand_int ();

unsigned int devrand ();

//Seed from /dev/urandom, returning the seed used so a run can be repeated
uint64_t dev_seed_rand ();

//Seed deterministically (same seed, same sequence on this thread)
void seed_rand (uint64_t seed);

//Returns double precision number selected uniformly between [0,1)
// Going for simple approach that only uses 32 bit random numbers
// instead of combining two random numbers for 53 bit precision
inline double rand_double () {
    return (double)rand_int() / 4294967296.0;
}

//Returns a normally distributed deviate with 0 mean and unit variance
//Algorithm is from Numerical Recipes in C, Second Edition
// Code is from ken stanley's NEAT implementation
double rand_gauss ();

#endif
