#ifndef	_RSELECT_H_
#define	_RSELECT_H_	"$Header$"

/*
 *	rselect.h	- definitions for the replacement selection algorithm.
 */

#ifndef C_H
#include "c.h"
#endif

#include "htup.h"

struct	leftist {
	short		lt_dist;	/* distance to leaf/empty node */
	short		lt_devnum;	/* device number of tuple */
	HeapTuple	lt_tuple;
	struct	leftist	*lt_left;
	struct	leftist	*lt_right;
};

extern	struct	leftist	*Tuples;
extern	HeapTuple	gettuple();
extern	int		puttuple();
extern	int		dumptuples();
extern	int		tuplecmp();

#endif
