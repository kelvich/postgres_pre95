#ifndef	_LSELECT_H_
#define	_LSELECT_H_	"$Header$"

/* ----------------------------------------------------------------
 *	lselect.h	- definitions for the replacement selection algorithm.
 *
 *	This file is used by
 *		utils/sort/lselect.c
 *		utils/sort/psort.c
 *
 *	-cim 6/12/90
 * ----------------------------------------------------------------
 */

#include "tmp/c.h"
#include "access/htup.h"

struct	leftist {
	short		lt_dist;	/* distance to leaf/empty node */
	short		lt_devnum;	/* device number of tuple */
	HeapTuple	lt_tuple;
	struct	leftist	*lt_left;
	struct	leftist	*lt_right;
};

extern	struct	leftist	*Tuples;

struct leftist *lmerge ARGS((struct leftist *pt , struct leftist *qt ));
HeapTuple gettuple ARGS((struct leftist **treep , short *devnum ));
int puttuple ARGS((struct leftist **treep , HeapTuple newtuple , int devnum ));
int dumptuples ARGS((FILE *file ));
int tuplecmp ARGS((HeapTuple ltup , HeapTuple rtup ));

#ifdef EBUG
int checktree ARGS((struct leftist *tree ));
int checktreer ARGS((struct leftist *tree , int level ));
#endif EBUG

#endif _LSELECT_H_
