#include <stdlib.h>
#include <unistd.h>
#include <atomic>
#include <iostream>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "random.h"

using namespace std;

//Code taken from Good Practice in (Pseudo) Random Number Generation for
// Bioinformatics Applications by David Jones, UCL Bioinformatics Unit

struct KissState {
    unsigned int x, y, z, c;
    int haveGauss;              //rand_gauss makes deviates in pairs
    double gauss;
};

/* Seed that new threads derive their state from, set by dev_seed_rand */
static KissState baseSeed = {123456789, 362436000, 521288629, 7654321, 0, 0};
static std::atomic<unsigned int> threadCount(0);

static KissState newThreadState() {
    KissState s = baseSeed;
    unsigned int n = threadCount++;
    if (n == 0) return s;   // first thread keeps the base seed exactly
    s.x += 0x9E3779B9u * n;
    s.y ^= 0x85EBCA6Bu * n;
    if (!s.y) s.y = 362436000;
    s.z += 0xC2B2AE35u * n;
    return s;
}

static thread_local KissState state = newThreadState();

unsigned int rand_int(){
   unsigned long long t, a = 698769069ULL;
   unsigned int &x = state.x, &y = state.y, &z = state.z, &c = state.c;
   x = 69069*x+12345;
   y ^= (y<<13); y ^= (y>>17); y ^= (y<<5);
   t = a*z+c; c = (t>>32);
   return x+y+(z=t);
}

//Read random integer from /dev/urandom
//  Mostly used for initializing seeds
unsigned int devrand()
{
    int fn;
    unsigned int r;
    fn = open("/dev/urandom", O_RDONLY);
    if (fn == -1) {
        cerr<<"Failed to open /dev/urandom"<<endl;
        exit(-1); /* Failed! */
    }
    if (read(fn, &r, 4) != 4) {
       cerr<<"Failed to read 4 bytes from /dev/urandom"<<endl;
       exit(-1); /* Failed! */
    }
    close(fn);
    return r;
}

//SplitMix64: expands one 64 bit seed into well mixed words
static uint64_t splitmix64(uint64_t &s) {
   uint64_t z = (s += 0x9E3779B97F4A7C15ull);
   z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
   z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
   return z ^ (z >> 31);
}

void seed_rand(uint64_t seed)
{
   KissState s;
   s.x = (unsigned int)splitmix64(seed);
   while (!(s.y = (unsigned int)splitmix64(seed))); /* y must not be zero */
   s.z = (unsigned int)splitmix64(seed);
   s.c = (unsigned int)(splitmix64(seed) % 698769069); /* < 698769069 */
   s.haveGauss = 0;
   s.gauss = 0;
   baseSeed = s;
   state = s;
}

/* Initialise KISS generator using /dev/urandom */
uint64_t dev_seed_rand()
{
   uint64_t seed = ((uint64_t)devrand() << 32) | devrand();
   seed_rand(seed);
   return seed;
}

//Normal deviate by the polar method, two at a time
double rand_gauss()
{
  if (state.haveGauss) {
    state.haveGauss = 0;
    return state.gauss;
  }
  double fac, rsq, v1, v2;
  do {
    v1 = 2.0*rand_double() - 1.0;
    v2 = 2.0*rand_double() - 1.0;
    rsq = v1*v1 + v2*v2;
  } while (rsq >= 1.0 || rsq == 0.0);
  fac = sqrt(-2.0*log(rsq)/rsq);
  state.gauss = v1*fac;
  state.haveGauss = 1;
  return v2*fac;
}
