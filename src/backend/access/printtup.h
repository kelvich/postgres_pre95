/*
 * printtup.h --
 *
 *
 * Identification:
 *	$Header$
 */

#ifndef	PrintTupIncluded		/* Include this file only once */
#define PrintTupIncluded	1

extern void printtup ARGS((HeapTuple tuple, struct attribute *typeinfo[]));
extern void debugtup ARGS((HeapTuple tuple, struct attribute *typeinfo[]));

#endif	/* !defined(PrintTupIncluded) */
