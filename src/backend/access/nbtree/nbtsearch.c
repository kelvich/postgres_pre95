/*
 *  btsearch.c -- search code for postgres btrees.
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
#include "access/skey.h"
#include "access/sdir.h"

#include "/n/eden/users/mao/postgres/src/access/nbtree/nbtree.h"

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
    btitem = (BTItem) PageGetItem(page, PageGetItemId(page, offind));
    itup = &(btitem->bti_itup);
    blkno = ItemPointerGetBlockNumber(&(itup->t_tid), 0);

    /*
     *  We need to save the bit image of the index entry we chose in the
     *  parent page on a stack.  In case we split the tree, we'll use this
     *  bit image to figure out what our real parent page is, in case the
     *  parent splits while we're working lower in the tree.  See the paper
     *  by Lehman and Yao for how this is detected and handled.  (We use
     *  sequence numbers to disambiguate duplicate keys in the index --
     *  Lehman and Yao disallow duplicate keys).
     */

    item_nbytes = itup->t_size
		     + (sizeof(BTItemData) - sizeof(IndexTupleData));
    item_save = (BTItem) palloc(item_nbytes);
    bcopy((char *) itup, (char *) item_save, item_nbytes);
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
    BlockNumber rblkno;

    page = BufferGetPage(buf, 0);
    opaque = (BTPageOpaque) PageGetSpecialPointer(page);

    /* if we're on a rightmost page, we don't need to move right */
    if (opaque->btpo_next == P_NONE)
	return (buf);

    /* by convention, item 0 on non-rightmost pages is the high key */
    hikey = PageGetItemId(page, 0);

    /* figure out if we need to move right */
    if (_bt_skeyless(rel, keysz, scankey, page, hikey)) {

	/* move right as long as we need to */
	do {
	    /* step right one page */
	    rblkno = opaque->btpo_next;
	    _bt_relbuf(rel, buf, access);
	    buf = _bt_getbuf(rel, rblkno, access);
	    page = BufferGetPage(buf, 0);
	    opaque = (BTPageOpaque) PageGetSpecialPointer(page);
	    hikey = PageGetItemId(page, 0);

	} while (opaque->btpo_next != P_NONE
		 && _bt_skeyless(rel, keysz, scankey, page, hikey));
    }
    return (buf);
}

/*
 *  _bt_skeyless() -- is a given scan key strictly less than a particular
 *		      item on a page?
 *
 *	We ignore the sequence numbers stored in the btree item here.  Those
 *	numbers are intended for use internally only, in repositioning a
 *	scan after a page split.  They do not impose any meaningful ordering.
 */

