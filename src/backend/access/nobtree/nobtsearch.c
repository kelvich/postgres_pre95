/*
 *  btsearch.c -- search code for postgres btrees.
 */

#include "tmp/c.h"

#ifdef NOBTREE
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
#include "access/skey.h"
#include "access/sdir.h"
#include "access/tupmacs.h"
#include "access/nobtree.h"

RcsId("$Header$");

/*
 *  _nobt_search() -- Search for a scan key in the index.
 *
 *	This routine is actually just a helper that sets things up and
 *	calls a recursive-descent search routine on the tree.
 */

NOBTStack
_nobt_search(rel, keysz, scankey, bufP)
    Relation rel;
    int keysz;
    ScanKey scankey;
    Buffer *bufP;
{
    *bufP = _nobt_getroot(rel, NOBT_READ);
    return (_nobt_searchr(rel, keysz, scankey, bufP, (NOBTStack) NULL));
}

/*
 *  _nobt_searchr() -- Search the tree recursively for a particular scankey.
 *
 *	This routine takes a pointer to a sequence number.  As we descend
 *	the tree, any time we see a key exactly equal to the one being
 *	inserted, we save its sequence number plus one.  If we are inserting
 *	a tuple, then we can use this number to distinguish among duplicates
 *	and to guarantee that we never store the same sequence number for the
 *	same key at any level in the tree.  This is an important guarantee,
 *	since the Lehman and Yao algorithm relies on being able to find
 *	its place in parent pages by looking at keys in the parent.
 */

NOBTStack
_nobt_searchr(rel, keysz, scankey, bufP, stack_in)
    Relation rel;
    int keysz;
    ScanKey scankey;
    Buffer *bufP;
    NOBTStack stack_in;
{
    NOBTStack stack;
    OffsetIndex offind;
    Page page;
    NOBTPageOpaque opaque;
    PageNumber newpg;
    BlockNumber par_blkno;
    BlockNumber blkno;
    ItemId itemid;
    NOBTIItem btitem;
    NOBTIItem item_save;
    int item_nbytes;

    page = BufferGetPage(*bufP, 0);
    opaque = (NOBTPageOpaque) PageGetSpecialPointer(page);

    /* if this is a leaf page, we're done */
    if (opaque->nobtpo_flags & NOBTP_LEAF)
	return (stack_in);

    /*
     *  Find the appropriate item on the internal page, and get the child
     *  page that it points to.
     */

    par_blkno = BufferGetBlockNumber(*bufP);
    offind = _nobt_binsrch(rel, *bufP, keysz, scankey, NOBT_DESCENT);
    itemid = PageGetItemId(page, offind);
    btitem = (NOBTIItem) PageGetItem(page, itemid);
    blkno = btitem->nobtii_child;

    /* save current, next key on stack */
    item_nbytes = ItemIdGetLength(itemid);
    item_save = (NOBTIItem) palloc(item_nbytes);
    bcopy((char *) btitem, (char *) item_save, item_nbytes);
    stack = (NOBTStack) palloc(sizeof(NOBTStackData));
    stack->nobts_offset = offind;
    stack->nobts_blkno = par_blkno;
    stack->nobts_btitem = item_save;

#ifndef	NORMAL
    /* if there's no "next" key on this page, use the high key */
    if (offind++ >= PageGetMaxOffsetIndex(page)) {
	if (opaque->nobtpo_next == P_NONE) {
	    stack->nobts_nxtitem = (NOBTIItem) NULL;
	} else {
	    itemid = PageGetItemId(page, 0);
	    item_nbytes = ItemIdGetLength(itemid);
	    item_save = (NOBTIItem) palloc(item_nbytes);
	    btitem = (NOBTIItem) PageGetItem(page, itemid);
	    bcopy((char *) btitem, (char *) item_save, item_nbytes);
	    stack->nobts_nxtitem = item_save;
	}
    } else {
	itemid = PageGetItemId(page, offind);
	item_nbytes = ItemIdGetLength(itemid);
	item_save = (NOBTIItem) palloc(item_nbytes);
	btitem = (NOBTIItem) PageGetItem(page, itemid);
	bcopy((char *) btitem, (char *) item_save, item_nbytes);
	stack->nobts_nxtitem = item_save;
    }
#endif	/* ndef NORMAL */

    stack->nobts_parent = stack_in;

    /* drop the read lock on the parent page and acquire one on the child */
    _nobt_relbuf(rel, *bufP, NOBT_READ);
    *bufP = _nobt_getbuf(rel, blkno, NOBT_READ);

    /*
     *  Race -- the page we just grabbed may have split since we read its
     *  pointer in the parent.  If it has, we may need to move right to its
     *  new sibling.  Do that.
     */

    *bufP = _nobt_moveright(rel, *bufP, keysz, scankey, NOBT_READ, stack);

    /* okay, all set to move down a level */
    return (_nobt_searchr(rel, keysz, scankey, bufP, stack));
}

/*
 *  _nobt_moveright() -- move right in the btree if necessary.
 *
 *	When we drop and reacquire a pointer to a page, it is possible that
 *	the page has changed in the meanwhile.  If this happens, we're
 *	guaranteed that the page has "split right" -- that is, that any
 *	data that appeared on the page originally is either on the page
 *	or strictly to the right of it.
 *
 *	This routine decides whether or not we need to move right in the
 *	tree by examining the high key entry on the page.  If that entry
 *	is strictly less than one we expect to be on the page, then our
 *	picture of the page is incorrect and we need to move right.
 *
 *	On entry, we have the buffer pinned and a lock of the proper type.
 *	If we move right, we release the buffer and lock and acquire the
 *	same on the right sibling.
 */

