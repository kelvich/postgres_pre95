/*
 *  nobtinsert.c -- Item insertion in Lehman and Yao btrees for Postgres.
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
#include "access/nbtree.h"

RcsId("$Header$");

/*
 *  _nobt_doinsert() -- Handle insertion of a single btitem in the tree.
 *
 *	This routine is called by the public interface routines, nobtbuild
 *	and nobtinsert.  By here, btitem is filled in, and has a unique
 *	(xid, seqno) pair.
 */

InsertIndexResult
_nobt_doinsert(rel, btitem)
    Relation rel;
    NOBTItem btitem;
{
    ScanKey itup_scankey;
    IndexTuple itup;
    NOBTStack stack;
    Buffer buf;
    BlockNumber blkno;
    OffsetIndex itup_off;
    int natts;
    InsertIndexResult res;

    itup = &(btitem->nobti_itup);

    /* we need a scan key to do our search, so build one */
    itup_scankey = _nobt_mkscankey(rel, itup);
    natts = rel->rd_rel->relnatts;

    /* find the page containing this key */
    stack = _nobt_search(rel, natts, itup_scankey, &buf);
    blkno = BufferGetBlockNumber(buf);

    /* trade in our read lock for a write lock */
    _nobt_relbuf(rel, buf, NOBT_READ);
    buf = _nobt_getbuf(rel, blkno, NOBT_WRITE);

    /*
     *  If the page was split between the time that we surrendered our
     *  read lock and acquired our write lock, then this page may no
     *  longer be the right place for the key we want to insert.  In this
     *  case, we need to move right in the tree.  See Lehman and Yao for
     *  an excruciatingly precise description.
     */

    buf = _nobt_moveright(rel, buf, natts, itup_scankey, NOBT_WRITE);

    /* do the insertion */
    res = _nobt_insertonpg(rel, buf, stack, natts, itup_scankey,
			 btitem, (NOBTItem) NULL);

    /* be tidy */
    _nobt_freestack(stack);
    _nobt_freeskey(itup_scankey);

    return (res);
}

/*
 *  _nobt_insertonpg() -- Insert a tuple on a particular page in the index.
 *
 *	This recursive procedure does the following things:
 *
 *	    +  if necessary, splits the target page.
 *	    +  finds the right place to insert the tuple (taking into
 *	       account any changes induced by a split).
 *	    +  inserts the tuple.
 *	    +  if the page was split, pops the parent stack, and finds the
 *	       right place to insert the new child pointer (by walking
 *	       right using information stored in the parent stack).
 *	    +  invoking itself with the appropriate tuple for the right
 *	       child page on the parent.
 *
 *	On entry, we must have the right buffer on which to do the
 *	insertion, and the buffer must be pinned and locked.  On return,
 *	we will have dropped both the pin and the write lock on the buffer.
 *
 *	The locking interactions in this code are critical.  You should
 *	grok Lehman and Yao's paper before making any changes.  In addition,
 *	you need to understand how we disambiguate duplicate keys in this
 *	implementation, in order to be able to find our location using
 *	L&Y "move right" operations.  Since we may insert duplicate user
 *	keys, and since these dups may propogate up the tree, we use the
 *	'afteritem' parameter to position ourselves correctly for the
 *	insertion on internal pages.
 */

