/*
 *  btsearch.c -- search code for postgres btrees.
 */

#include "tmp/c.h"
#include "tmp/postgres.h"

#include "storage/bufmgr.h"
#include "storage/bufpage.h"
#include "storage/page.h"

#include "fmgr.h"
#include "utils/log.h"
#include "utils/rel.h"
#include "utils/excid.h"

#include "access/heapam.h"
#include "access/genam.h"
#include "access/ftup.h"
#include "access/skey.h"
#include "access/sdir.h"
#include "access/nbtree.h"

RcsId("$Header$");

/*
 *  _bt_search() -- Search for a scan key in the index.
 *
 *	This routine is actually just a helper that sets things up and
 *	calls a recursive-descent search routine on the tree.
 */

BTStack
_bt_search(rel, keysz, scankey, bufP)
    Relation rel;
    int keysz;
    ScanKey scankey;
    Buffer *bufP;
{
    *bufP = _bt_getroot(rel, BT_READ);
    return (_bt_searchr(rel, keysz, scankey, bufP, (BTStack) NULL));
}

/*
 *  _bt_searchr() -- Search the tree recursively for a particular scankey.
 */

BTStack
_bt_searchr(rel, keysz, scankey, bufP, stack_in)
    Relation rel;
    int keysz;
    ScanKey scankey;
    Buffer *bufP;
    BTStack stack_in;
{
    BTStack stack;
    OffsetIndex offind;
    Page page;
    BTPageOpaque opaque;
    BlockNumber par_blkno;
    BlockNumber blkno;
    ItemId itemid;
    BTItem btitem;
    BTItem item_save;
    int item_nbytes;
    IndexTuple itup;
    IndexTuple itup_save;

    /* if this is a leaf page, we're done */
    page = BufferGetPage(*bufP, 0);
    opaque = (BTPageOpaque) PageGetSpecialPointer(page);
    if (opaque->btpo_flags & BTP_LEAF)
	return (stack_in);

    /*
     *  Find the appropriate item on the internal page, and get the child
     *  page that it points to.
     */

    par_blkno = BufferGetBlockNumber(*bufP);
    offind = _bt_binsrch(rel, *bufP, keysz, scankey, BT_DESCENT);
    itemid = PageGetItemId(page, offind);
    btitem = (BTItem) PageGetItem(page, itemid);
    itup = &(btitem->bti_itup);
    blkno = ItemPointerGetBlockNumber(&(itup->t_tid));

    /*
     *  We need to save the bit image of the index entry we chose in the
     *  parent page on a stack.  In case we split the tree, we'll use this
     *  bit image to figure out what our real parent page is, in case the
     *  parent splits while we're working lower in the tree.  See the paper
     *  by Lehman and Yao for how this is detected and handled.  (We use
     *  unique OIDs to disambiguate duplicate keys in the index -- Lehman
     *  and Yao disallow duplicate keys).
     */

    item_nbytes = ItemIdGetLength(itemid);
    item_save = (BTItem) palloc(item_nbytes);
    bcopy((char *) btitem, (char *) item_save, item_nbytes);
    stack = (BTStack) palloc(sizeof(BTStackData));
    stack->bts_blkno = par_blkno;
    stack->bts_offset = offind;
    stack->bts_btitem = item_save;
    stack->bts_parent = stack_in;

    /* drop the read lock on the parent page and acquire one on the child */
    _bt_relbuf(rel, *bufP, BT_READ);
    *bufP = _bt_getbuf(rel, blkno, BT_READ);

    /*
     *  Race -- the page we just grabbed may have split since we read its
     *  pointer in the parent.  If it has, we may need to move right to its
     *  new sibling.  Do that.
     */

    *bufP = _bt_moveright(rel, *bufP, keysz, scankey, BT_READ);

    /* okay, all set to move down a level */
    return (_bt_searchr(rel, keysz, scankey, bufP, stack));
}

