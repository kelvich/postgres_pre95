/*
 *   FILE
 *	rusagestub.h
 *
 *   DESCRIPTION
 *	Stubs for getrusage(3).
 *
 *   NOTES
 *
 *   IDENTIFICATION
 *	$Header$
 */

#ifndef rusagestubIncluded		/* include this file only once */
#define rusagestubIncluded 1

#include <sys/time.h>	/* for struct timeval */
#include <sys/times.h>	/* for struct tms */
#include <limits.h>	/* for CLK_TCK */

#define	RUSAGE_SELF	0
#define	RUSAGE_CHILDREN	-1

struct rusage {
    struct timeval ru_utime;		/* user time used */
    struct timeval ru_stime;		/* system time used */
};

extern int getrusage ARGS((int who, struct rusage *rusage));

#endif /* rusagestubIncluded */