InsertIndexResult
_nobt_insertonpg(rel, buf, stack, keysz, scankey, btitem, afteritem)
    Relation rel;
    Buffer buf;
    NOBTStack stack;
    int keysz;
    ScanKey scankey;
    NOBTItem btitem;
    NOBTItem afteritem;
{
    InsertIndexResult res;
    Page page;
    Buffer rbuf;
    Buffer pbuf;
    Page rpage;
    ScanKey newskey;
    NOBTItem ritem;
    NOBTPageOpaque rpageop;
    BlockNumber rbknum, itup_blkno;
    OffsetIndex itup_off;
    int itemsz;
    int ritemsz;
    ItemId hikey;
    InsertIndexResult newres;
    NOBTItem new_item = (NOBTItem) NULL;

    page = BufferGetPage(buf, 0);
    itemsz = IndexTupleDSize(btitem->nobti_itup)
	   + (sizeof(NONOBTItemData) - sizeof(IndexTupleData));

    if (PageGetFreeSpace(page) < itemsz) {

	/* split the buffer into left and right halves */
	rbuf = _nobt_split(rel, buf);

	/* which new page (left half or right half) gets the tuple? */
	if (_nobt_goesonpg(rel, buf, keysz, scankey, afteritem)) {
	    itup_off = _nobt_pgaddtup(rel, buf, keysz, scankey,
				    itemsz, btitem, afteritem);
	    itup_blkno = BufferGetBlockNumber(buf);
	} else {
	    itup_off = _nobt_pgaddtup(rel, rbuf, keysz, scankey,
				    itemsz, btitem, afteritem);
	    itup_blkno = BufferGetBlockNumber(rbuf);
	}

	/*
	 *  By here,
	 *
	 *	+  our target page has been split;
	 *	+  the original tuple has been inserted;
	 *	+  we have write locks on both the old (left half) and new
	 *	   (right half) buffers, after the split; and
	 *	+  we have the key we want to insert into the parent.
	 *
	 *  Do the parent insertion.  We need to hold onto the locks for
	 *  the child pages until we locate the parent, but we can release
	 *  them before doing the actual insertion (see Lehman and Yao for
	 *  the reasoning).
	 */

	if (stack == (NOBTStack) NULL) {

	    /* create a new root node and release the split buffers */
	    _nobt_newroot(rel, buf, rbuf);
	    _nobt_relbuf(rel, buf, NOBT_WRITE);
	    _nobt_relbuf(rel, rbuf, NOBT_WRITE);

	} else {

	    /* form a index tuple that points at the new right page */
	    rbknum = BufferGetBlockNumber(rbuf);
	    rpage = BufferGetPage(rbuf, 0);
	    rpageop = (NOBTPageOpaque) PageGetSpecialPointer(rpage);

	    /*
	     *  By convention, the first entry (0) on every non-leftmost page
	     *  is the high key for that page.  In order to get the lowest key
	     *  on the new right page, we actually look at its second (1) entry.
	     */

	    if (rpageop->nobtpo_next != P_NONE)
		ritem = (NOBTItem) PageGetItem(rpage, PageGetItemId(rpage, 1));
	    else
		ritem = (NOBTItem) PageGetItem(rpage, PageGetItemId(rpage, 0));

	    /* get a unique btitem for this key */
	    new_item = _nobt_formitem(&(ritem->nobti_itup),
				    GetCurrentTransactionId(),
				    NOBTSEQ_GET());

	    ItemPointerSet(&(new_item->nobti_itup.t_tid), 0, rbknum, 0, 1);

	    /* find the parent buffer */
	    pbuf = _nobt_getstackbuf(rel, stack, NOBT_WRITE);

	    /* don't need the children anymore */
	    _nobt_relbuf(rel, buf, NOBT_WRITE);
	    _nobt_relbuf(rel, rbuf, NOBT_WRITE);

	    newskey = _nobt_mkscankey(rel, &(new_item->nobti_itup));
	    newres = _nobt_insertonpg(rel, pbuf, stack->nobts_parent,
				    keysz, newskey, new_item,
				    stack->nobts_btitem);

	    /* be tidy */
	    pfree(newres);
	    pfree(newskey);
	    pfree(new_item);
	}
    } else {
	itup_off = _nobt_pgaddtup(rel, buf, keysz, scankey,
				itemsz, btitem, afteritem);
	itup_blkno = BufferGetBlockNumber(buf);

	_nobt_relbuf(rel, buf, NOBT_WRITE);
    }

    /* by here, the new tuple is inserted */
    res = (InsertIndexResult) palloc(sizeof(InsertIndexResultData));
    ItemPointerSet(&(res->pointerData), 0, itup_blkno, 0, itup_off);
    res->lock = (RuleLock) NULL;
    res->offset = (double) 0;

    return (res);
}

/*
 *  _nobt_split() -- split a page in the btree.
 *
 *	On entry, buf is the page to split, and is write-locked and pinned.
 *	Returns the new right sibling of buf, pinned and write-locked.  The
 *	pin and lock on buf are maintained.
 */

