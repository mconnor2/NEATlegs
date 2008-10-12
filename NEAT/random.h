#ifndef __RANDOM_HELPERS_H
#define __RANDOM_HELPERS_H

#include <math.h>

/**
 * Some random number generating code that should be better than stdlib rand
 *
 */


//KISS RNG for generating random integer
unsigned int rand_int ();

unsigned int devrand ();

void dev_seed_rand ();

//Returns double precision number selected uniformly between [0,1)
// Going for simple approach that only uses 32 bit random numbers
// instead of combining two random numbers for 53 bit precision
inline double rand_double () {
    return (double)rand_int() / 4294967296.0;
}

//Returns a normally distributed deviate with 0 mean and unit variance
//Algorithm is from Numerical Recipes in C, Second Edition
// Code is from ken stanley's NEAT implementation
inline double rand_gauss() {
  static int iset=0;
  static double gset;
  double fac,rsq,v1,v2;

  if (iset==0) {
    do {
      v1=2.0*rand_double()-1.0;
      v2=2.0*rand_double()-1.0;
      rsq=v1*v1+v2*v2;
    } while (rsq>=1.0 || rsq==0.0);
    fac=sqrt(-2.0*log(rsq)/rsq);
    gset=v1*fac;
    iset=1;
    return v2*fac;
  } 
  else {
    iset=0;
    return gset;
  }
} 

#endif
