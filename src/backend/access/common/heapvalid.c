/* ----------------------------------------------------------------
 *   FILE
 *	heapvalid.c
 *	
 *   DESCRIPTION
 *	heap tuple qualification validity checking code
 *
 *   INTERFACE ROUTINES
 *	heap_satisifies
 *	heap_keytest
 *	TupleUpdatedByCurXactAndCmd
 *
 *   OLD INTERFACE ROUTINES (turned into macros in valid.h -cim 4/30/91)
 *	amvalidtup
 *	ItemIdHasValidHeapTupleForQualification
 *	keytest_tupdesc
 *	keytest
 *	
 *   NOTES
 *	
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#include "tmp/c.h"

#include "access/htup.h"
#include "access/skey.h"
#include "access/tqual.h"
#include "access/valid.h"
#include "access/xact.h"

#include "storage/buf.h"
#include "storage/bufmgr.h"
#include "storage/itemid.h"
#include "storage/page.h"
#include "utils/fmgr.h"
#include "utils/log.h"
#include "utils/rel.h"

RcsId("$Header$");

/* ----------------
 *	heap_keytest
 *
 *	Test a heap tuple with respect to a scan key.
 * ----------------
 */
bool
heap_keytest(t, tupdesc, nkeys, keys)
    HeapTuple		t;
    TupleDescriptor 	tupdesc;
    int			nkeys;
    struct skey 	keys[];
{
    Boolean	isnull;
    DATUM	atp;
    int		test;

    for (; nkeys--; keys++) {
	atp = heap_getattr(t, InvalidBuffer,
			   keys->sk_attnum,  tupdesc, &isnull);
	
	if (isnull)
	    /* XXX eventually should check if SK_ISNULL */
	    return false;

	if (keys->sk_flags & SK_COMMUTE)
	    test = (int) (*(keys->func)) (keys->sk_data, atp);
	else
	    test = (int) (*(keys->func)) (atp, keys->sk_data);
	
	if (!test == !(keys->sk_flags & SK_NEGATE))
	    return false;
    }
    
    return true;
}

/* ----------------
 *	heap_satisifies
 *
 *	Complete check of validity including LP_CTUP and keytest.
 *	This should perhaps be combined with valid somehow in the
 *	future.  (Also, additional rule tests/time range tests.)
 * ----------------
 */
bool
heap_satisifies(itemId, buffer, qual, nKeys, key)
    ItemId	itemId;
    Buffer	buffer;
    TimeQual	qual;
    ScanKeySize	nKeys;
    struct skey	*key;
{
    HeapTuple	tuple;
    Relation	relation = BufferGetRelation(buffer);

    if (! ItemIdIsUsed(itemId) ||
	ItemIdIsContinuation(itemId) || ItemIdIsLock(itemId))
	return false;

    tuple = (HeapTuple)
	PageGetItem(BufferGetBlock(buffer), itemId);

    if (relation->rd_rel->relkind == 'u')
	return (bool)
	    keytest(tuple, relation, nKeys, key);

    if (! HeapTupleSatisfiesTimeQual(tuple, qual) ||
	! keytest(tuple, relation, nKeys, key))
	return false;
    
    return true;
}

/* ----------------
 *	TupleUpdatedByCurXactAndCmd
 * ----------------
 */
bool
TupleUpdatedByCurXactAndCmd(t)
    HeapTuple	t;
{
    if (TransactionIdEquals(t->t_xmax, GetCurrentTransactionId()) &&
	t->t_cmax == (CID) GetCurrentCommandId())
	return true;

    return false;
}
