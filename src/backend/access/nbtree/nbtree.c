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
#include "access/nbtree.h"
#include "access/funcindex.h"

RcsId("$Header$");

void
btbuild(heap, index, natts, attnum, istrat, pcount, params, finfo)
    Relation heap;
    Relation index;
    AttributeNumber natts;
    AttributeNumber *attnum;
    IndexStrategy istrat;
    uint16 pcount;
    Datum *params;
    FuncIndexInfo *finfo;
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

    /* first initialize the btree index metadata page */
    _bt_metapinit(index);

    /* get tuple descriptors for heap and index relations */
    htupdesc = RelationGetTupleDescriptor(heap);
    itupdesc = RelationGetTupleDescriptor(index);

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
	    attdata[attoff] = GetIndexValue(htup, 
					    htupdesc,
					    attoff, 
					    attnum, 
					    finfo, 
					    &attnull,
					    buffer);
	    null[attoff] = (attnull ? 'n' : ' ');
	}

	/* form an index tuple and point it at the heap tuple */
	itup = FormIndexTuple(natts, itupdesc, attdata, null);
	itup->t_tid = htup->t_ctid;

	btitem = _bt_formitem(itup);
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
 *	new tuple, put it there, set its unique OID as appropriate, and
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

    btitem = _bt_formitem(itup);

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

/*
 *  btbeginscan() -- start a scan on a btree index
 */

char *
btbeginscan(rel, fromEnd, keysz, scankey)
    Relation rel;
    Boolean fromEnd;
    uint16 keysz;
    ScanKey scankey;
{
    IndexScanDesc scan;
    StrategyNumber strat;
    BTScanOpaque so;

    /* first order the keys in the qualification */
    if (keysz > 1)
	_bt_orderkeys(rel, &keysz, scankey);

    /* now get the scan */
    scan = RelationGetIndexScan(rel, fromEnd, keysz, scankey);
    so = (BTScanOpaque) palloc(sizeof(BTScanOpaqueData));
    so->btso_curbuf = so->btso_mrkbuf = InvalidBuffer;
    scan->opaque = (Pointer) so;

    /* finally, be sure that the scan exploits the tree order */
    scan->scanFromEnd = false;
    scan->flags = 0x0;
    if (keysz > 0) {
	strat = _bt_getstrat(scan->relation, 1 /* XXX */,
			     scankey->data[0].procedure);

	if (strat == BTLessStrategyNumber
	    || strat == BTLessEqualStrategyNumber)
	    scan->scanFromEnd = true;
    } else {
	scan->scanFromEnd = true;
    }

    /* register scan in case we change pages it's using */
    _bt_regscan(scan);

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
    ItemPointer iptr;
    BTScanOpaque so;

    so = (BTScanOpaque) scan->opaque;

    /* we hold a read lock on the current page in the scan */
    if (ItemPointerIsValid(iptr = &(scan->currentItemData))) {
	_bt_relbuf(scan->relation, so->btso_curbuf, BT_READ);
	so->btso_curbuf = InvalidBuffer;
	ItemPointerSetInvalid(iptr);
    }

    /* and we hold a read lock on the last marked item in the scan */
    if (ItemPointerIsValid(iptr = &(scan->currentMarkData))) {
	_bt_relbuf(scan->relation, so->btso_mrkbuf, BT_READ);
	so->btso_mrkbuf = InvalidBuffer;
	ItemPointerSetInvalid(iptr);
    }

    /* reset the scan key */
    if (scan->numberOfKeys > 0) {
	bcopy( (Pointer)&scankey->data[0],
	       (Pointer)&scan->keyData.data[0],
	       scan->numberOfKeys * sizeof(scankey->data[0])
	     );
    }
}

/*
 *  btendscan() -- close down a scan
 */

void
btendscan(scan)
    IndexScanDesc scan;
{
    ItemPointer iptr;
    BTScanOpaque so;

    so = (BTScanOpaque) scan->opaque;

    /* release any locks we still hold */
    if (ItemPointerIsValid(iptr = &(scan->currentItemData))) {
	_bt_relbuf(scan->relation, so->btso_curbuf, BT_READ);
	so->btso_curbuf = InvalidBuffer;
	ItemPointerSetInvalid(iptr);
    }

    if (ItemPointerIsValid(iptr = &(scan->currentMarkData))) {
	_bt_relbuf(scan->relation, so->btso_mrkbuf, BT_READ);
	so->btso_mrkbuf = InvalidBuffer;
	ItemPointerSetInvalid(iptr);
    }

    /* don't need scan registered anymore */
    _bt_dropscan(scan);

    /* be tidy */
    pfree (scan->opaque);
}

/*
 *  btmarkpos() -- save current scan position
 */

void
btmarkpos(scan)
    IndexScanDesc scan;
{
    ItemPointer iptr;
    BTScanOpaque so;

    so = (BTScanOpaque) scan->opaque;

    /* release lock on old marked data, if any */
    if (ItemPointerIsValid(iptr = &(scan->currentMarkData))) {
	_bt_relbuf(scan->relation, so->btso_mrkbuf, BT_READ);
	so->btso_mrkbuf = InvalidBuffer;
	ItemPointerSetInvalid(iptr);
    }

    /* bump lock on currentItemData and copy to currentMarkData */
    if (ItemPointerIsValid(&(scan->currentItemData))) {
	so->btso_mrkbuf = _bt_getbuf(scan->relation,
				     BufferGetBlockNumber(so->btso_curbuf),
				     BT_READ);
	scan->currentMarkData = scan->currentItemData;
    }
}

/*
 *  btrestrpos() -- restore scan to last saved position
 */

void
btrestrpos(scan)
    IndexScanDesc scan;
{
    ItemPointer iptr;
    BTScanOpaque so;

    so = (BTScanOpaque) scan->opaque;

    /* release lock on current data, if any */
    if (ItemPointerIsValid(iptr = &(scan->currentItemData))) {
	_bt_relbuf(scan->relation, so->btso_curbuf, BT_READ);
	so->btso_curbuf = InvalidBuffer;
	ItemPointerSetInvalid(iptr);
    }

    /* bump lock on currentMarkData and copy to currentItemData */
    if (ItemPointerIsValid(&(scan->currentMarkData))) {
	so->btso_curbuf = _bt_getbuf(scan->relation,
				     BufferGetBlockNumber(so->btso_mrkbuf),
				     BT_READ);
				     
	scan->currentItemData = scan->currentMarkData;
    }
}

/* stubs */
void
btdelete(rel, tid)
    Relation rel;
    ItemPointer tid;
{
    /* adjust any active scans that will be affected by this deletion */
    _bt_adjscans(rel, tid);

    /* delete the data from the page */
    _bt_pagedel(rel, tid);
}
