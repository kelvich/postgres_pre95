/*
 *  btree.c -- Implementation of Lehman and Yao's btree management algorithm
 *	       for Postgres.
 *
 *	This file contains only the public interface routines.
 */

#include "tmp/c.h"
#include "tmp/postgres.h"

#include "storage/bufmgr.h"
#include "storage/bufpage.h"
#include "storage/page.h"

#include "utils/log.h"
#include "utils/rel.h"
#include "utils/excid.h"

#include "access/heapam.h"
#include "access/genam.h"
#include "access/ftup.h"
#include "access/sdir.h"
#include "access/isop.h"

#include "/n/eden/users/mao/postgres/src/access/nbtree/nbtree.h"

RcsId("$Header$");

/* the global sequence number for insertions is defined here */
uint32	BTCurrSeqNo = 0;

void
btbuild(heap, index, natts, attnum, istrat, pcount, params)
    Relation heap;
    Relation index;
    AttributeNumber natts;
    AttributeNumber *attnum;
    IndexStrategy istrat;
    uint16 pcount;
    Datum *params;
{
    HeapScanDesc hscan;
    Buffer buffer;
    HeapTuple htup;
    IndexTuple itup;
    TupleDescriptor htupdesc, itupdesc;
    Datum *attdata;
    Boolean *null;
    InsertIndexResult res;
    int ntups;
    int i;
    BTItem btitem;
    TransactionId currxid;
    extern TransactionId GetCurrentTransactionId();

    /* first initialize the btree index metadata page */
    _bt_metapinit(index);

    /* get tuple descriptors for heap and index relations */
    htupdesc = RelationGetTupleDescriptor(heap);
    itupdesc = RelationGetTupleDescriptor(index);

    /* record current transaction id for uniqueness */
    currxid = GetCurrentTransactionId();
    BTSEQ_INIT();

    /* get space for data items that'll appear in the index tuple */
    attdata = (Datum *) palloc(natts * sizeof(Datum));
    null = (Boolean *) palloc(natts * sizeof(Boolean));

    /* start a heap scan */
    hscan = heap_beginscan(heap, 0, NowTimeQual, 0, (ScanKey) NULL);
    htup = heap_getnext(hscan, 0, &buffer);

    /* build the index */
    ntups = 0;

    while (HeapTupleIsValid(htup)) {

	ntups++;

	/*
	 *  For the current heap tuple, extract all the attributes
	 *  we use in this index, and note which are null.
	 */

	for (i = 1; i <= natts; i++) {
	    AttributeOffset attoff;
	    Boolean attnull;

	    /*
	     *  Offsets are from the start of the tuple, and are
	     *  zero-based; indices are one-based.  The next call
	     *  returns i - 1.  That's data hiding for you.
	     */

	    attoff = AttributeNumberGetAttributeOffset(i);
	    attdata[attoff] = HeapTupleGetAttributeValue(htup, buffer,
				    attnum[attoff], htupdesc, &attnull);
	    null[attoff] = (attnull ? 'n' : ' ');
	}

	/* form an index tuple and point it at the heap tuple */
	itup = FormIndexTuple(natts, itupdesc, attdata, null);
	itup->t_tid = htup->t_ctid;

	btitem = _bt_formitem(itup, currxid, BTSEQ_GET());
	res = _bt_doinsert(index, btitem);
	pfree(btitem);
	pfree(itup);
	pfree(res);

	/* do the next tuple in the heap */
	htup = heap_getnext(hscan, 0, &buffer);
    }

    /* okay, all heap tuples are indexed */
    heap_endscan(hscan);

    /*
     *  Since we just counted the tuples in the heap, we update its
     *  stats in pg_class to guarantee that the planner takes advantage
     *  of the index we just created.
     */

    UpdateStats(heap, ntups);
    UpdateStats(index, ntups);

    /* be tidy */
    pfree(null);
    pfree(attdata);
}

/*
 *  btinsert() -- insert an index tuple into a btree.
 *
 *	Descend the tree recursively, find the appropriate location for our
 *	new tuple, put it there, set its sequence number as appropriate, and
 *	return an InsertIndexResult to the caller.
 */

InsertIndexResult
btinsert(rel, itup)
    Relation rel;
    IndexTuple itup;
{
    BTItem btitem;
    int nbytes_btitem;
    InsertIndexResult res;

    BTSEQ_INIT();
    btitem = _bt_formitem(itup, GetCurrentTransactionId(), BTSEQ_GET());

    res = _bt_doinsert(rel, btitem);
    pfree(btitem);

    return (res);
}

/*
 *  btgettuple() -- Get the next tuple in the scan.
 */

char *
btgettuple(scan, dir)
    IndexScanDesc scan;
    ScanDirection dir;
{
    RetrieveIndexResult res;

    /*
     *  If we've already initialized this scan, we can just advance it
     *  in the appropriate direction.  If we haven't done so yet, we
     *  call a routine to get the first item in the scan.
     */

    if (ItemPointerIsValid(&(scan->currentItemData)))
	res = _bt_next(scan, dir);
    else
	res = _bt_first(scan, dir);

    return ((char *) res);
}

char *
btbeginscan(rel, fromEnd, keysz, scankey)
    Relation rel;
    Boolean fromEnd;
    uint16 keysz;
    ScanKey scankey;
{
    IndexScanDesc scan;
    StrategyNumber strat;

    /* first order the keys in the qualification */
    _bt_orderkeys(rel, &keysz, scankey);

    /* now get the scan */
    scan = RelationGetIndexScan(rel, fromEnd, keysz, scankey);

    /* finally, be sure that the scan exploits the tree order */
    scan->flags = 0x0;
    strat = _bt_getstrat(scan->relation, 1 /* XXX */,
			 scankey->data[0].procedure);

    if (strat == BTLessStrategyNumber || strat == BTLessEqualStrategyNumber)
	scan->scanFromEnd = true;
    else
	scan->scanFromEnd = false;

    return ((char *) scan);
}

/*
 *  btrescan() -- rescan an index relation
 */

void
btrescan(scan, fromEnd, scankey)
    IndexScanDesc scan;
    Boolean fromEnd;
    ScanKey scankey;
{
    /* we hold a read lock on the current page in the scan */
    if (ItemPointerIsValid(&(scan->currentItemData))) {
	_bt_unsetpagelock(scan->relation,
			  ItemPointerGetBlockNumber(&(scan->currentItemData), 0),
			  BT_READ);
	ItemPointerSetInvalid(&scan->currentItemData);
    }

    /* and we hold a read lock on the last marked item in the scan */
    if (ItemPointerIsValid(&(scan->currentMarkData))) {
	_bt_unsetpagelock(scan->relation,
			  ItemPointerGetBlockNumber(&(scan->currentMarkData), 0),
			  BT_READ);
	ItemPointerSetInvalid(&scan->currentMarkData);
    }

    /* reset the scan key */
    if (scan->numberOfKeys > 0) {
	bcopy( (Pointer)&scankey->data[0],
	       (Pointer)&scan->keyData.data[0],
	       scan->numberOfKeys * sizeof(scankey->data[0])
	     );
    }
}

/* stubs */
void btdelete() { ; }
void btendscan() { ; }
void btmarkpos() { ; }
void btrestrpos() { ; }
