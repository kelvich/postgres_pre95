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
#include "storage/bufpage.h"
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
	    test = (keys->func != (ScanKeyFunc) NULL) ? 
		    (int) (*(keys->func)) (keys->sk_data, atp) :
			    (int) fmgr(keys->sk_opr, keys->sk_data, atp);
	else
	    test = (keys->func != (ScanKeyFunc) NULL) ? 
		    (int) (*(keys->func)) (atp, keys->sk_data) :
			    (int) fmgr(keys->sk_opr, atp, keys->sk_data);
	
	if (!test == !(keys->sk_flags & SK_NEGATE))
	    return false;
    }
    
    return true;
}

/* ----------------
 *	heap_tuple_satisfies
 *
 *  Returns a valid HeapTuple if it satisfies the timequal and keytest.
 *  Returns NULL otherwise.  Used to be heap_satisifies (sic) which
 *  returned a boolean.  It now returns a tuple so that we can avoid doing two
 *  PageGetItem's per tuple.
 *
 *	Complete check of validity including LP_CTUP and keytest.
 *	This should perhaps be combined with valid somehow in the
 *	future.  (Also, additional rule tests/time range tests.)
 *
 *  on 8/21/92 mao says:  i rearranged the tests here to do keytest before
 *  SatisfiesTimeQual.  profiling indicated that even for vacuumed relations,
 *  time qual checking was more expensive than key testing.  time qual is
 *  least likely to fail, too.  we should really add the time qual test to
 *  the restriction and optimize it in the normal way.  this has interactions
 *  with joey's expensive function work.
 * ----------------
 */

HeapTuple
heap_tuple_satisfies(itemId, relation, disk_page, qual, nKeys, key)
    ItemId	itemId;
    Relation relation;
    PageHeader disk_page;
    TimeQual	qual;
    ScanKeySize	nKeys;
    ScanKey key;
{
    HeapTuple	tuple;
    bool res;

    if (! ItemIdIsUsed(itemId) || ItemIdIsLock(itemId))
	return NULL;

    tuple = (HeapTuple) PageGetItem((Page) disk_page, itemId);

    if (key != NULL)
	res = keytest(tuple, relation, nKeys, (struct skey *) key);
    else
	res = TRUE;

    if (res && (relation->rd_rel->relkind == 'u'
		|| HeapTupleSatisfiesTimeQual(tuple,qual)))
	return tuple;

    return (HeapTuple) NULL;
}

/* ----------------
 *	TupleUpdatedByCurXactAndCmd
 * ----------------
 */
bool
TupleUpdatedByCurXactAndCmd(t)
    HeapTuple	t;
{
    if (TransactionIdEquals((TransactionId) t->t_xmax,
							GetCurrentTransactionId()) &&
	t->t_cmax == (CID) GetCurrentCommandId())
	return true;

    return false;
}