Buffer
_nobt_split(rel, buf)
    Relation rel;
    Buffer buf;
{
    Buffer rbuf;
    Page origpage;
    Page leftpage, rightpage;
    BlockNumber lbkno, rbkno;
    NOBTPageOpaque ropaque, lopaque, oopaque;
    Buffer sbuf;
    Page spage;
    NOBTPageOpaque sopaque;
    int itemsz;
    ItemId itemid;
    NOBTItem item;
    int nleft, nright;
    OffsetIndex start;
    OffsetIndex maxoff;
    OffsetIndex firstright;
    OffsetIndex i;
    Size llimit;

    rbuf = _nobt_getbuf(rel, P_NEW, NOBT_WRITE);
    origpage = BufferGetPage(buf, 0);
    leftpage = PageGetTempPage(origpage, sizeof(NONOBTPageOpaqueData));
    rightpage = BufferGetPage(rbuf, 0);

    _nobt_pageinit(rightpage);
    _nobt_pageinit(leftpage);

    /* init btree private data */
    oopaque = (NOBTPageOpaque) PageGetSpecialPointer(origpage);
    lopaque = (NOBTPageOpaque) PageGetSpecialPointer(leftpage);
    ropaque = (NOBTPageOpaque) PageGetSpecialPointer(rightpage);

    /* if we're splitting this page, it won't be the root when we're done */
    oopaque->nobtpo_flags &= ~NOBTP_ROOT;
    lopaque->nobtpo_flags = ropaque->nobtpo_flags = oopaque->nobtpo_flags;
    lopaque->nobtpo_prev = oopaque->nobtpo_prev;
    ropaque->nobtpo_prev = BufferGetBlockNumber(buf);
    lopaque->nobtpo_next = BufferGetBlockNumber(rbuf);
    ropaque->nobtpo_next = oopaque->nobtpo_next;

    /*
     *  If the page we're splitting is not the rightmost page at its level
     *  in the tree, then the first (0) entry on the page is the high key
     *  for the page.  We need to copy that to the right half.  Otherwise,
     *  we should treat the line pointers beginning at zero as user data.
     *
     *  We leave a blank space at the start of the line table for the left
     *  page.  We'll come back later and fill it in with the high key item
     *  we get from the right key.
     */

    nleft = 2;
    nright = 1;
    if ((ropaque->nobtpo_next = oopaque->nobtpo_next) != P_NONE) {
	start = 1;
	itemid = PageGetItemId(origpage, 0);
	itemsz = ItemIdGetLength(itemid);
	item = (NOBTItem) PageGetItem(origpage, itemid);
	PageAddItem(rightpage, item, itemsz, nright++, LP_USED);
    } else {
	start = 0;
    }
    maxoff = PageGetMaxOffsetIndex(origpage);
    llimit = PageGetFreeSpace(leftpage) / 2;
    firstright = _nobt_findsplitloc(rel, origpage, start, maxoff, llimit);

    for (i = start; i <= maxoff; i++) {
	itemid = PageGetItemId(origpage, i);
	itemsz = ItemIdGetLength(itemid);
	item = (NOBTItem) PageGetItem(origpage, itemid);

	/* decide which page to put it on */
	if (i < firstright)
	    PageAddItem(leftpage, item, itemsz, nleft++, LP_USED);
	else
	    PageAddItem(rightpage, item, itemsz, nright++, LP_USED);
    }

    /*
     *  Okay, page has been split, high key on right page is correct.  Now
     *  set the high key on the left page to be the min key on the right
     *  page.
     */

    if (ropaque->nobtpo_next == P_NONE)
	itemid = PageGetItemId(rightpage, 0);
    else
	itemid = PageGetItemId(rightpage, 1);
    itemsz = ItemIdGetLength(itemid);
    item = (NOBTItem) PageGetItem(rightpage, itemid);

    /*
     *  We left a hole for the high key on the left page; fill it.  The
     *  modal crap is to tell the page manager to put the new item on the
     *  page and not screw around with anything else.  Whoever designed
     *  this interface has presumably crawled back into the dung heap they
     *  came from.  No one here will admit to it.
     */

    PageManagerModeSet(OverwritePageManagerMode);
    PageAddItem(leftpage, item, itemsz, 1, LP_USED);
    PageManagerModeSet(ShufflePageManagerMode);

    /*
     *  By here, the original data page has been split into two new halves,
     *  and these are correct.  The algorithm requires that the left page
     *  never move during a split, so we copy the new left page back on top
     *  of the original.  Note that this is not a waste of time, since we
     *  also require (in the page management code) that the center of a
     *  page always be clean, and the most efficient way to guarantee this
     *  is just to compact the data by reinserting it into a new left page.
     */

    PageRestoreTempPage(leftpage, origpage);

    /* write these guys out */
    _nobt_wrtnorelbuf(rel, rbuf);
    _nobt_wrtnorelbuf(rel, buf);

    /*
     *  Finally, we need to grab the right sibling (if any) and fix the
     *  prev pointer there.  We are guaranteed that this is deadlock-free
     *  since no other writer will be moving holding a lock on that page
     *  and trying to move left, and all readers release locks on a page
     *  before trying to fetch its neighbors.
     */

    if (ropaque->nobtpo_next != P_NONE) {
	sbuf = _nobt_getbuf(rel, ropaque->nobtpo_next, NOBT_WRITE);
	spage = BufferGetPage(sbuf, 0);
	sopaque = (NOBTPageOpaque) PageGetSpecialPointer(spage);
	sopaque->nobtpo_prev = BufferGetBlockNumber(rbuf);

	/* write and release the old right sibling */
	_nobt_wrtbuf(rel, sbuf);
    }

    /* split's done */
    return (rbuf);
}