Buffer
_nobt_moveright(rel, buf, keysz, scankey, access, stack)
    Relation rel;
    Buffer buf;
    int keysz;
    ScanKey scankey;
    int access;
    NOBTStack stack;
{
    Page page;
    PageNumber right;
    PageNumber newpg;
    NOBTPageOpaque opaque;
    ItemId hikey;
    ItemId itemid;
    BlockNumber rblkno;
    IndexTuple stktupa, chktupa;
    IndexTuple stktupb, chktupb;
    NOBTIItem chkiitem;
    NOBTLItem chklitem;
    char *stkstra, *chkstra;
    char *stkstrb, *chkstrb;
    int cmpsiza, cmpsizb;
    bool inconsistent;
    bool isleaf;

    page = BufferGetPage(buf, 0);
    opaque = (NOBTPageOpaque) PageGetSpecialPointer(page);
    inconsistent = false;

    if (opaque->nobtpo_flags & NOBTP_LEAF)
	isleaf = true;
    else
	isleaf = false;

    /*
     *  For the no-overwrite implementation, here are the things that can
     *  cause us to have to move around in the tree:
     *
     *    +  The page we have just come to has just split, but the parent
     *	     now contains the correct entries for the new children, so the
     *       tree is consistent.  In this case, we adjust the parent pointer
     *       stack, move right as far as necessary, and continue.
     *
     *    +  The tree is genuinely inconsistent -- there was a failure during
     *       the write of the modified tree to disk, and some set of pages
     *       failed to make it out.  In this case, we must immediately repair
     *       the damage.  We're on one of the modified pages.
     *
     *  Short-term Lehman and Yao locking guarantees that if no failure has
     *  occurred, then case one must have happened.  The no-overwrite algorithm
     *	by Sullivan guarantees that we can recover from two in the face of
     *  any failure.
     */

#ifdef	SHADOW
    /* first, if this page has been replaced, then move to the new page */
    while ((newpg = opaque->nobtpo_replaced) != P_NONE) {
	_nobt_relbuf(rel, buf, access);
	buf = _nobt_getbuf(rel, newpg, access);
	page = BufferGetPage(buf, 0);
	opaque = (NOBTPageOpaque) PageGetSpecialPointer(page);
	inconsistent = true;
    }
#endif	/* SHADOW */

    /*
     *  If the key range on the parent doesn't match the key range on the
     *  child at which we've arrived (after possibly following "replaced"
     *  links above), then the tree may be inconsistent.  The hairy code
     *  structure (next page, prev page == P_NONE -- four cases) is due to
     *  the fact that the high and low key in the index are handled specially
     *  to avoid expensive updates.
     */

#ifndef	NORMAL
    if (opaque->nobtpo_next == P_NONE) {
	if (opaque->nobtpo_prev == P_NONE) {
	    if (stack != (NOBTStack) NULL)
		inconsistent = true;
	} else if (stack == NULL) {
	    inconsistent = true;
	} else {
	    stkstra = ((char *) stack->nobts_btitem) + sizeof(NOBTIItemData);
	    if (isleaf) {
		chklitem = (NOBTLItem) PageGetItem(page, PageGetItemId(page, 0));
		chktupa = &(chklitem->nobtli_itup);
		chkstra = ((char *) chktupa) + sizeof(IndexTupleData);
		cmpsiza = IndexTupleSize(chktupa) - sizeof(IndexTupleData);
	    } else {
		chkiitem = (NOBTIItem) PageGetItem(page, PageGetItemId(page, 0));
		chkstra = ((char *) chkiitem) + sizeof(NOBTIItemData);
		cmpsiza = NOBTIItemGetSize(chkiitem) - sizeof(NOBTIItemData);
	    }
	    if (bcmp(stkstra, chkstra, cmpsiza) != 0)
		inconsistent = true;
	}
    } else {
	if (stack == NULL || stack->nobts_nxtitem == (NOBTIItem) NULL) {
	    inconsistent = true;
	} else if (opaque->nobtpo_prev == P_NONE) {
	    if (stack->nobts_offset != 0)
		inconsistent = true;
	} else {
	    /* stack tuple a, check tuple a are the low key on the page */
	    stkstra = ((char *) stack->nobts_btitem) + sizeof(NOBTIItemData);
	    if (isleaf) {
		chklitem = (NOBTLItem) PageGetItem(page, PageGetItemId(page, 1));
		chktupa = &(chklitem->nobtli_itup);
		chkstra = ((char *) chktupa) + sizeof(IndexTupleData);
		cmpsiza = IndexTupleSize(chktupa) - sizeof(IndexTupleData);
	    } else {
		chkiitem = (NOBTIItem) PageGetItem(page, PageGetItemId(page, 1));
		chkstra = ((char *) chkiitem) + sizeof(NOBTIItemData);
		cmpsiza = NOBTIItemGetSize(chkiitem) - sizeof(NOBTIItemData);
	    }

	    /* stack tuple b, check tuple b are the high key */
	    stkstrb = ((char *) stack->nobts_nxtitem) + sizeof(NOBTIItemData);
	    if (isleaf) {
		chklitem = (NOBTLItem) PageGetItem(page, PageGetItemId(page, 0));
		chktupb = &(chklitem->nobtli_itup);
		chkstrb = ((char *) chktupb) + sizeof(IndexTupleData);
		cmpsizb = IndexTupleSize(chktupb) - sizeof(IndexTupleData);
	    } else {
		chkiitem = (NOBTIItem) PageGetItem(page, PageGetItemId(page, 0));
		chkstrb = ((char *) chkiitem) + sizeof(NOBTIItemData);
		cmpsizb = NOBTIItemGetSize(chkiitem) - sizeof(NOBTIItemData);
	    }

	    if (bcmp(stkstra, chkstra, cmpsiza) != 0
		|| bcmp(stkstrb, chkstrb, cmpsizb) != 0)
		inconsistent = true;
	}
    }
#endif	/* ndef NORMAL */

    /* XXX XXX XXX peer pointer check? */

    /* if the tree may be inconsistent, it has to be fixed... */
    if (inconsistent) {
	elog(NOTICE, "inconsistency detected, should do something here...");
    }

    return (buf);
}

