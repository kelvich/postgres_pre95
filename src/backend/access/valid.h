/* ----------------------------------------------------------------
 *   FILE
 *	valid.h
 *
 *	XXX this file is becoming obsolete!  use the heap_ routines
 *	instead -cim 4/30/91
 *
 *   DESCRIPTION
 *	POSTGRES tuple qualification validity definitions.
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef	ValidIncluded		/* Include this file only once */
#define ValidIncluded	1

#define VALID_H	"$Header$"

#include "tmp/c.h"
#include "access/skey.h"
#include "storage/buf.h"
#include "access/tqual.h"

/* ----------------
 *	extern decl's
 * ----------------
 */

/* 
 *  heap_keytest
 *    -- Test a heap tuple with respect to a scan key.
 */
extern
bool
heap_keytest ARGS((
    HeapTuple		t,
    TupleDescriptor 	tupdesc,
    int			nkeys,
    struct skey 	keys[]
));		   

/* 
 *  heap_satisifies
 *    -- test if item on page satisifies scan key
 */
extern
bool
heap_satisifies ARGS((
    ItemId	itemId,
    Buffer	buffer,
    TimeQual	qual,
    ScanKeySize	nKeys,
    struct skey	*key
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

/* ----------------
 *	old interface macros
 * ----------------
 */
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
#define ItemIdHasValidHeapTupleForQualification(itemId, buffer, qual, nKeys, key) \
    heap_satisifies(itemId, buffer, qual, nKeys, key)

#define amvalidtup(b, lp, nkeys, key) \
    ((int) heap_satisifies(lp, b, NowTimeQual, nkeys, key))

/* 
 * keytest_tupdesc
 * keytest
 */
#define keytest_tupdesc(t, tupdesc, nkeys, keys) \
    ((int) heap_keytest(t, tupdesc, nkeys, keys))

#define keytest(t, rdesc, nkeys, keys) \
    ((int) heap_keytest(t, &(rdesc)->rd_att, nkeys, keys))

#endif	/* !defined(ValidIncluded) */
