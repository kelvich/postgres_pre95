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

#include "/n/eden/users/mao/postgres/src/access/nbtree/nbtree.h"

RcsId("$Header$");

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
	    attdata[attoff] = HeapTupleGetAttributeValue(htup, buffer,
				    attnum[attoff], htupdesc, &attnull);
	    null[attoff] = (attnull ? 'n' : ' ');
	}

	/* form an index tuple and point it at the heap tuple */
	itup = FormIndexTuple(natts, itupdesc, attdata, null);
	itup->t_tid = htup->t_ctid;

	res = btinsert(index, itup);
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

    _bt_dump(index);

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
    ScanKey itup_scankey;
    BTStack stack;
    Buffer buf;
    BlockNumber blkno;
    OffsetIndex itup_off;
    Datum itup_datum;
    Page page;
    uint32 *seqno;
    InsertIndexResult res;
    int natts;

    /* make a copy of the index tuple with room for the sequence number */
    nbytes_btitem = LONGALIGN(itup->t_size) +
			(sizeof(BTItemData) - sizeof(IndexTupleData));

    btitem = (BTItem) palloc(nbytes_btitem);
    bcopy((char *) itup, (char *) &(btitem->bti_itup), itup->t_size);
    btitem->bti_seqno = 0;

    /* we need a scan key to do our search, so build one */
    itup_scankey = _bt_mkscankey(rel, itup);
    natts = rel->rd_rel->relnatts;

    /* find the page containing this key */
    stack = _bt_search(rel, natts, itup_scankey, &buf);

    /* trade in our read lock for a write lock */
    _bt_relbuf(rel, buf, BT_READ);
    buf = _bt_getbuf(rel, blkno, BT_WRITE);

    /*
     *  If the page was split between the time that we surrendered our
     *  read lock and acquired our write lock, then this page may no
     *  longer be the right place for the key we want to insert.  In this
     *  case, we need to move right in the tree.  See Lehman and Yao for
     *  an excruciatingly precise description.
     */

    buf = _bt_moveright(rel, buf, natts, itup_scankey, BT_WRITE);

    /* do the insertion */
    res = _bt_insertonpg(rel, buf, stack, natts, itup_scankey, btitem);

    /* be tidy */
    _bt_freestack(stack);
    _bt_freeskey(itup_scankey);
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
    InsertIndexResult res;

    /*
     *  If we've already initialized this scan, we can just advance it
     *  in the appropriate direction.  If we haven't done so yet, we
     *  call a routine to get the first item in the scan.
     */

#ifdef notdef
    if (ItemPointerIsValid(&(scan->currentItemData)))
	res = _bt_next(scan, dir);
    else
	res = _bt_first(scan, dir);
#else /* notdef */
    res = (InsertIndexResult) NULL;
#endif /* notdef */

    return ((char *) res);
}

char *
btbeginscan(rel, fromEnd, keysz, scankey)
    Relation rel;
    Boolean fromEnd;
    uint16 keysz;
    ScanKey scankey;
{
    return ((char *) RelationGetIndexScan(rel, fromEnd, keysz, scankey));
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
			  ItemPointerGetBlockNumber(&(scan->currentItemData)),
			  BT_READ);
	ItemPointerSetInvalid(&scan->currentItemData);
    }

    /* and we hold a read lock on the last marked item in the scan */
    if (ItemPointerIsValid(&(scan->currentMarkData))) {
	_bt_unsetpagelock(scan->relation,
			  ItemPointerGetBlockNumber(&(scan->currentMarkData)),
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