/*
 *  _nobt_skeycmp() -- compare a scan key to a particular item on a page using
 *		     a requested strategy (<, <=, =, >=, >).
 *
 *	We ignore the sequence numbers stored in the btree item here.  Those
 *	numbers are intended for use internally only, in repositioning a
 *	scan after a page split.  They do not impose any meaningful ordering.
 *
 *	The comparison is A <op> B, where A is the scan key and B is the
 *	tuple pointed at by itemid on page.
 */

bool
_nobt_skeycmp(rel, keysz, scankey, page, itemid, strat)
    Relation rel;
    Size keysz;
    ScanKey scankey;
    Page page;
    ItemId itemid;
    StrategyNumber strat;
{
    NOBTIItem iitem;
    NOBTLItem litem;
    IndexTuple indexTuple;
    TupleDescriptor tupDes;
    ScanKeyEntry entry;
    int i;
    Datum attrDatum;
    Datum keyDatum;
    NOBTPageOpaque opaque;
    bool compare;
    bool isNull;
    char *tempd;

    opaque = (NOBTPageOpaque) PageGetSpecialPointer(page);

    tupDes = RelationGetTupleDescriptor(rel);
    if (opaque->nobtpo_flags & NOBTP_LEAF) {
	litem = (NOBTLItem) PageGetItem(page, itemid);
	indexTuple = &(litem->nobtli_itup);

	/* see if the comparison is true for all of the key attributes */
	for (i=1; i <= keysz; i++) {

	    entry = &scankey->data[i-1];
	    attrDatum = IndexTupleGetAttributeValue(indexTuple,
						    entry->attributeNumber,
						    tupDes, &isNull);
	    keyDatum  = entry->argument;

	    compare = _nobt_invokestrat(rel, i, strat, keyDatum, attrDatum);
	    if (!compare)
		return (false);
	}
    } else {
	iitem = (NOBTIItem) PageGetItem(page, itemid);
	tempd = ((char *) iitem) + sizeof(NOBTIItemData);

	entry = &scankey->data[0];
	keyDatum  = entry->argument;
	attrDatum = (Datum) fetchatt(((struct attribute **) tupDes), tempd);

	compare = _nobt_invokestrat(rel, 1, strat, keyDatum, attrDatum);
	if (!compare)
	    return (false);
    }
    return (true);
}

/*
 *  _nobt_binsrch() -- Do a binary search for a key on a particular page.
 *
 *	The scankey we get has the compare function stored in the procedure
 *	entry of each data struct.  We invoke this regproc to do the
 *	comparison for every key in the scankey.  _nobt_binsrch() returns
 *	the OffsetIndex of the first matching key on the page, or the
 *	OffsetIndex at which the matching key would appear if it were
 *	on this page.
 *
 *	By the time this procedure is called, we're sure we're looking
 *	at the right page -- don't need to walk right.  _nobt_binsrch() has
 *	no lock or refcount side effects on the buffer.
 */

OffsetIndex
_nobt_binsrch(rel, buf, keysz, scankey, srchtype)
    Relation rel;
    Buffer buf;
    int keysz;
    ScanKey scankey;
    int srchtype;
{
    TupleDescriptor itupdesc;
    Page page;
    NOBTPageOpaque opaque;
    OffsetIndex low, mid, high;
    bool match;
    int result;

    page = BufferGetPage(buf, 0);
    opaque = (NOBTPageOpaque) PageGetSpecialPointer(page);

    /* by convention, item 0 on any non-leftmost page is the high key */
    if (opaque->nobtpo_next != P_NONE)
	low = 1;
    else
	low = 0;

    high = PageGetMaxOffsetIndex(page);

    /*
     *  Since for non-leftmost pages, the zeroeth item on the page is the
     *  high key, there are two notions of emptiness.  One is if nothing
     *  appears on the page.  The other is if nothing but the high key does.
     */

    if (PageIsEmpty(page) || (opaque->nobtpo_next != P_NONE && high == low))
	return (low);

    itupdesc = RelationGetTupleDescriptor(rel);
    match = false;

    while ((high - low) > 1) {
	mid = low + ((high - low) / 2);
	result = _nobt_compare(rel, itupdesc, page, keysz, scankey, mid);

	if (result > 0)
	    low = mid;
	else if (result < 0)
	    high = mid - 1;
	else {
	    match = true;
	    break;
	}
    }

    /* if we found a match, we want to find the first one on the page */
    if (match) {
	return (_nobt_firsteq(rel, itupdesc, page, keysz, scankey, mid));
    } else {

	/*
	 *  We terminated because the endpoints got too close together.  There
	 *  are two cases to take care of.
	 *
	 *  For non-insertion searches on internal pages, we want to point at
	 *  the last key <, or first key =, the scankey on the page.  This
	 *  guarantees that we'll descend the tree correctly.
	 *
	 *  For all other cases, we want to point at the first key >=
	 *  the scankey on the page.  This guarantees that scans and
	 *  insertions will happen correctly.
	 */

	opaque = (NOBTPageOpaque) PageGetSpecialPointer(page);
	if (!(opaque->nobtpo_flags & NOBTP_LEAF) && srchtype == NOBT_DESCENT) {

	    /*
	     *  We want the last key <, or first key ==, the scan key.
	     */

	    result = _nobt_compare(rel, itupdesc, page, keysz, scankey, high);

	    if (result == 0) {
		return (_nobt_firsteq(rel, itupdesc, page, keysz, scankey, high));
	    } else if (result > 0) {
		return (high);
	    } else {
		return (low);
	    }
	    /* NOTREACHED */
	} else {

	    /* we want the first key >= the scan key */
	    result = _nobt_compare(rel, itupdesc, page, keysz, scankey, low);
	    if (result <= 0) {
		return (low);
	    } else {
		if (low == high)
		    return (low + 1);

		result = _nobt_compare(rel, itupdesc, page, keysz, scankey, high);
		if (result <= 0)
		    return (high);
		else
		    return (high + 1);
	    }
	}
	/* NOTREACHED */
    }
}