/*
 *  _nobt_findsplitloc() -- find a safe place to split a page.
 *
 *	In order to guarantee the proper handling of searches for duplicate
 *	keys, the first duplicate in the chain must either be the first
 *	item on the page after the split, or the entire chain must be on
 *	one of the two pages.  That is,
 *		[1 2 2 2 3 4 5]
 *	must become
 *		[1] [2 2 2 3 4 5]
 *	or
 *		[1 2 2 2] [3 4 5]
 *	but not
 *		[1 2 2] [2 3 4 5].
 *	However,
 *		[2 2 2 2 2 3 4]
 *	may be split as
 *		[2 2 2 2] [2 3 4].
 */

_nobt_findsplitloc(rel, page, start, maxoff, llimit)
    Relation rel;
    Page page;
    OffsetIndex start;
    OffsetIndex maxoff;
    Size llimit;
{
    OffsetIndex i;
    OffsetIndex saferight;
    ItemId nxtitemid, safeitemid;
    NOBTItem safeitem, nxtitem;
    IndexTuple safetup, nxttup;
    Size nbytes;
    TupleDescriptor itupdesc;
    int natts;
    int attno;
    Datum attsafe;
    Datum attnext;
    Boolean null;

    itupdesc = RelationGetTupleDescriptor(rel);
    natts = rel->rd_rel->relnatts;

    saferight = start;
    safeitemid = PageGetItemId(page, saferight);
    nbytes = ItemIdGetLength(safeitemid) + sizeof(ItemIdData);
    safeitem = (NOBTItem) PageGetItem(page, safeitemid);
    safetup = &(safeitem->nobti_itup);

    i = start + 1;

    while (nbytes < llimit) {

	/* check the next item on the page */
	nxtitemid = PageGetItemId(page, i);
	nbytes += (ItemIdGetLength(nxtitemid) + sizeof(ItemIdData));
	nxtitem = (NOBTItem) PageGetItem(page, nxtitemid);
	nxttup = &(nxtitem->nobti_itup);

	/* test against last known safe item */
	for (attno = 1; attno <= natts; attno++) {
	    attsafe = (Datum) IndexTupleGetAttributeValue(safetup, attno,
							  itupdesc, &null);
	    attnext = (Datum) IndexTupleGetAttributeValue(nxttup, attno,
							  itupdesc, &null);

	    /*
	     *  If the tuple we're looking at isn't equal to the last safe one
	     *  we saw, then it's our new safe tuple.
	     */

	    if (!_nobt_invokestrat(rel, attno, NOBTEqualStrategyNumber,
				 attsafe, attnext)) {
		safetup = nxttup;
		saferight = i;

		/* break is for the attno for loop */
		break;
	    }
	}
	i++;
    }

    /*
     *  If the chain of dups starts at the beginning of the page and extends
     *  past the halfway mark, we can split it in the middle.
     */

    if (saferight == start)
	saferight = i;

    return (saferight);
}