bool
_bt_skeyless(rel, keysz, scankey, page, itemid)
    Relation rel;
    Size keysz;
    ScanKey scankey;
    Page page;
    ItemId itemid;
{
    BTItem item;
    IndexTuple indexTuple;
    TupleDescriptor tupDes;
    ScanKeyEntry entry;
    int i;
    Datum attrDatum;
    Datum keyDatum;
    bool isNull;
    bool isLessThan;

    item = (BTItem) PageGetItem(page, itemid);
    indexTuple = &(item->bti_itup);

    tupDes = RelationGetTupleDescriptor(rel);

    /* see if it's less for than of the key attributes */
    for (i=1; i <= keysz; i++) {

	entry = &scankey->data[i-1];
	attrDatum = IndexTupleGetAttributeValue(indexTuple,
						entry->attributeNumber,
						tupDes, &isNull);
	keyDatum  = entry->argument;

	isLessThan = RelationInvokeBTreeStrategy(rel, i,
					         BTLessStrategyNumber,
					         attrDatum, keyDatum);
	if (!isLessThan)
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
     */

    if (PageIsEmpty(page) || (opaque->btpo_next != P_NONE && high == low))
	return (low);

    itupdesc = RelationGetTupleDescriptor(rel);
    match = false;

    while ((high - low) > 1) {
	mid = low + ((high - low) / 2);
	result = _bt_compare(rel, itupdesc, page, keysz, scankey, mid);

	if (result > 0)
	    low = mid + 1;
	else if (result < 0)
	    high = mid;
	else {
	    match = true;
	    break;
	}
    }

    /* if we found a match, we want to find the first one on the page */
    if (match) {
	while (mid > 0
	       && _bt_compare(rel, itupdesc, page, keysz, scankey, mid) == 0)
	    mid--;

        if (_bt_compare(rel, itupdesc, page, keysz, scankey, mid) != 0)
	    mid++;

	return (mid);
    } else {

	/*
	 *  We terminated because the endpoints got too close together.  There
	 *  are two cases to take care of.
	 *
	 *  For non-insertion searches on internal pages, we want to point at
	 *  the last key <= the scankey on the page.  This guarantees that
	 *  we'll descend the tree correctly.
	 *
	 *  For all other cases, we want to point at the first key >=
	 *  the scankey on the page.  This guarantees that scans and
	 *  insertions will happen correctly.
	 */

	opaque = (BTPageOpaque) PageGetSpecialPointer(page);
	if (!(opaque->btpo_flags & BTP_LEAF) && srchtype == BT_DESCENT) {

	    /* we want the last key <= the scankey */
	    result = _bt_compare(rel, itupdesc, page, keysz, scankey, high);

	    if (result == 0) {
		result = _bt_compare(rel, itupdesc, page,
				     keysz, scankey, low);
		if (result < 0)
		    return (high);
		return (low);
	    } else if (result > 0) {
		return (low);
	    } else {
		return (high);
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
	result = (int) fmgr(entry->procedure, entry->argument, datum);

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

    rel = scan->relation;
    current = &(scan->currentItemData);
    blkno = ItemPointerGetBlockNumber(current, 0);

    /* we still hold a read lock on the buffer */
    buf = _bt_getbuf(rel, blkno, BT_NONE);

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

	/* unpin, but don't unlock, the buffer */
	_bt_relbuf(rel, buf, BT_NONE);
    } else {
	ItemPointerSetInvalid(current);
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
 *	the current index tuple is read locked, but not pinned.
 *
 *	Locking interactions are critical, and you'll screw them up if you
 *	try to change this code without understanding them.
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
    int result;
    ScanKeyData skdata;

    /* if we just need to walk down one edge of the tree, do that */
    if (scan->scanFromEnd)
	return (_bt_endpoint(scan, dir));

    rel = scan->relation;
    itupdesc = RelationGetTupleDescriptor(scan->relation);
    current = &(scan->currentItemData);

    /*
     *  Okay, we want something more complicated.  What we'll do is use
     *  the first item in the scan key passed in (which has been correctly
     *  ordered to take advantage of index ordering) to position ourselves
     *  at the right place in the scan.
     */

    skdata.data[0].flags = 0x0;
    /*
     *  XXX -- The attribute number stored in the scan key is the attno
     *	       in the heap relation.  We need to transmogrify this into
     *         the index relation attno here.  For the nonce, however...
     *  skdata.data[0].attributeNumber = scan->keyData.data[0].attributeNumber;
     */
    skdata.data[0].attributeNumber = 1;
    skdata.data[0].argument = scan->keyData.data[0].argument;
    skdata.data[0].procedure = index_getprocid(rel,
					       skdata.data[0].attributeNumber,
					       BTORDER_PROC);

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

    /* now find the right place to start the scan */
    result = _bt_compare(rel, itupdesc, page, 1, &skdata, offind);
    strat = _bt_getstrat(rel, 1, scan->keyData.data[0].procedure);

    switch (strat) {
      case BTLessStrategyNumber:
	if (result <= 0) {
	    do {
		if (!_bt_step(scan, &buf, BackwardScanDirection))
		    return ((RetrieveIndexResult) NULL);

		offind = ItemPointerGetOffsetNumber(current, 0) - 1;
		page = BufferGetPage(buf, 0);
		result = _bt_compare(rel, itupdesc, page, 1, &skdata, offind);
	    } while (result <= 0);

	    if (!_bt_step(scan, &buf, ForwardScanDirection))
		return((RetrieveIndexResult) NULL);
	}
	break;

      case BTLessEqualStrategyNumber:
	if (result >= 0) {
	    do {
		if (!_bt_step(scan, &buf, ForwardScanDirection))
		    return ((RetrieveIndexResult) NULL);

		offind = ItemPointerGetOffsetNumber(current, 0) - 1;
		page = BufferGetPage(buf, 0);
		result = _bt_compare(rel, itupdesc, page, 1, &skdata, offind);
	    } while (result >= 0);

	    if (!_bt_step(scan, &buf, BackwardScanDirection))
		return((RetrieveIndexResult) NULL);
	}
	break;

      case BTEqualStrategyNumber:
	if (result != 0)
	  return ((RetrieveIndexResult) NULL);

      case BTGreaterEqualStrategyNumber:
	if (result <= 0) {
	    do {
		if (!_bt_step(scan, &buf, BackwardScanDirection))
		    return ((RetrieveIndexResult) NULL);

		offind = ItemPointerGetOffsetNumber(current, 0) - 1;
		page = BufferGetPage(buf, 0);
		result = _bt_compare(rel, itupdesc, page, 1, &skdata, offind);
	    } while (result <= 0);

	    if (!_bt_step(scan, &buf, ForwardScanDirection))
		return((RetrieveIndexResult) NULL);
	}
	break;

      case BTGreaterStrategyNumber:
	if (result >= 0) {
	    do {
		if (!_bt_step(scan, &buf, ForwardScanDirection))
		    return ((RetrieveIndexResult) NULL);

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

	/* unpin, but don't unlock, the buffer */
	_bt_relbuf(rel, buf, BT_NONE);
    } else {
	ItemPointerSetInvalid(current);
	_bt_relbuf(rel, buf, BT_READ);
	res = (RetrieveIndexResult) NULL;
    }

    return (res);
}

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
    ItemPointer current;
    Relation rel;

    rel = scan->relation;
    current = &(scan->currentItemData);
    offind = ItemPointerGetOffsetNumber(current, 0) - 1;
    page = BufferGetPage(*bufP, 0);
    opaque = (BTPageOpaque) PageGetSpecialPointer(page);
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
		return (false);
	    } else {

		/* walk right to the next page with data */
		_bt_relbuf(rel, *bufP, BT_READ);
		for (;;) {
		    *bufP = _bt_getbuf(rel, blkno, BT_READ);
		    page = BufferGetPage(*bufP, 0);
		    opaque = (BTPageOpaque) PageGetSpecialPointer(page);
		    if (!PageIsEmpty(page)) {
			break;
		    } else {
			blkno = opaque->btpo_next;
			_bt_relbuf(rel, *bufP, BT_READ);
			if (blkno == P_NONE) {
			    ItemPointerSetInvalid(current);
			    return (false);
			}
		    }
		}
		offind = 0;
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
		ItemPointerSetInvalid(current);
		return (false);
	    } else {

		/* walk right to the next page with data */
		_bt_relbuf(rel, *bufP, BT_READ);
		for (;;) {
		    *bufP = _bt_getbuf(rel, blkno, BT_READ);
		    page = BufferGetPage(*bufP, 0);
		    opaque = (BTPageOpaque) PageGetSpecialPointer(page);
		    maxoff = PageGetMaxOffsetIndex(page);

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
			_bt_relbuf(rel, *bufP, BT_READ);
			if (blkno == P_NONE) {
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
    ItemPointerSet(current, 0, blkno, 0, offind + 1);

    return (true);
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

	blkno = ItemPointerGetBlockNumber(&(itup->t_tid), 0);

	_bt_relbuf(rel, buf, BT_READ);
	buf = _bt_getbuf(rel, blkno, BT_READ);
	page = BufferGetPage(buf, 0);
	opaque = (BTPageOpaque) PageGetSpecialPointer(page);

	/*
	 *  Race condition:  If the child page we just stepped onto is in
	 *  the process of being split, we need to make sure we're all the
	 *  way at the edge of the tree.  See the paper by Lehman and Yao.
	 */

	if (ScanDirectionIsForward(dir) && opaque->btpo_next != P_NONE) {
	    do {
		blkno = opaque->btpo_next;
		_bt_relbuf(rel, buf, BT_READ);
		buf = _bt_getbuf(rel, blkno, BT_READ);
		page = BufferGetPage(buf, 0);
		opaque = (BTPageOpaque) PageGetSpecialPointer(page);
	    } while (opaque->btpo_next != P_NONE);
	} else if (ScanDirectionIsBackward(dir)
		    && opaque->btpo_prev != P_NONE) {
	    do {
		blkno = opaque->btpo_prev;
		_bt_relbuf(rel, buf, BT_READ);
		buf = _bt_getbuf(rel, blkno, BT_READ);
		page = BufferGetPage(buf, 0);
		opaque = (BTPageOpaque) PageGetSpecialPointer(page);
	    } while (opaque->btpo_prev != P_NONE);
	}
    }

    /* okay, we've got the {left,right}-most page in the tree */
    maxoff = PageGetMaxOffsetIndex(page);

    if (ScanDirectionIsForward(dir)) {
	maxoff = PageGetMaxOffsetIndex(page);
	if (opaque->btpo_next == P_NONE)
	    start = 0;
	else
	    start = 1;

	if (PageIsEmpty(page) || start > maxoff) {
	    ItemPointerSet(current, 0, blkno, 0, maxoff + 1);
	    if (!_bt_step(scan, &buf, ForwardScanDirection))
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
	    if (!_bt_step(scan, &buf, BackwardScanDirection))
		return ((RetrieveIndexResult) NULL);

	    start = ItemPointerGetOffsetNumber(current, 0);
	    page = BufferGetPage(buf);
	} else {
	    ItemPointerSet(current, 0, blkno, 0, start + 1);
	}
    } else {
	elog(WARN, "Illegal scan direction %d", dir);
    }

    btitem = (BTItem) PageGetItem(page, PageGetItemId(page, start));
    itup = &(btitem->bti_itup);
    iptr = (ItemPointer) palloc(sizeof(ItemPointerData));
    bcopy((char *) &(itup->t_tid), (char *) iptr, sizeof(ItemPointerData));
    res = ItemPointerFormRetrieveIndexResult(current, iptr);

    /* unpin, but don't unlock, the buffer */
    _bt_relbuf(rel, buf, BT_NONE);

    return (res);
}