OffsetIndex
_nobt_firsteq(rel, itupdesc, page, keysz, scankey, offind)
    Relation rel;
    TupleDescriptor itupdesc;
    Page page;
    Size keysz;
    ScanKey scankey;
    OffsetIndex offind;
{
    NOBTPageOpaque opaque;
    OffsetIndex limit;

    opaque = (NOBTPageOpaque) PageGetSpecialPointer(page);

    /* skip the high key, if any */
    if (opaque->nobtpo_next != P_NONE)
	limit = 1;
    else
	limit = 0;

    /* walk backwards looking for the first key in the chain of duplicates */
    while (offind > limit
	   && _nobt_compare(rel, itupdesc, page,
			  keysz, scankey, offind - 1) == 0) {
	offind--;
    }

    return (offind);
}

/*
 *  _nobt_compare() -- Compare scankey to a particular tuple on the page.
 *
 *	This routine returns:
 *	    -1 if scankey < tuple at offind;
 *	     0 if scankey == tuple at offind;
 *	    +1 if scankey > tuple at offind.
 *
 *	In order to avoid having to propogate changes up the tree any time
 *	a new minimal key is inserted, the leftmost entry on the leftmost
 *	page is less than all possible keys, by definition.
 */

int
_nobt_compare(rel, itupdesc, page, keysz, scankey, offind)
    Relation rel;
    TupleDescriptor itupdesc;
    Page page;
    int keysz;
    ScanKey scankey;
    OffsetIndex offind;
{
    Datum datum;
    NOBTLItem btlitem;
    NOBTIItem btiitem;
    ItemId itemid;
    IndexTuple itup;
    NOBTPageOpaque opaque;
    ScanKeyEntry entry;
    AttributeNumber attno;
    int result;
    int i;
    char *tempd;
    Boolean null;
    bool isleaf;

    /*
     *  If this is a leftmost internal page, and if our comparison is
     *  with the first key on the page, then the item at that position is
     *  by definition less than the scan key.
     */

    opaque = (NOBTPageOpaque) PageGetSpecialPointer(page);

    if (opaque->nobtpo_flags & NOBTP_LEAF)
	isleaf = true;
    else
	isleaf = false;

    if (!isleaf && opaque->nobtpo_prev == P_NONE && offind == 0) {
	    itemid = PageGetItemId(page, offind);

	    /*
	     *  If the item on the page is equal to the scankey, that's
	     *  okay to admit.  We just can't claim that the first key on
	     *  the page is greater than anything.
	     */

	    if (_nobt_skeycmp(rel, keysz, scankey, page, itemid,
			    NOBTEqualStrategyNumber)) {
		return (0);
	    }
	    return (1);
    }

    /*
     *  The scan key is set up with the attribute number associated with each
     *  term in the key.  It is important that, if the index is multi-key,
     *  the scan contain the first k key attributes, and that they be in
     *  order.  If you think about how multi-key ordering works, you'll
     *  understand why this is.
     *
     *  We don't test for violation of this condition here.
     */

    entry = &(scankey->data[0]);

    if (isleaf) {
	btlitem = (NOBTLItem) PageGetItem(page, PageGetItemId(page, offind));
	itup = &(btlitem->nobtli_itup);
	attno = entry->attributeNumber;
	datum = (Datum) IndexTupleGetAttributeValue(itup, attno,
						    itupdesc, &null);
    } else {
	btiitem = (NOBTIItem) PageGetItem(page, PageGetItemId(page, offind));
	tempd = ((char *) btiitem) + sizeof(NOBTIItemData);
	datum = (Datum) fetchatt(((struct attribute **) itupdesc), tempd);
    }

    result = (entry->func != (ScanKeyFunc) NULL) ?
	    (int) (*(entry->func))(entry->argument, datum) :
		    (int) fmgr(entry->procedure, entry->argument, datum);
    return (result);
}

/*
 *  _nobt_next() -- Get the next item in a scan.
 *
 *	On entry, we have a valid currentItemData in the scan, and a
 *	read lock on the page that contains that item.  We do not have
 *	the page pinned.  We return the next item in the scan.  On
 *	exit, we have the page containing the next item locked but not
 *	pinned.
 */