/*
 *  _nobt_newroot() -- Create a new root page for the index.
 *
 *	We've just split the old root page and need to create a new one.
 *	In order to do this, we add a new root page to the file, then lock
 *	the metadata page and update it.  This is guaranteed to be deadlock-
 *	free, because all readers release their locks on the metadata page
 *	before trying to lock the root, and all writers lock the root before
 *	trying to lock the metadata page.  We have a write lock on the old
 *	root page, so we have not introduced any cycles into the waits-for
 *	graph.
 *
 *	On entry, lbuf (the old root) and rbuf (its new peer) are write-
 *	locked.  We don't drop the locks in this routine; that's done by
 *	the caller.  On exit, a new root page exists with entries for the
 *	two new children.  The new root page is neither pinned nor locked.
 */

_nobt_newroot(rel, lbuf, rbuf)
    Relation rel;
    Buffer lbuf;
    Buffer rbuf;
{
    Buffer rootbuf;
    Page lpage, rpage, rootpage;
    BlockNumber lbkno, rbkno;
    BlockNumber rootbknum;
    NOBTPageOpaque rootopaque;
    ItemId itemid;
    NOBTItem item;
    int itemsz;
    NOBTItem new_item;

    /* get a new root page */
    rootbuf = _nobt_getbuf(rel, P_NEW, NOBT_WRITE);
    rootpage = BufferGetPage(rootbuf, 0);
    _nobt_pageinit(rootpage);

    /* set btree special data */
    rootopaque = (NOBTPageOpaque) PageGetSpecialPointer(rootpage);
    rootopaque->nobtpo_prev = rootopaque->nobtpo_next = P_NONE;
    rootopaque->nobtpo_flags |= NOBTP_ROOT;

    /*
     *  Insert the internal tuple pointers.
     */

    lbkno = BufferGetBlockNumber(lbuf);
    rbkno = BufferGetBlockNumber(rbuf);
    lpage = BufferGetPage(lbuf, 0);
    rpage = BufferGetPage(rbuf, 0);

    /* step over the high key on the left page */
    itemid = PageGetItemId(lpage, 1);
    itemsz = ItemIdGetLength(itemid);
    item = (NOBTItem) PageGetItem(lpage, itemid);
    new_item = _nobt_formitem(&(item->nobti_itup),
			    GetCurrentTransactionId(),
			    NOBTSEQ_GET());
    ItemPointerSet(&(new_item->nobti_itup.t_tid), 0, lbkno, 0, 1);

    /* insert the left page pointer */
    PageAddItem(rootpage, new_item, itemsz, 1, LP_USED);
    pfree(new_item);

    itemid = PageGetItemId(rpage, 0);
    itemsz = ItemIdGetLength(itemid);
    item = (NOBTItem) PageGetItem(rpage, itemid);
    new_item = _nobt_formitem(&(item->nobti_itup),
			    GetCurrentTransactionId(),
			    NOBTSEQ_GET());
    ItemPointerSet(&(new_item->nobti_itup.t_tid), 0, rbkno, 0, 1);

    /* insert the right page pointer */
    PageAddItem(rootpage, new_item, itemsz, 2, LP_USED);
    pfree (new_item);

    /* write and let go of the root buffer */
    rootbknum = BufferGetBlockNumber(rootbuf);
    _nobt_wrtbuf(rel, rootbuf);

    /* update metadata page with new root block number */
    _nobt_metaproot(rel, rootbknum);
}

/*
 *  _nobt_pgaddtup() -- add a tuple to a particular page in the index.
 *
 *	This routine adds the tuple to the page as requested, and keeps the
 *	write lock and reference associated with the page's buffer.  It is
 *	an error to call pgaddtup() without a write lock and reference.  If
 *	afteritem is non-null, it's the item that we expect our new item
 *	to follow.  Otherwise, we do a binary search for the correct place
 *	and insert the new item there.
 */

