/*
 *   FILE
 *	port.c
 *
 *   DESCRIPTION
 *	SunOS5-specific routines
 *
 *   INTERFACE ROUTINES
 *	sparc_bug_set_outerjoincost
 *	random
 *	srandom
 *
 *   NOTES
 *
 *   IDENTIFICATION
 *	$Header$
 */

#include "nodes/relation.h"
#include <math.h>		/* for pow() prototype */

RcsId("$Header$");

/*
 * Hackety-hack for Sparc compiler error.
 *
 * See planner/path/joinpath.c
 */
void
sparc_bug_set_outerjoincost(p, val)
    char *p;
    long val;
{
    set_outerjoincost((Path)p, (Cost)val);
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