RetrieveIndexResult
_nobt_next(scan, dir)
    IndexScanDesc scan;
    ScanDirection dir;
{
    Relation rel;
    NOBTPageOpaque opaque;
    Buffer buf;
    Page page;
    OffsetIndex offind, maxoff;
    RetrieveIndexResult res;
    BlockNumber blkno;
    ItemPointer current;
    ItemPointer iptr;
    NOBTLItem btitem;
    IndexTuple itup;
    NOBTScanOpaque so;

    rel = scan->relation;
    so = (NOBTScanOpaque) scan->opaque;
    current = &(scan->currentItemData);

    if (!BufferIsValid(so->nobtso_curbuf))
	so->nobtso_curbuf = _nobt_getbuf(rel, ItemPointerGetBlockNumber(current),
				     NOBT_READ);

    /* we still have the buffer pinned and locked */
    buf = so->nobtso_curbuf;
    blkno = BufferGetBlockNumber(buf);

    /* step one tuple in the appropriate direction */
    if (!_nobt_step(scan, &buf, dir))
	return ((RetrieveIndexResult) NULL);

    /* by here, current is the tuple we want to return */
    offind = ItemPointerGetOffsetNumber(current, 0) - 1;
    page = BufferGetPage(buf, 0);
    btitem = (NOBTLItem) PageGetItem(page, PageGetItemId(page, offind));
    itup = &btitem->nobtli_itup;

    if (_nobt_checkqual(scan, itup)) {
	iptr = (ItemPointer) palloc(sizeof(ItemPointerData));
	bcopy((char *) &(itup->t_tid), (char *) iptr, sizeof(ItemPointerData));
	res = ItemPointerFormRetrieveIndexResult(current, iptr);

	/* remember which buffer we have pinned and locked */
	so->nobtso_curbuf = buf;
    } else {
	ItemPointerSetInvalid(current);
	so->nobtso_curbuf = InvalidBuffer;
	_nobt_relbuf(rel, buf, NOBT_READ);
	res = (RetrieveIndexResult) NULL;
    }

    return (res);
}

/*
 *  _nobt_first() -- Find the first item in a scan.
 *
 *	We need to be clever about the type of scan, the operation it's
 *	performing, and the tree ordering.  We return the RetrieveIndexResult
 *	of the first item in the tree that satisfies the qualification
 *	associated with the scan descriptor.  On exit, the page containing
 *	the current index tuple is read locked and pinned, and the scan's
 *	opaque data entry is updated to include the buffer.
 */

RetrieveIndexResult
_nobt_first(scan, dir)
    IndexScanDesc scan;
    ScanDirection dir;
{
    Relation rel;
    TupleDescriptor itupdesc;
    Buffer buf;
    Page page;
    NOBTStack stack;
    OffsetIndex offind, maxoff;
    NOBTLItem btitem;
    IndexTuple itup;
    ItemPointer current;
    ItemPointer iptr;
    BlockNumber blkno;
    StrategyNumber strat;
    RetrieveIndexResult res;
    RegProcedure proc;
    int result;
    NOBTScanOpaque so;
    ScanKeyData skdata;

    /* if we just need to walk down one edge of the tree, do that */
    if (scan->scanFromEnd)
	return (_nobt_endpoint(scan, dir));

    rel = scan->relation;
    itupdesc = RelationGetTupleDescriptor(scan->relation);
    current = &(scan->currentItemData);
    so = (NOBTScanOpaque) scan->opaque;

    /*
     *  Okay, we want something more complicated.  What we'll do is use
     *  the first item in the scan key passed in (which has been correctly
     *  ordered to take advantage of index ordering) to position ourselves
     *  at the right place in the scan.
     */

    /*
     *  XXX -- The attribute number stored in the scan key is the attno
     *	       in the heap relation.  We need to transmogrify this into
     *         the index relation attno here.  For the moment, we have
     *	       hardwired attno == 1.
     */
    proc = index_getprocid(rel, 1, NOBTORDER_PROC);
    ScanKeyEntryInitialize(&skdata, 0x0, 1, proc,
			   scan->keyData.data[0].argument);

    stack = _nobt_search(rel, 1, &skdata, &buf);
    _nobt_freestack(stack);

    /* find the nearest match to the manufactured scan key on the page */
    offind = _nobt_binsrch(rel, buf, 1, &skdata, NOBT_DESCENT);
    page = BufferGetPage(buf, 0);
    maxoff = PageGetMaxOffsetIndex(page);

    if (offind > maxoff)
	offind = maxoff;

    blkno = BufferGetBlockNumber(buf);
    ItemPointerSet(current, 0, blkno, 0, offind + 1);

    /* now find the right place to start the scan */
    result = _nobt_compare(rel, itupdesc, page, 1, &skdata, offind);
    strat = _nobt_getstrat(rel, 1, scan->keyData.data[0].procedure);

    switch (strat) {
      case NOBTLessStrategyNumber:
	if (result <= 0) {
	    do {
		if (!_nobt_twostep(scan, &buf, BackwardScanDirection))
		    break;

		offind = ItemPointerGetOffsetNumber(current, 0) - 1;
		page = BufferGetPage(buf, 0);
		result = _nobt_compare(rel, itupdesc, page, 1, &skdata, offind);
	    } while (result <= 0);

	    /* if this is true, the key we just looked at is gone */
	    if (result > 0)
		(void) _nobt_twostep(scan, &buf, ForwardScanDirection);
	}
	break;

      case NOBTLessEqualStrategyNumber:
	if (result >= 0) {
	    do {
		if (!_nobt_twostep(scan, &buf, ForwardScanDirection))
		    break;

		offind = ItemPointerGetOffsetNumber(current, 0) - 1;
		page = BufferGetPage(buf, 0);
		result = _nobt_compare(rel, itupdesc, page, 1, &skdata, offind);
	    } while (result >= 0);

	    if (result < 0)
		(void) _nobt_twostep(scan, &buf, BackwardScanDirection);
	}
	break;

      case NOBTEqualStrategyNumber:
	if (result != 0) {
	  _nobt_relbuf(scan->relation, buf, NOBT_READ);
	  so->nobtso_curbuf = InvalidBuffer;
	  ItemPointerSetInvalid(&(scan->currentItemData));
	  return ((RetrieveIndexResult) NULL);
	}
	break;

      case NOBTGreaterEqualStrategyNumber:
	if (result < 0) {
	    do {
		if (!_nobt_twostep(scan, &buf, BackwardScanDirection))
		    break;

		offind = ItemPointerGetOffsetNumber(current, 0) - 1;
		page = BufferGetPage(buf, 0);
		result = _nobt_compare(rel, itupdesc, page, 1, &skdata, offind);
	    } while (result < 0);
	}
	break;

      case NOBTGreaterStrategyNumber:
	if (result >= 0) {
	    do {
		if (!_nobt_twostep(scan, &buf, ForwardScanDirection))
		    break;

		offind = ItemPointerGetOffsetNumber(current, 0) - 1;
		page = BufferGetPage(buf, 0);
		result = _nobt_compare(rel, itupdesc, page, 1, &skdata, offind);
	    } while (result >= 0);
	}
	break;
    }

    /* okay, current item pointer for the scan is right */
    offind = ItemPointerGetOffsetNumber(current, 0) - 1;
    page = BufferGetPage(buf, 0);
    btitem = (NOBTLItem) PageGetItem(page, PageGetItemId(page, offind));
    itup = &btitem->nobtli_itup;

    if (_nobt_checkqual(scan, itup)) {
	iptr = (ItemPointer) palloc(sizeof(ItemPointerData));
	bcopy((char *) &(itup->t_tid), (char *) iptr, sizeof(ItemPointerData));
	res = ItemPointerFormRetrieveIndexResult(current, iptr);

	/* remember which buffer we have pinned */
	so->nobtso_curbuf = buf;
    } else {
	ItemPointerSetInvalid(current);
	so->nobtso_curbuf = InvalidBuffer;
	_nobt_relbuf(rel, buf, NOBT_READ);
	res = (RetrieveIndexResult) NULL;
    }

    return (res);
}