OffsetIndex
_nobt_pgaddtup(rel, buf, keysz, itup_scankey, itemsize, btitem, afteritem)
    Relation rel;
    Buffer buf;
    int keysz;
    ScanKey itup_scankey;
    int itemsize;
    NOBTItem btitem;
    NOBTItem afteritem;
{
    OffsetIndex itup_off, maxoff;
    ItemId itemid;
    NOBTItem olditem;
    Page page;
    NOBTPageOpaque opaque;
    ScanKey tmpskey;
    NOBTItem chkitem;
    int chknbytes;

    page = BufferGetPage(buf, 0);
    opaque = (NOBTPageOpaque) PageGetSpecialPointer(page);

    if (afteritem == (NOBTItem) NULL) {
	itup_off = _nobt_binsrch(rel, buf, keysz, itup_scankey, NOBT_INSERTION) + 1;
    } else {
	chknbytes = sizeof(NONOBTItemData) - sizeof(IndexTupleData);
	tmpskey = _nobt_mkscankey(rel, &(afteritem->nobti_itup));
	itup_off = _nobt_binsrch(rel, buf, keysz, tmpskey, NOBT_INSERTION);

	for (;;) {
	    chkitem = (NOBTItem) PageGetItem(page,
					   PageGetItemId(page, itup_off++));
	    if (bcmp((char *) chkitem, (char *) afteritem, chknbytes) == 0)
		break;
	}

	/* we want to be after the item */
	itup_off++;
    }

    PageAddItem(page, btitem, itemsize, itup_off, LP_USED);

    /* write the buffer, but hold our lock */
    _nobt_wrtnorelbuf(rel, buf);

    return (itup_off);
}

/*
 *  _nobt_goesonpg() -- Does a new tuple belong on this page?
 *
 *	This is part of the complexity introduced by allowing duplicate
 *	keys into the index.  The tuple belongs on this page if:
 *
 *		+ there is no page to the right of this one; or
 *		+ it is less than the high key on the page; or
 *		+ the item it is to follow ("afteritem") appears on this
 *		  page.
 */

bool
_nobt_goesonpg(rel, buf, keysz, scankey, afteritem)
    Relation rel;
    Buffer buf;
    Size keysz;
    ScanKey scankey;
    NOBTItem afteritem;
{
    Page page;
    ItemId hikey;
    NOBTPageOpaque opaque;
    ItemId itemid;
    NOBTItem chkitem;
    int cmpbytes;
    OffsetIndex offind, maxoff;
    ScanKey tmpskey;
    bool found;

    page = BufferGetPage(buf, 0);

    /* no right neighbor? */
    opaque = (NOBTPageOpaque) PageGetSpecialPointer(page);
    if (opaque->nobtpo_next == P_NONE)
	return (true);

    /* < the high key? */
    hikey = PageGetItemId(page, 0);
    if (_nobt_skeycmp(rel, keysz, scankey, page, hikey, NOBTLessStrategyNumber))
	return (true);

    /*
     *  If the scan key is > the high key, then it for sure doesn't belong
     *  here.
     */

    if (_nobt_skeycmp(rel, keysz, scankey, page, hikey, NOBTGreaterStrategyNumber))
	return (false);

    /*
     *  If we have no adjacency information, and the item is equal to the
     *  high key on the page (by here it is), then the item does not belong
     *  on this page.
     */

    if (afteritem == (NOBTItem) NULL)
	return (false);

    /* damn, have to work for it.  i hate that. */
    tmpskey = _nobt_mkscankey(rel, &(afteritem->nobti_itup));
    maxoff = PageGetMaxOffsetIndex(page);
    offind = _nobt_binsrch(rel, buf, keysz, tmpskey, NOBT_INSERTION);

    /* only need to look at unique xid/seqno pair */
    cmpbytes = sizeof(NONOBTItemData) - sizeof(IndexTupleData);

    /*
     *  This loop will terminate quickly.  Because of the way that splits
     *  work, if we start at some location on the page, we're guaranteed
     *  to hit the tuple we're looking for before we get to the next key
     *  of a different value on the page.  Lucky, huh?
     */

    found = false;
    while (offind <= maxoff) {
	chkitem = (NOBTItem) PageGetItem(page, PageGetItemId(page, offind));
	if (bcmp((char *) chkitem, (char *) afteritem, cmpbytes) == 0) {
	    found = true;
	    break;
	}
	offind++;
    }

    _nobt_freeskey(tmpskey);
    return (found);
}
