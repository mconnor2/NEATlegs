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
};

/* Seed that new threads derive their state from, set by dev_seed_rand */
static KissState baseSeed = {123456789, 362436000, 521288629, 7654321};
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

/* Initialise KISS generator using /dev/urandom */
void dev_seed_rand()
{
   KissState s;
   s.x = devrand();
   while (!(s.y = devrand())); /* y must not be zero */
   s.z = devrand();
   /* Don't really need to seed c as well but if you really want to... */
   s.c = devrand() % 698769069; /* Should be less than 698769069 */
   baseSeed = s;
   state = s;
}


