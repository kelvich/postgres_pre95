/*
 * rac.h --
 *	POSTGRES rule lock access definitions.
 *
 * Identification:
 *	$Header$
 */

#ifndef	RAcIncluded	/* Include this file only once. */
#define RAcIncluded	1

#include "c.h"

#include "block.h"
#include "buf.h"
#include "htup.h"
#include "rel.h"
#include "rlock.h"

/*
 * HeapTupleGetRuleLock --
 *	Returns the rule lock for a heap tuple or InvalidRuleLock if
 *	the rule lock is NULL.
 *
 * Note:
 *	Assumes heap tuple is valid.
 *	Assumes buffer is invalid or associated with the heap tuple.
 */
extern
RuleLock
HeapTupleGetRuleLock ARGS((
	HeapTuple	tuple,
	Buffer		buffer
));

/*
 * HeapTupleSetRuleLock --
 *	Sets the rule lock for a heap tuple.
 *
 * Note:
 *	Assumes heap tuple is valid.
 */
extern
HeapTuple
HeapTupleSetRuleLock ARGS((
	HeapTuple	tuple,
	Buffer		buffer,
	RuleLock	lock
));

/*
 * HeapTupleStoreRuleLock --
 *	Stores the rule lock of a heap tuple into a heap relation.
 *
 *	If the tuple had a valid lock, then it is physically deleted.
 *	Then the new lock is stored on the same frame as the tuple, if
 *	possible.  Finally, the tuple is linked to the new lock.
 *
 * Note:
 *	Assumes heap tuple is valid.
 *	Assumes buffer is valid.
 */
extern
void
HeapTupleStoreRuleLock ARGS((
	HeapTuple	tuple;
	Buffer		buffer;
	RuleLock	lock;
));

#endif	/* !defined(RAcIncluded) */