/*
 *  _nobt_step() -- Step one item in the requested direction in a scan on
 *		  the tree.
 *
 *	If no adjacent record exists in the requested direction, return
 *	false.  Else, return true and set the currentItemData for the
 *	scan to the right thing.
 */

bool
_nobt_step(scan, bufP, dir)
    IndexScanDesc scan;
    Buffer *bufP;
    ScanDirection dir;
{
    Page page;
    NOBTPageOpaque opaque;
    OffsetIndex offind, maxoff;
    OffsetIndex start;
    BlockNumber blkno;
    BlockNumber obknum;
    NOBTScanOpaque so;
    ItemPointer current;
    Relation rel;

    rel = scan->relation;
    current = &(scan->currentItemData);
    offind = ItemPointerGetOffsetNumber(current, 0) - 1;
    page = BufferGetPage(*bufP, 0);
    opaque = (NOBTPageOpaque) PageGetSpecialPointer(page);
    so = (NOBTScanOpaque) scan->opaque;
    maxoff = PageGetMaxOffsetIndex(page);

    /* get the next tuple */
    if (ScanDirectionIsForward(dir)) {
	if (offind < maxoff) {
	    offind++;
	} else {

	    /* if we're at end of scan, release the buffer and return */
	    if ((blkno = opaque->nobtpo_next) == P_NONE) {
		_nobt_relbuf(rel, *bufP, NOBT_READ);
		ItemPointerSetInvalid(current);
		*bufP = so->nobtso_curbuf = InvalidBuffer;
		return (false);
	    } else {

		/* walk right to the next page with data */
		_nobt_relbuf(rel, *bufP, NOBT_READ);
		for (;;) {
		    *bufP = _nobt_getbuf(rel, blkno, NOBT_READ);
		    page = BufferGetPage(*bufP, 0);
		    opaque = (NOBTPageOpaque) PageGetSpecialPointer(page);
		    maxoff = PageGetMaxOffsetIndex(page);
		    if (opaque->nobtpo_next != P_NONE)
			start = 1;
		    else
			start = 0;

		    if (!PageIsEmpty(page) && start <= maxoff) {
			break;
		    } else {
			blkno = opaque->nobtpo_next;
			_nobt_relbuf(rel, *bufP, NOBT_READ);
			if (blkno == P_NONE) {
			    *bufP = so->nobtso_curbuf = InvalidBuffer;
			    ItemPointerSetInvalid(current);
			    return (false);
			}
		    }
		}
		offind = start;
	    }
	}
    } else if (ScanDirectionIsBackward(dir)) {

	/* remember that high key is item zero on non-rightmost pages */
	if (opaque->nobtpo_next == P_NONE)
	    start = 0;
	else
	    start = 1;

	if (offind > start) {
	    offind--;
	} else {

	    /* if we're at end of scan, release the buffer and return */
	    if ((blkno = opaque->nobtpo_prev) == P_NONE) {
		_nobt_relbuf(rel, *bufP, NOBT_READ);
		*bufP = so->nobtso_curbuf = InvalidBuffer;
		ItemPointerSetInvalid(current);
		return (false);
	    } else {

		obknum = BufferGetBlockNumber(*bufP);

		/* walk right to the next page with data */
		_nobt_relbuf(rel, *bufP, NOBT_READ);
		for (;;) {
		    *bufP = _nobt_getbuf(rel, blkno, NOBT_READ);
		    page = BufferGetPage(*bufP, 0);
		    opaque = (NOBTPageOpaque) PageGetSpecialPointer(page);
		    maxoff = PageGetMaxOffsetIndex(page);

		    /*
		     *  If the adjacent page just split, then we may have the
		     *  wrong block.  Handle this case.  Because pages only
		     *  split right, we don't have to worry about this failing
		     *  to terminate.
		     */

		    while (opaque->nobtpo_next != obknum) {
			blkno = opaque->nobtpo_next;
			_nobt_relbuf(rel, *bufP, NOBT_READ);
			*bufP = _nobt_getbuf(rel, blkno, NOBT_READ);
			page = BufferGetPage(*bufP, 0);
			opaque = (NOBTPageOpaque) PageGetSpecialPointer(page);
			maxoff = PageGetMaxOffsetIndex(page);
		    }

		    /* don't consider the high key */
		    if (opaque->nobtpo_next == P_NONE)
			start = 0;
		    else
			start = 1;

		    /* anything to look at here? */
		    if (!PageIsEmpty(page) && maxoff >= start) {
			break;
		    } else {
			blkno = opaque->nobtpo_prev;
			obknum = BufferGetBlockNumber(*bufP);
			_nobt_relbuf(rel, *bufP, NOBT_READ);
			if (blkno == P_NONE) {
			    *bufP = so->nobtso_curbuf = InvalidBuffer;
			    ItemPointerSetInvalid(current);
			    return (false);
			}
		    }
		}
		offind = maxoff;
	    }
	}
    }
    blkno = BufferGetBlockNumber(*bufP);
    so->nobtso_curbuf = *bufP;
    ItemPointerSet(current, 0, blkno, 0, offind + 1);

    return (true);
}

