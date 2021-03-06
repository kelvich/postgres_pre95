/* ----------------------------------------------------------------
 *   FILE
 *	rac.h
 *
 *   DESCRIPTION
 *	POSTGRES rule lock access definitions.
 *
 *   NOTES
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef	RAcIncluded	/* Include this file only once. */
#define RAcIncluded	1

#include "tmp/c.h"

#include "storage/block.h"
#include "storage/buf.h"
#include "access/htup.h"
#include "utils/rel.h"
#include "rules/prs2locks.h"

/*
 * HeapTupleFreeRuleLock
 *	Free a rule lock but only if it is a main memory pointer.
 */
extern
void
HeapTupleFreeRuleLock ARGS((
	HeapTuple	tuple
));

/*
 * HeapTupleGetRuleLock --
 *	Returns the rule lock for a heap tuple or InvalidRuleLock if
 *	the rule lock is NULL.
 *
 * Note:
 *	Assumes heap tuple is valid.
 */
extern
RuleLock
HeapTupleGetRuleLock ARGS((
	HeapTuple	tuple,
	Buffer		buffer
));

/*
 * HeapTupleHasEmptyRuleLock
 * return true iff the given tuple has an empty rule lock
 */
extern
bool
HeapTupleHasEmptyRuleLock ARGS((
	HeapTuple	tuple,
	Buffer		buffer
));

/*
 * HeapTupleSetRuleLock --
 *	Sets the rule lock for a heap tuple.
 *
 * Note:
 *	Assumes heap tuple is valid.
 * NOTE #2: SOS !
 *	XXX: if the previous old lock was a memory pointer,
 *	then this old lock is pfreed !!!!!
 * NOTE #3: SOS !
 *      XXX: NO copy of the 'lock' or 'tuple' is made!
 *
 */
extern
void
HeapTupleSetRuleLock ARGS((
	HeapTuple	tuple,
	Buffer		buffer,
	RuleLock	lock
));

/*
 * HeapTupleStoreRuleLock --
 *	If a tuple has a "main memory" rule lock (i.e. a RuleLock)
 * 	thn it stores this lock to the relation (in the same page
 * 	as the tuple if possible).
 *	Finally the tuple is linked to this new "disk" lock.
 *
 *	NOTE: (*DANGER*, DANGER Mr. Robinson......)
 *	XXX:the old (main memory) lock is pfreed!!!!!!
 */
extern
void
HeapTupleStoreRuleLock ARGS((
	HeapTuple	tuple,
	Buffer		buffer
));

#endif	/* !defined(RAcIncluded) */
