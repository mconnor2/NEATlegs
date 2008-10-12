#include <stdlib.h>
#include <iostream>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "random.h"

using namespace std;

//Code taken from Good Practice in (Pseudo) Random Number Generation for
// Bioinformatics Applications by David Jones, UCL Bioinformatics Unit

/* Seed variables */
unsigned int x = 123456789,y = 362436000,z = 521288629,c = 7654321; 

unsigned int rand_int(){
   unsigned long long t, a = 698769069ULL;
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
   x = devrand();
   while (!(y = devrand())); /* y must not be zero */
   z = devrand();
   /* Don't really need to seed c as well but if you really want to... */
   c = devrand() % 698769069; /* Should be less than 698769069 */
}