/*
 *  _nobt_twostep() -- Move to an adjacent record in a scan on the tree,
 *		     if an adjacent record exists.
 *
 *	This is like _nobt_step, except that if no adjacent record exists
 *	it restores us to where we were before trying the step.  This is
 *	only hairy when you cross page boundaries, since the page you cross
 *	from could have records inserted or deleted, or could even split.
 *	This is unlikely, but we try to handle it correctly here anyway.
 *
 *	This routine contains the only case in which our changes to Lehman
 *	and Yao's algorithm.
 *
 *	Like step, this routine leaves the scan's currentItemData in the
 *	proper state and acquires a lock and pin on *bufP.  If the twostep
 *	succeeded, we return true; otherwise, we return false.
 */

bool
_nobt_twostep(scan, bufP, dir)
    IndexScanDesc scan;
    Buffer *bufP;
    ScanDirection dir;
{
    Page page;
    NOBTPageOpaque opaque;
    OffsetIndex offind, maxoff;
    OffsetIndex start;
    ItemPointer current;
    ItemId itemid;
    int itemsz;
    NOBTLItem btitem;
    NOBTLItem svitem;
    BlockNumber blkno;
    int nbytes;

    blkno = BufferGetBlockNumber(*bufP);
    page = BufferGetPage(*bufP, 0);
    opaque = (NOBTPageOpaque) PageGetSpecialPointer(page);
    maxoff = PageGetMaxOffsetIndex(page);
    current = &(scan->currentItemData);
    offind = ItemPointerGetOffsetNumber(current, 0) - 1;

    if (opaque->nobtpo_next != P_NONE)
	start = 1;
    else
	start = 0;

    /* if we're safe, just do it */
    if (ScanDirectionIsForward(dir) && offind < maxoff) {
	ItemPointerSet(current, 0, blkno, 0, offind + 2);
	return (true);
    } else if (ScanDirectionIsBackward(dir) && offind < start) {
	ItemPointerSet(current, 0, blkno, 0, offind);
	return (true);
    }

    /* if we've hit end of scan we don't have to do any work */
    if (ScanDirectionIsForward(dir) && opaque->nobtpo_next == P_NONE)
	return (false);
    else if (ScanDirectionIsBackward(dir) && opaque->nobtpo_prev == P_NONE)
	return (false);

    /*
     *  Okay, it's off the page; let _nobt_step() do the hard work, and we'll
     *  try to remember where we were.  This is not guaranteed to work; this
     *  is the only place in the code where concurrency can screw us up,
     *  and it's because we want to be able to move in two directions in
     *  the scan.
     */

    itemid = PageGetItemId(page, offind);
    itemsz = ItemIdGetLength(itemid);
    btitem = (NOBTLItem) PageGetItem(page, itemid);
    svitem = (NOBTLItem) palloc(itemsz);
    bcopy((char *) btitem, (char *) svitem, itemsz);

    if (_nobt_step(scan, bufP, dir)) {
	pfree(svitem);
	return (true);
    }

    /* try to find our place again */
    *bufP = _nobt_getbuf(scan->relation, blkno, NOBT_READ);
    page = BufferGetPage(*bufP, 0);
    maxoff = PageGetMaxOffsetIndex(page);
    nbytes = sizeof(NOBTLItemData) - sizeof(IndexTupleData);

    while (offind <= maxoff) {
	itemid = PageGetItemId(page, offind);
	btitem = (NOBTLItem) PageGetItem(page, itemid);
	if (bcmp((char *) btitem, (char *) svitem, nbytes) == 0) {
	    pfree (svitem);
	    ItemPointerSet(current, 0, blkno, 0, offind + 1);
	    return (false);
	}
    }

    /*
     *  XXX crash and burn -- can't find our place.  We can be a little
     *  smarter -- walk to the next page to the right, for example, since
     *  that's the only direction that splits happen in.  Deletions screw
     *  us up less often since they're only done by the vacuum daemon.
     */

    elog(WARN, "btree synchronization error:  concurrent update botched scan");

    /* NOTREACHED */
}

