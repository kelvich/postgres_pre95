/*
 *   FILE
 *	port.c
 *
 *   DESCRIPTION
 *	port-specific routines for HP-UX
 *
 *   INTERFACE ROUTINES
 *	init_address_fixup
 *	rint
 *	cbrt
 *	random
 *	srandom
 *	getrusage
 *
 *   NOTES
 *	For the most part, this file gets around some non-POSIX calls 
 *	in POSTGRES.
 *
 *   IDENTIFICATION
 *	$Header$
 */

#include <unistd.h>		/* for rand()/srand() prototypes */
#include <math.h>		/* for pow() prototype */
#include <sys/syscall.h>	/* for syscall #defines */

#include "tmp/c.h"

init_address_fixup()
{
    /*
     * On PA-RISC, unaligned access fixup is handled by the compiler,
     * not by the kernel.
     */
}

#ifdef NEED_RINT
double 
rint(x)
	double x;
{
	if (x < 0.0) 
		return((double)((long)(x-0.5)));
	return((double)((long)(x+0.5)));
}
#endif /* NEED_RINT */

double
cbrt(x)
	double x;
{
	return(pow(x, 1.0/3.0));
}

long
random()
{
	return(lrand48());
}

void
srandom(seed)
	int seed;
{
	srand48((long int) seed);
}

getrusage(who, ru)
	int who;
	struct rusage *ru;
{
	return(syscall(SYS_GETRUSAGE, who, ru));
}