/*
 *  _bt_moveright() -- move right in the btree if necessary.
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
_bt_moveright(rel, buf, keysz, scankey, access)
    Relation rel;
    Buffer buf;
    int keysz;
    ScanKey scankey;
    int access;
{
    Page page;
    PageNumber right;
    BTPageOpaque opaque;
    ItemId hikey;
    ItemId itemid;
    BlockNumber rblkno;

    page = BufferGetPage(buf, 0);
    opaque = (BTPageOpaque) PageGetSpecialPointer(page);

    /* if we're on a rightmost page, we don't need to move right */
    if (opaque->btpo_next == P_NONE)
	return (buf);

    /* by convention, item 0 on non-rightmost pages is the high key */
    hikey = PageGetItemId(page, 0);

    /*
     *  If the scan key that brought us to this page is >= the high key
     *  stored on the page, then the page has split and we need to move
     *  right.
     */

    if (_bt_skeycmp(rel, keysz, scankey, page, hikey,
		    BTGreaterEqualStrategyNumber)) {

	/* move right as long as we need to */
	do {
	    /*
	     *  If this page consists of all duplicate keys (hikey and first
	     *  key on the page have the same value), then we don't need to
	     *  step right.
	     */
	    if (PageGetMaxOffsetIndex(page) > 0) {
		itemid = PageGetItemId(page, 1);
		if (_bt_skeycmp(rel, keysz, scankey, page, itemid,
				BTEqualStrategyNumber)) {
		    /* break is for the "move right" while loop */
		    break;
		}
	    }

	    /* step right one page */
	    rblkno = opaque->btpo_next;
	    _bt_relbuf(rel, buf, access);
	    buf = _bt_getbuf(rel, rblkno, access);
	    page = BufferGetPage(buf, 0);
	    opaque = (BTPageOpaque) PageGetSpecialPointer(page);
	    hikey = PageGetItemId(page, 0);

	} while (opaque->btpo_next != P_NONE
		 && _bt_skeycmp(rel, keysz, scankey, page, hikey,
				 BTGreaterEqualStrategyNumber));
    }
    return (buf);
}

/*
 *  _bt_skeycmp() -- compare a scan key to a particular item on a page using
 *		     a requested strategy (<, <=, =, >=, >).
 *
 *	We ignore the unique OIDs stored in the btree item here.  Those
 *	numbers are intended for use internally only, in repositioning a
 *	scan after a page split.  They do not impose any meaningful ordering.
 *
 *	The comparison is A <op> B, where A is the scan key and B is the
 *	tuple pointed at by itemid on page.
 */

bool
_bt_skeycmp(rel, keysz, scankey, page, itemid, strat)
    Relation rel;
    Size keysz;
    ScanKey scankey;
    Page page;
    ItemId itemid;
    StrategyNumber strat;
{
    BTItem item;
    IndexTuple indexTuple;
    TupleDescriptor tupDes;
    ScanKeyEntry entry;
    int i;
    Datum attrDatum;
    Datum keyDatum;
    bool compare;
    bool isNull;

    item = (BTItem) PageGetItem(page, itemid);
    indexTuple = &(item->bti_itup);

    tupDes = RelationGetTupleDescriptor(rel);

    /* see if the comparison is true for all of the key attributes */
    for (i=1; i <= keysz; i++) {

	entry = &scankey->data[i-1];
	attrDatum = IndexTupleGetAttributeValue(indexTuple,
						entry->attributeNumber,
						tupDes, &isNull);
	keyDatum  = entry->argument;

	compare = _bt_invokestrat(rel, i, strat, keyDatum, attrDatum);
	if (!compare)
	    return (false);
    }

    return (true);
}

/*
 *  _bt_binsrch() -- Do a binary search for a key on a particular page.
 *
 *	The scankey we get has the compare function stored in the procedure
 *	entry of each data struct.  We invoke this regproc to do the
 *	comparison for every key in the scankey.  _bt_binsrch() returns
 *	the OffsetIndex of the first matching key on the page, or the
 *	OffsetIndex at which the matching key would appear if it were
 *	on this page.
 *
 *	By the time this procedure is called, we're sure we're looking
 *	at the right page -- don't need to walk right.  _bt_binsrch() has
 *	no lock or refcount side effects on the buffer.
 */

