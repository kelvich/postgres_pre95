/*
 * valid.h --
 *	POSTGRES tuple qualification validity definitions.
 */

#ifndef	ValidIncluded		/* Include this file only once */
#define ValidIncluded	1

/*
 * Identification:
 */
#define VALID_H	"$Header$"

#ifndef C_H
#include "c.h"
#endif

#include "skey.h"

#ifndef	BUF_H
# include "buf.h"
#endif
#ifndef	TQUAL_H
# include "tqual.h"
#endif

/*
 * ItemIdHasValidHeapTupleForQualification --
 *	True iff a heap tuple associated with an item identifier satisfies
 *	both a time range and a scan key qualification.
 *
 * Note:
 *	Assumes item identifier is valid.
 *	Assumes buffer is locked appropriately.
 *	Assumes time range is valid.
 *	Assumes scan qualification (key) is valid.
 */
extern
bool
ItemIdHasValidHeapTupleForQualification ARGS((
	ItemId		itemId,
	Buffer		buffer,
	TimeQual	qual,
	ScanKeySize	numberOfKeys,
	ScanKey		key
));

/*
 *  TupleUpdatedByCurXactAndCmd() -- Returns true if this tuple has
 *	already been updated once by the current transaction/command
 *	pair.
 */

extern
bool
TupleUpdatedByCurXactAndCmd ARGS((
	HeapTuple	tuple
));

#endif	/* !defined(ValidIncluded) */
