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
#include "access/tupdesc.h"
#include "utils/rel.h"
#include "storage/bufpage.h"

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
 *  heap_tuple_satisfies
 *    -- returns a tuple satisfying the timequal and scankey.
 */

extern
HeapTuple
heap_tuple_satisfies ARGS((
    ItemId	itemId,
	Relation relation,
    PageHeader  disk_page,
    TimeQual	qual,
    ScanKeySize	nKeys,
    ScanKey key
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

/* 
 * keytest_tupdesc
 * keytest
 */
#define keytest_tupdesc(t, tupdesc, nkeys, keys) \
    ((int) heap_keytest(t, tupdesc, nkeys, keys))

#define keytest(t, rdesc, nkeys, keys) \
    ((int) heap_keytest(t, &(rdesc)->rd_att, nkeys, keys))

#endif	/* !defined(ValidIncluded) */