OffsetIndex
_bt_binsrch(rel, buf, keysz, scankey, srchtype)
    Relation rel;
    Buffer buf;
    int keysz;
    ScanKey scankey;
    int srchtype;
{
    TupleDescriptor itupdesc;
    Page page;
    BTPageOpaque opaque;
    OffsetIndex low, mid, high;
    bool match;
    int result;

    page = BufferGetPage(buf, 0);
    opaque = (BTPageOpaque) PageGetSpecialPointer(page);

    /* by convention, item 0 on any non-leftmost page is the high key */
    if (opaque->btpo_next != P_NONE)
	low = 1;
    else
	low = 0;

    high = PageGetMaxOffsetIndex(page);

    /*
     *  Since for non-leftmost pages, the zeroeth item on the page is the
     *  high key, there are two notions of emptiness.  One is if nothing
     *  appears on the page.  The other is if nothing but the high key does.
     *  The reason we test high <= low, rather than high == low, is that
     *  after vacuuming there may be nothing *but* the high key on a page.
     *  In that case, given the scheme above, low = 1 and high = 0.
     */

    if (PageIsEmpty(page) || (opaque->btpo_next != P_NONE && high <= low))
	return (low);

    itupdesc = RelationGetTupleDescriptor(rel);
    match = false;

    while ((high - low) > 1) {
	mid = low + ((high - low) / 2);
	result = _bt_compare(rel, itupdesc, page, keysz, scankey, mid);

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
	return (_bt_firsteq(rel, itupdesc, page, keysz, scankey, mid));
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

	opaque = (BTPageOpaque) PageGetSpecialPointer(page);
	if (!(opaque->btpo_flags & BTP_LEAF) && srchtype == BT_DESCENT) {

	    /*
	     *  We want the last key <, or first key ==, the scan key.
	     */

	    result = _bt_compare(rel, itupdesc, page, keysz, scankey, high);

	    if (result == 0) {
		return (_bt_firsteq(rel, itupdesc, page, keysz, scankey, high));
	    } else if (result > 0) {
		return (high);
	    } else {
		return (low);
	    }
	    /* NOTREACHED */
	} else {

	    /* we want the first key >= the scan key */
	    result = _bt_compare(rel, itupdesc, page, keysz, scankey, low);
	    if (result <= 0) {
		return (low);
	    } else {
		if (low == high)
		    return (low + 1);

		result = _bt_compare(rel, itupdesc, page, keysz, scankey, high);
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
_bt_firsteq(rel, itupdesc, page, keysz, scankey, offind)
    Relation rel;
    TupleDescriptor itupdesc;
    Page page;
    Size keysz;
    ScanKey scankey;
    OffsetIndex offind;
{
    BTPageOpaque opaque;
    OffsetIndex limit;

    opaque = (BTPageOpaque) PageGetSpecialPointer(page);

    /* skip the high key, if any */
    if (opaque->btpo_next != P_NONE)
	limit = 1;
    else
	limit = 0;

    /* walk backwards looking for the first key in the chain of duplicates */
    while (offind > limit
	   && _bt_compare(rel, itupdesc, page,
			  keysz, scankey, offind - 1) == 0) {
	offind--;
    }

    return (offind);
}

/*
 *  _bt_compare() -- Compare scankey to a particular tuple on the page.
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
_bt_compare(rel, itupdesc, page, keysz, scankey, offind)
    Relation rel;
    TupleDescriptor itupdesc;
    Page page;
    int keysz;
    ScanKey scankey;
    OffsetIndex offind;
{
    Datum datum;
    BTItem btitem;
    ItemId itemid;
    IndexTuple itup;
    BTPageOpaque opaque;
    ScanKeyEntry entry;
    AttributeNumber attno;
    int result;
    int i;
    Boolean null;

    /*
     *  If this is a leftmost internal page, and if our comparison is
     *  with the first key on the page, then the item at that position is
     *  by definition less than the scan key.
     */

    opaque = (BTPageOpaque) PageGetSpecialPointer(page);
    if (!(opaque->btpo_flags & BTP_LEAF)
	&& opaque->btpo_prev == P_NONE
	&& offind == 0) {
	    itemid = PageGetItemId(page, offind);

	    /*
	     *  If the item on the page is equal to the scankey, that's
	     *  okay to admit.  We just can't claim that the first key on
	     *  the page is greater than anything.
	     */

	    if (_bt_skeycmp(rel, keysz, scankey, page, itemid,
			    BTEqualStrategyNumber)) {
		return (0);
	    }
	    return (1);
    }

    btitem = (BTItem) PageGetItem(page, PageGetItemId(page, offind));
    itup = &(btitem->bti_itup);

    /*
     *  The scan key is set up with the attribute number associated with each
     *  term in the key.  It is important that, if the index is multi-key,
     *  the scan contain the first k key attributes, and that they be in
     *  order.  If you think about how multi-key ordering works, you'll
     *  understand why this is.
     *
     *  We don't test for violation of this condition here.
     */

    for (i = 1; i <= keysz; i++) {
	entry = &(scankey->data[i - 1]);
	attno = entry->attributeNumber;
	datum = (Datum) IndexTupleGetAttributeValue(itup, attno,
						    itupdesc, &null);
	result = (int) FMGR_PTR2(entry->func, entry->procedure,
				 entry->argument, datum);

	/* if the keys are unequal, return the difference */
	if (result != 0)
	    return (result);
    }

    /* by here, the keys are equal */
    return (0);
}

/*
 *  _bt_next() -- Get the next item in a scan.
 *
 *	On entry, we have a valid currentItemData in the scan, and a
 *	read lock on the page that contains that item.  We do not have
 *	the page pinned.  We return the next item in the scan.  On
 *	exit, we have the page containing the next item locked but not
 *	pinned.
 */

RetrieveIndexResult
_bt_next(scan, dir)
    IndexScanDesc scan;
    ScanDirection dir;
{
    Relation rel;
    BTPageOpaque opaque;
    Buffer buf;
    Page page;
    OffsetIndex offind, maxoff;
    RetrieveIndexResult res;
    BlockNumber blkno;
    ItemPointer current;
    ItemPointer iptr;
    BTItem btitem;
    IndexTuple itup;
    BTScanOpaque so;

    rel = scan->relation;
    so = (BTScanOpaque) scan->opaque;
    current = &(scan->currentItemData);

    /*
     *  XXX 10 may 91:  somewhere there's a bug in our management of the
     *  cached buffer for this scan.  wei discovered it.  the following
     *  is a workaround so he can work until i figure out what's going on.
     */

    if (!BufferIsValid(so->btso_curbuf))
	so->btso_curbuf = _bt_getbuf(rel, ItemPointerGetBlockNumber(current),
				     BT_READ);

    /* we still have the buffer pinned and locked */
    buf = so->btso_curbuf;
    blkno = BufferGetBlockNumber(buf);

    /* step one tuple in the appropriate direction */
    if (!_bt_step(scan, &buf, dir))
	return ((RetrieveIndexResult) NULL);

    /* by here, current is the tuple we want to return */
    offind = ItemPointerGetOffsetNumber(current, 0) - 1;
    page = BufferGetPage(buf, 0);
    btitem = (BTItem) PageGetItem(page, PageGetItemId(page, offind));
    itup = &btitem->bti_itup;

    if (_bt_checkqual(scan, itup)) {
	iptr = (ItemPointer) palloc(sizeof(ItemPointerData));
	bcopy((char *) &(itup->t_tid), (char *) iptr, sizeof(ItemPointerData));
	res = ItemPointerFormRetrieveIndexResult(current, iptr);

	/* remember which buffer we have pinned and locked */
	so->btso_curbuf = buf;
    } else {
	ItemPointerSetInvalid(current);
	so->btso_curbuf = InvalidBuffer;
	_bt_relbuf(rel, buf, BT_READ);
	res = (RetrieveIndexResult) NULL;
    }

    return (res);
}

/*
 *  _bt_first() -- Find the first item in a scan.
 *
 *	We need to be clever about the type of scan, the operation it's
 *	performing, and the tree ordering.  We return the RetrieveIndexResult
 *	of the first item in the tree that satisfies the qualification
 *	associated with the scan descriptor.  On exit, the page containing
 *	the current index tuple is read locked and pinned, and the scan's
 *	opaque data entry is updated to include the buffer.
 */

RetrieveIndexResult
_bt_first(scan, dir)
    IndexScanDesc scan;
    ScanDirection dir;
{
    Relation rel;
    TupleDescriptor itupdesc;
    Buffer buf;
    Page page;
    BTStack stack;
    OffsetIndex offind, maxoff;
    BTItem btitem;
    IndexTuple itup;
    ItemPointer current;
    ItemPointer iptr;
    BlockNumber blkno;
    StrategyNumber strat;
    RetrieveIndexResult res;
    RegProcedure proc;
    int result;
    BTScanOpaque so;
    ScanKeyData skdata;

    /* if we just need to walk down one edge of the tree, do that */
    if (scan->scanFromEnd)
	return (_bt_endpoint(scan, dir));

    rel = scan->relation;
    itupdesc = RelationGetTupleDescriptor(scan->relation);
    current = &(scan->currentItemData);
    so = (BTScanOpaque) scan->opaque;

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
    proc = index_getprocid(rel, 1, BTORDER_PROC);
    ScanKeyEntryInitialize((ScanKeyEntry) &skdata, 0x0, 1, proc,
			   scan->keyData.data[0].argument);

    stack = _bt_search(rel, 1, &skdata, &buf);
    _bt_freestack(stack);

    /* find the nearest match to the manufactured scan key on the page */
    offind = _bt_binsrch(rel, buf, 1, &skdata, BT_DESCENT);
    page = BufferGetPage(buf, 0);
    maxoff = PageGetMaxOffsetIndex(page);

    if (offind > maxoff)
	offind = maxoff;

    blkno = BufferGetBlockNumber(buf);
    ItemPointerSet(current, 0, blkno, 0, offind + 1);

    /*
     *  Now find the right place to start the scan.  Result is the
     *  value we're looking for minus the value we're looking at
     *  in the index.
     */

    result = _bt_compare(rel, itupdesc, page, 1, &skdata, offind);
    strat = _bt_getstrat(rel, 1, scan->keyData.data[0].procedure);

    switch (strat) {
      case BTLessStrategyNumber:
	if (result <= 0) {
	    do {
		if (!_bt_twostep(scan, &buf, BackwardScanDirection))
		    break;

		offind = ItemPointerGetOffsetNumber(current, 0) - 1;
		page = BufferGetPage(buf, 0);
		result = _bt_compare(rel, itupdesc, page, 1, &skdata, offind);
	    } while (result <= 0);

	    /* if this is true, the key we just looked at is gone */
	    if (result > 0)
		(void) _bt_twostep(scan, &buf, ForwardScanDirection);
	}
	break;

      case BTLessEqualStrategyNumber:
	if (result >= 0) {
	    do {
		if (!_bt_twostep(scan, &buf, ForwardScanDirection))
		    break;

		offind = ItemPointerGetOffsetNumber(current, 0) - 1;
		page = BufferGetPage(buf, 0);
		result = _bt_compare(rel, itupdesc, page, 1, &skdata, offind);
	    } while (result >= 0);

	    if (result < 0)
		(void) _bt_twostep(scan, &buf, BackwardScanDirection);
	}
	break;

      case BTEqualStrategyNumber:
	if (result != 0) {
	  _bt_relbuf(scan->relation, buf, BT_READ);
	  so->btso_curbuf = InvalidBuffer;
	  ItemPointerSetInvalid(&(scan->currentItemData));
	  return ((RetrieveIndexResult) NULL);
	}
	break;

      case BTGreaterEqualStrategyNumber:
	if (result < 0) {
	    do {
		if (!_bt_twostep(scan, &buf, BackwardScanDirection))
		    break;

		offind = ItemPointerGetOffsetNumber(current, 0) - 1;
		page = BufferGetPage(buf, 0);
		result = _bt_compare(rel, itupdesc, page, 1, &skdata, offind);
	    } while (result < 0);

	    if (result > 0)
		(void) _bt_twostep(scan, &buf, ForwardScanDirection);
	}
	break;

      case BTGreaterStrategyNumber:
	if (result >= 0) {
	    do {
		if (!_bt_twostep(scan, &buf, ForwardScanDirection))
		    break;

		offind = ItemPointerGetOffsetNumber(current, 0) - 1;
		page = BufferGetPage(buf, 0);
		result = _bt_compare(rel, itupdesc, page, 1, &skdata, offind);
	    } while (result >= 0);
	}
	break;
    }

    /* okay, current item pointer for the scan is right */
    offind = ItemPointerGetOffsetNumber(current, 0) - 1;
    page = BufferGetPage(buf, 0);
    btitem = (BTItem) PageGetItem(page, PageGetItemId(page, offind));
    itup = &btitem->bti_itup;

    if (_bt_checkqual(scan, itup)) {
	iptr = (ItemPointer) palloc(sizeof(ItemPointerData));
	bcopy((char *) &(itup->t_tid), (char *) iptr, sizeof(ItemPointerData));
	res = ItemPointerFormRetrieveIndexResult(current, iptr);

	/* remember which buffer we have pinned */
	so->btso_curbuf = buf;
    } else {
	ItemPointerSetInvalid(current);
	so->btso_curbuf = InvalidBuffer;
	_bt_relbuf(rel, buf, BT_READ);
	res = (RetrieveIndexResult) NULL;
    }

    return (res);
}

/*
 *  _bt_step() -- Step one item in the requested direction in a scan on
 *		  the tree.
 *
 *	If no adjacent record exists in the requested direction, return
 *	false.  Else, return true and set the currentItemData for the
 *	scan to the right thing.
 */

bool
_bt_step(scan, bufP, dir)
    IndexScanDesc scan;
    Buffer *bufP;
    ScanDirection dir;
{
    Page page;
    BTPageOpaque opaque;
    OffsetIndex offind, maxoff;
    OffsetIndex start;
    BlockNumber blkno;
    BlockNumber obknum;
    BTScanOpaque so;
    ItemPointer current;
    Relation rel;

    rel = scan->relation;
    current = &(scan->currentItemData);
    offind = ItemPointerGetOffsetNumber(current, 0) - 1;
    page = BufferGetPage(*bufP, 0);
    opaque = (BTPageOpaque) PageGetSpecialPointer(page);
    so = (BTScanOpaque) scan->opaque;
    maxoff = PageGetMaxOffsetIndex(page);

    /* get the next tuple */
    if (ScanDirectionIsForward(dir)) {
	if (offind < maxoff) {
	    offind++;
	} else {

	    /* if we're at end of scan, release the buffer and return */
	    if ((blkno = opaque->btpo_next) == P_NONE) {
		_bt_relbuf(rel, *bufP, BT_READ);
		ItemPointerSetInvalid(current);
		*bufP = so->btso_curbuf = InvalidBuffer;
		return (false);
	    } else {

		/* walk right to the next page with data */
		_bt_relbuf(rel, *bufP, BT_READ);
		for (;;) {
		    *bufP = _bt_getbuf(rel, blkno, BT_READ);
		    page = BufferGetPage(*bufP, 0);
		    opaque = (BTPageOpaque) PageGetSpecialPointer(page);
		    maxoff = PageGetMaxOffsetIndex(page);
		    if (opaque->btpo_next != P_NONE)
			start = 1;
		    else
			start = 0;

		    if (!PageIsEmpty(page) && start <= maxoff) {
			break;
		    } else {
			blkno = opaque->btpo_next;
			_bt_relbuf(rel, *bufP, BT_READ);
			if (blkno == P_NONE) {
			    *bufP = so->btso_curbuf = InvalidBuffer;
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
	if (opaque->btpo_next == P_NONE)
	    start = 0;
	else
	    start = 1;

	if (offind > start) {
	    offind--;
	} else {

	    /* if we're at end of scan, release the buffer and return */
	    if ((blkno = opaque->btpo_prev) == P_NONE) {
		_bt_relbuf(rel, *bufP, BT_READ);
		*bufP = so->btso_curbuf = InvalidBuffer;
		ItemPointerSetInvalid(current);
		return (false);
	    } else {

		obknum = BufferGetBlockNumber(*bufP);

		/* walk right to the next page with data */
		_bt_relbuf(rel, *bufP, BT_READ);
		for (;;) {
		    *bufP = _bt_getbuf(rel, blkno, BT_READ);
		    page = BufferGetPage(*bufP, 0);
		    opaque = (BTPageOpaque) PageGetSpecialPointer(page);
		    maxoff = PageGetMaxOffsetIndex(page);

		    /*
		     *  If the adjacent page just split, then we may have the
		     *  wrong block.  Handle this case.  Because pages only
		     *  split right, we don't have to worry about this failing
		     *  to terminate.
		     */

		    while (opaque->btpo_next != obknum) {
			blkno = opaque->btpo_next;
			_bt_relbuf(rel, *bufP, BT_READ);
			*bufP = _bt_getbuf(rel, blkno, BT_READ);
			page = BufferGetPage(*bufP, 0);
			opaque = (BTPageOpaque) PageGetSpecialPointer(page);
			maxoff = PageGetMaxOffsetIndex(page);
		    }

		    /* don't consider the high key */
		    if (opaque->btpo_next == P_NONE)
			start = 0;
		    else
			start = 1;

		    /* anything to look at here? */
		    if (!PageIsEmpty(page) && maxoff >= start) {
			break;
		    } else {
			blkno = opaque->btpo_prev;
			obknum = BufferGetBlockNumber(*bufP);
			_bt_relbuf(rel, *bufP, BT_READ);
			if (blkno == P_NONE) {
			    *bufP = so->btso_curbuf = InvalidBuffer;
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
    so->btso_curbuf = *bufP;
    ItemPointerSet(current, 0, blkno, 0, offind + 1);

    return (true);
}

/*
 *  _bt_twostep() -- Move to an adjacent record in a scan on the tree,
 *		     if an adjacent record exists.
 *
 *	This is like _bt_step, except that if no adjacent record exists
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
_bt_twostep(scan, bufP, dir)
    IndexScanDesc scan;
    Buffer *bufP;
    ScanDirection dir;
{
    Page page;
    BTPageOpaque opaque;
    OffsetIndex offind, maxoff;
    OffsetIndex start;
    ItemPointer current;
    ItemId itemid;
    int itemsz;
    BTItem btitem;
    BTItem svitem;
    BlockNumber blkno;

    blkno = BufferGetBlockNumber(*bufP);
    page = BufferGetPage(*bufP, 0);
    opaque = (BTPageOpaque) PageGetSpecialPointer(page);
    maxoff = PageGetMaxOffsetIndex(page);
    current = &(scan->currentItemData);
    offind = ItemPointerGetOffsetNumber(current, 0) - 1;

    if (opaque->btpo_next != P_NONE)
	start = 1;
    else
	start = 0;

    /* if we're safe, just do it */
    if (ScanDirectionIsForward(dir) && offind < maxoff) {
	ItemPointerSet(current, 0, blkno, 0, offind + 2);
	return (true);
    } else if (ScanDirectionIsBackward(dir) && offind >= start) {
	ItemPointerSet(current, 0, blkno, 0, offind);
	return (true);
    }

    /* if we've hit end of scan we don't have to do any work */
    if (ScanDirectionIsForward(dir) && opaque->btpo_next == P_NONE)
	return (false);
    else if (ScanDirectionIsBackward(dir) && opaque->btpo_prev == P_NONE)
	return (false);

    /*
     *  Okay, it's off the page; let _bt_step() do the hard work, and we'll
     *  try to remember where we were.  This is not guaranteed to work; this
     *  is the only place in the code where concurrency can screw us up,
     *  and it's because we want to be able to move in two directions in
     *  the scan.
     */

    itemid = PageGetItemId(page, offind);
    itemsz = ItemIdGetLength(itemid);
    btitem = (BTItem) PageGetItem(page, itemid);
    svitem = (BTItem) palloc(itemsz);
    bcopy((char *) btitem, (char *) svitem, itemsz);

    if (_bt_step(scan, bufP, dir)) {
	pfree(svitem);
	return (true);
    }

    /* try to find our place again */
    *bufP = _bt_getbuf(scan->relation, blkno, BT_READ);
    page = BufferGetPage(*bufP, 0);
    maxoff = PageGetMaxOffsetIndex(page);

    while (offind <= maxoff) {
	itemid = PageGetItemId(page, offind);
	btitem = (BTItem) PageGetItem(page, itemid);
	if (btitem->bti_oid == svitem->bti_oid) {
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
 *  _bt_endpoint() -- Find the first or last key in the index.
 */

RetrieveIndexResult
_bt_endpoint(scan, dir)
    IndexScanDesc scan;
    ScanDirection dir;
{
    Relation rel;
    Buffer buf;
    Page page;
    BTPageOpaque opaque;
    ItemPointer current;
    ItemPointer iptr;
    OffsetIndex offind, maxoff;
    OffsetIndex start;
    BlockNumber blkno;
    BTItem btitem;
    IndexTuple itup;
    BTScanOpaque so;
    RetrieveIndexResult res;

    rel = scan->relation;
    current = &(scan->currentItemData);

    buf = _bt_getroot(rel, BT_READ);
    blkno = BufferGetBlockNumber(buf);
    page = BufferGetPage(buf, 0);
    opaque = (BTPageOpaque) PageGetSpecialPointer(page);

    for (;;) {
	if (opaque->btpo_flags & BTP_LEAF)
	    break;

	if (ScanDirectionIsForward(dir)) {
	    if (opaque->btpo_next == P_NONE)
		offind = 0;
	    else
		offind = 1;
	} else
	    offind = PageGetMaxOffsetIndex(page);

	btitem = (BTItem) PageGetItem(page, PageGetItemId(page, offind));
	itup = &(btitem->bti_itup);

	blkno = ItemPointerGetBlockNumber(&(itup->t_tid));

	_bt_relbuf(rel, buf, BT_READ);
	buf = _bt_getbuf(rel, blkno, BT_READ);
	page = BufferGetPage(buf, 0);
	opaque = (BTPageOpaque) PageGetSpecialPointer(page);

	/*
	 *  Race condition:  If the child page we just stepped onto is in
	 *  the process of being split, we need to make sure we're all the
	 *  way at the edge of the tree.  See the paper by Lehman and Yao.
	 */

	if (ScanDirectionIsBackward(dir) && opaque->btpo_next != P_NONE) {
	    do {
		blkno = opaque->btpo_next;
		_bt_relbuf(rel, buf, BT_READ);
		buf = _bt_getbuf(rel, blkno, BT_READ);
		page = BufferGetPage(buf, 0);
		opaque = (BTPageOpaque) PageGetSpecialPointer(page);
	    } while (opaque->btpo_next != P_NONE);
	}
    }

    /* okay, we've got the {left,right}-most page in the tree */
    maxoff = PageGetMaxOffsetIndex(page);

    if (ScanDirectionIsForward(dir)) {
	if (PageIsEmpty(page))
	    maxoff = 0;
	else
	    maxoff = PageGetMaxOffsetIndex(page);
	if (opaque->btpo_next == P_NONE)
	    start = 0;
	else
	    start = 1;

	if (PageIsEmpty(page) || start > maxoff) {
	    ItemPointerSet(current, 0, blkno, 0, maxoff + 1);
	    if (!_bt_step(scan, &buf, BackwardScanDirection))
		return ((RetrieveIndexResult) NULL);

	    start = ItemPointerGetOffsetNumber(current, 0);
	    page = BufferGetPage(buf,0);
	} else {
	    ItemPointerSet(current, 0, blkno, 0, start + 1);
	}
    } else if (ScanDirectionIsBackward(dir)) {
	if (PageIsEmpty(page)) {
	    ItemPointerSet(current, 0, blkno, 0, 1);
	    if (!_bt_step(scan, &buf, ForwardScanDirection))
		return ((RetrieveIndexResult) NULL);

	    start = ItemPointerGetOffsetNumber(current, 0);
	    page = BufferGetPage(buf,0);
	} else {
	    start = PageGetMaxOffsetIndex(page);
	    ItemPointerSet(current, 0, blkno, 0, start + 1);
	}
    } else {
	elog(WARN, "Illegal scan direction %d", dir);
    }

    btitem = (BTItem) PageGetItem(page, PageGetItemId(page, start));
    itup = &(btitem->bti_itup);

    /* see if we picked a winner */
    if (_bt_checkqual(scan, itup)) {
	iptr = (ItemPointer) palloc(sizeof(ItemPointerData));
	bcopy((char *) &(itup->t_tid), (char *) iptr, sizeof(ItemPointerData));
	res = ItemPointerFormRetrieveIndexResult(current, iptr);

	/* remember which buffer we have pinned */
	so = (BTScanOpaque) scan->opaque;
	so->btso_curbuf = buf;
    } else {
	_bt_relbuf(rel, buf, BT_READ);
	res = (RetrieveIndexResult) NULL;
    }

    return (res);
}
