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
 *	getrusage
 *
 *   NOTES
 *
 *   IDENTIFICATION
 *	$Header$
 */

#include "nodes/relation.h"
#include <math.h>		/* for pow() prototype */

#include <errno.h>
#include "rusagestub.h"

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

int
getrusage(who, rusage)
    int who;
    struct rusage *rusage;
{
    struct tms tms;
    register int tick_rate = CLK_TCK;	/* ticks per second */
    clock_t u, s;

    if (rusage == (struct rusage *) NULL) {
	errno = EFAULT;
	return(-1);
    }
    if (times(&tms) < 0) {
	/* errno set by times */
	return(-1);
    }
    switch (who) {
    case RUSAGE_SELF:
	u = tms.tms_utime;
	s = tms.tms_stime;
	break;
    case RUSAGE_CHILDREN:
	u = tms.tms_cutime;
	s = tms.tms_cstime;
	break;
    default:
	errno = EINVAL;
	return(-1);
    }
#define TICK_TO_SEC(T, RATE)	((T)/(RATE))
#define	TICK_TO_USEC(T,RATE)	(((T)%(RATE)*1000000)/RATE)
    rusage->ru_utime.tv_sec = TICK_TO_SEC(u, tick_rate);
    rusage->ru_utime.tv_usec = TICK_TO_USEC(u, tick_rate);
    rusage->ru_stime.tv_sec = TICK_TO_SEC(s, tick_rate);
    rusage->ru_stime.tv_usec = TICK_TO_USEC(u, tick_rate);
    return(0);
}