/*
 *  _nobt_endpoint() -- Find the first or last key in the index.
 */

RetrieveIndexResult
_nobt_endpoint(scan, dir)
    IndexScanDesc scan;
    ScanDirection dir;
{
    Relation rel;
    Buffer buf;
    Page page;
    NOBTPageOpaque opaque;
    ItemPointer current;
    ItemPointer iptr;
    OffsetIndex offind, maxoff;
    OffsetIndex start;
    BlockNumber blkno;
    NOBTLItem btitem;
    NOBTIItem btiitem;
    IndexTuple itup;
    NOBTScanOpaque so;
    RetrieveIndexResult res;

    rel = scan->relation;
    current = &(scan->currentItemData);

    buf = _nobt_getroot(rel, NOBT_READ);
    blkno = BufferGetBlockNumber(buf);
    page = BufferGetPage(buf, 0);
    opaque = (NOBTPageOpaque) PageGetSpecialPointer(page);

    for (;;) {
	if (opaque->nobtpo_flags & NOBTP_LEAF)
	    break;

	if (ScanDirectionIsForward(dir)) {
	    if (opaque->nobtpo_next == P_NONE)
		offind = 0;
	    else
		offind = 1;
	} else
	    offind = PageGetMaxOffsetIndex(page);

	btiitem = (NOBTIItem) PageGetItem(page, PageGetItemId(page, offind));
	blkno = btiitem->nobtii_child;

	_nobt_relbuf(rel, buf, NOBT_READ);
	buf = _nobt_getbuf(rel, blkno, NOBT_READ);
	page = BufferGetPage(buf, 0);
	opaque = (NOBTPageOpaque) PageGetSpecialPointer(page);

	/*
	 *  Race condition:  If the child page we just stepped onto is in
	 *  the process of being split, we need to make sure we're all the
	 *  way at the edge of the tree.  See the paper by Lehman and Yao.
	 */

	if (ScanDirectionIsBackward(dir) && opaque->nobtpo_next != P_NONE) {
	    do {
		blkno = opaque->nobtpo_next;
		_nobt_relbuf(rel, buf, NOBT_READ);
		buf = _nobt_getbuf(rel, blkno, NOBT_READ);
		page = BufferGetPage(buf, 0);
		opaque = (NOBTPageOpaque) PageGetSpecialPointer(page);
	    } while (opaque->nobtpo_next != P_NONE);
	}
    }

    /* okay, we've got the {left,right}-most page in the tree */
    maxoff = PageGetMaxOffsetIndex(page);

    if (ScanDirectionIsForward(dir)) {
	maxoff = PageGetMaxOffsetIndex(page);
	if (opaque->nobtpo_next == P_NONE)
	    start = 0;
	else
	    start = 1;

	if (PageIsEmpty(page) || start > maxoff) {
	    ItemPointerSet(current, 0, blkno, 0, maxoff + 1);
	    if (!_nobt_step(scan, &buf, BackwardScanDirection))
		return ((RetrieveIndexResult) NULL);

	    start = ItemPointerGetOffsetNumber(current, 0);
	    page = BufferGetPage(buf);
	} else {
	    ItemPointerSet(current, 0, blkno, 0, start + 1);
	}
    } else if (ScanDirectionIsBackward(dir)) {
	start = PageGetMaxOffsetIndex(page);
	if (PageIsEmpty(page)) {
	    ItemPointerSet(current, 0, blkno, 0, start + 1);
	    if (!_nobt_step(scan, &buf, ForwardScanDirection))
		return ((RetrieveIndexResult) NULL);

	    start = ItemPointerGetOffsetNumber(current, 0);
	    page = BufferGetPage(buf);
	} else {
	    ItemPointerSet(current, 0, blkno, 0, start + 1);
	}
    } else {
	elog(WARN, "Illegal scan direction %d", dir);
    }

    btitem = (NOBTLItem) PageGetItem(page, PageGetItemId(page, start));
    itup = &(btitem->nobtli_itup);

    /* see if we picked a winner */
    if (_nobt_checkqual(scan, itup)) {
	iptr = (ItemPointer) palloc(sizeof(ItemPointerData));
	bcopy((char *) &(itup->t_tid), (char *) iptr, sizeof(ItemPointerData));
	res = ItemPointerFormRetrieveIndexResult(current, iptr);

	/* remember which buffer we have pinned */
	so = (NOBTScanOpaque) scan->opaque;
	so->nobtso_curbuf = buf;
    } else {
	_nobt_relbuf(rel, buf, NOBT_READ);
	res = (RetrieveIndexResult) NULL;
    }

    return (res);
}

#endif /* NOBTREE */
