/*
 *  btinsert.c -- Item insertion in Lehman and Yao btrees for Postgres.
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

#include "/n/eden/users/mao/postgres/src/access/nbtree/nbtree.h"

RcsId("$Header$");

/*
 *  _bt_insertonpg() -- Insert a tuple on a particular page in the index.
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
 *	grok Lehman and Yao's paper before making any changes.
 */

InsertIndexResult
_bt_insertonpg(rel, buf, stack, keysz, scankey, btitem)
    Relation rel;
    Buffer buf;
    BTStack stack;
    int keysz;
    ScanKey scankey;
    BTItem btitem;
{
    InsertIndexResult res;
    Page page;
    Buffer rbuf;
    Buffer pbuf;
    Page rpage;
    ScanKey newskey;
    BTItem ritem;
    BTPageOpaque rpageop;
    BlockNumber rbknum, itup_blkno;
    OffsetIndex itup_off;
    int itemsz;
    int ritemsz;
    ItemId hikey;
    InsertIndexResult newres;
    BTItem new_item = (BTItem) NULL;

    page = BufferGetPage(buf, 0);
    itemsz = btitem->bti_itup.t_size
		+ (sizeof(BTItemData) - sizeof(IndexTupleData));

    if (PageGetFreeSpace(page) < itemsz) {

	/* split the buffer into left and right halves */
	rbuf = _bt_split(rel, buf);

	/*
	 *  On return from _bt_split(), both the left (old) and right (new)
	 *  halves of buf are locked.  We need to do an insertion into the
	 *  parent page for the new right page.  For this, we need an index
	 *  tuple pointing to the right page.
	 *
	 *  The fact that new_item is non-null will indicate to us that we
	 *  have split the page later, when we're ready to propogate the
	 *  split up the tree.
	 */
	rbknum = BufferGetBlockNumber(rbuf);
	rpage = BufferGetPage(rbuf, 0);
	rpageop = (BTPageOpaque) PageGetSpecialPointer(rpage);

	/*
	 *  By convention, the first entry (0) on every non-leftmost page
	 *  is the high key for that page.  In order to get the lowest key
	 *  on the new right page, we actually look at its second (1) entry.
	 */

	if (rpageop->btpo_next != P_NONE)
	    ritem = (BTItem) PageGetItem(rpage, PageGetItemId(rpage, 1));
	else
	    ritem = (BTItem) PageGetItem(rpage, PageGetItemId(rpage, 0));

	ritemsz = ritem->bti_itup.t_size
		    + (sizeof(BTItemData) - sizeof(IndexTupleData));
	new_item = (BTItem) palloc(ritemsz);
	bcopy((char *) ritem, (char *) new_item, ritemsz);
	ItemPointerSet(&(new_item->bti_itup.t_tid), 0, rbknum, 0, 1);
    }

    /*
     *  Okay, if we split the page, we may need to add the new tuple to
     *  the new (right) half, rather than the old (left) half.  Figure
     *  out if that's the case, and do the right thing.
     */

    if (new_item != (BTItem) NULL) {

	/*
	 *  We split the page.  Figure out which half to put the
	 *  original tuple on.
	 */

	hikey = PageGetItemId(page, 0);
	if (!_bt_skeyless(rel, keysz, scankey, page, hikey)) {
	    itup_off = _bt_pgaddtup(rel, rbuf, keysz, scankey, itemsz, btitem);
	    itup_blkno = BufferGetBlockNumber(rbuf);
	} else {
	    itup_off = _bt_pgaddtup(rel, buf, keysz, scankey, itemsz, btitem);
	    itup_blkno = BufferGetBlockNumber(buf);
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

	if (stack == (BTStack) NULL) {

	    /* create a new root node and release the split buffers */
	    _bt_newroot(rel, buf, rbuf);
	    _bt_relbuf(rel, buf, BT_WRITE);
	    _bt_relbuf(rel, rbuf, BT_WRITE);

	} else {

	    /* find the parent buffer */
	    pbuf = _bt_getstackbuf(rel, stack, BT_WRITE);

	    /* don't need the children anymore */
	    _bt_relbuf(rel, buf, BT_WRITE);
	    _bt_relbuf(rel, rbuf, BT_WRITE);

	    newskey = _bt_mkscankey(rel, &(new_item->bti_itup));
	    newres = _bt_insertonpg(rel, pbuf, stack->bts_parent,
				    keysz, newskey, new_item);

	    /* don't really need the result for internal insertions */
	    pfree(newres);
	    pfree(newskey);
	}
	pfree(new_item);
    } else {
	itup_off = _bt_pgaddtup(rel, buf, keysz, scankey, itemsz, btitem);
	itup_blkno = BufferGetBlockNumber(buf);

	_bt_relbuf(rel, buf, BT_WRITE);
    }

    _bt_dump(rel);

    /* by here, the new tuple is inserted */
    res = (InsertIndexResult) palloc(sizeof(InsertIndexResultData));
    ItemPointerSet(&(res->pointerData), 0, itup_blkno, 0, itup_off);
    res->lock = (RuleLock) NULL;
    res->offset = (double) 0;

    return (res);
}

Buffer
_bt_split(rel, buf)
    Relation rel;
    Buffer buf;
{
    Buffer rbuf;
    Page origpage;
    Page leftpage, rightpage;
    BlockNumber lbkno, rbkno;
    BTPageOpaque ropaque, lopaque, oopaque;
    int itemsz;
    ItemId itemid;
    BTItem item;
    int nleft, nright;
    OffsetIndex start;
    OffsetIndex maxoff;
    OffsetIndex i;
    int nmoved;
    int llimit;

    rbuf = _bt_getbuf(rel, P_NEW, BT_WRITE);
    origpage = BufferGetPage(buf, 0);
    leftpage = PageGetTempPage(origpage, sizeof(BTPageOpaqueData));
    rightpage = BufferGetPage(rbuf, 0);

    _bt_pageinit(rightpage);
    _bt_pageinit(leftpage);

    /* init btree private data */
    oopaque = (BTPageOpaque) PageGetSpecialPointer(origpage);
    lopaque = (BTPageOpaque) PageGetSpecialPointer(leftpage);
    ropaque = (BTPageOpaque) PageGetSpecialPointer(rightpage);
    lopaque->btpo_flags = ropaque->btpo_flags = oopaque->btpo_flags;
    lopaque->btpo_prev = oopaque->btpo_prev;
    ropaque->btpo_prev = BufferGetBlockNumber(buf);
    lopaque->btpo_next = BufferGetBlockNumber(rbuf);

    /*
     *  If the page we're splitting is not the rightmost page at its level
     *  in the tree, then the first (0) entry on the page is the high key
     *  for the page.  We need to copy that to the right half.  Otherwise,
     *  we should treat the line pointers beginning at zero as user data.
     */

    nleft = 2;
    nright = 1;
    if ((ropaque->btpo_next = oopaque->btpo_next) != P_NONE) {
	start = 1;
	itemid = PageGetItemId(origpage, 0);
	itemsz = ItemIdGetLength(itemid);
	item = (BTItem) PageGetItem(origpage, itemid);
	PageAddItem(rightpage, item, itemsz, nright++, LP_USED);
    } else {
	start = 0;
    }
    maxoff = PageGetMaxOffsetIndex(origpage);
    llimit = PageGetFreeSpace(leftpage) / 2;
    nmoved = 0;

    for (i = start; i <= maxoff; i++) {
	itemid = PageGetItemId(origpage, i);
	itemsz = ItemIdGetLength(itemid);
	item = (BTItem) PageGetItem(origpage, itemid);
	if (nmoved < llimit) {
	    PageAddItem(leftpage, item, itemsz, nleft++, LP_USED);
	    nmoved += (itemsz + sizeof(ItemIdData));
	} else {
	    PageAddItem(rightpage, item, itemsz, nright++, LP_USED);
	    nmoved += (itemsz + sizeof(ItemIdData));
	}
    }

    /*
     *  Okay, data has been split, high key on right page is correct.  Now
     *  set the high key on the left page to be the min key on the right
     *  page.
     */

    itemid = PageGetItemId(rightpage, 0);
    itemsz = ItemIdGetLength(itemid);
    item = (BTItem) PageGetItem(rightpage, itemid);
    PageAddItem(leftpage, item, itemsz, 1, LP_USED);

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

    /* split's done */
    return (rbuf);
}

/*
 *  _bt_newroot() -- Create a new root page for the index.
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
 *	two new children.
 */

_bt_newroot(rel, lbuf, rbuf)
    Relation rel;
    Buffer lbuf;
    Buffer rbuf;
{
    Buffer rootbuf;
    Page lpage, rpage, rootpage;
    BlockNumber lbkno, rbkno;
    BlockNumber rootbknum;
    BTPageOpaque rootopaque;
    ItemId itemid;
    BTItem item;
    int itemsz;
    BTItem new_item;

    /* get a new root page */
    rootbuf = _bt_getbuf(rel, P_NEW, BT_WRITE);
    rootpage = BufferGetPage(rootbuf, 0);
    _bt_pageinit(rootpage);

    /* set btree special data */
    rootopaque = (BTPageOpaque) PageGetSpecialPointer(rootpage);
    rootopaque->btpo_prev = rootopaque->btpo_next = P_NONE;
    rootopaque->btpo_flags = 0x0;

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
    item = (BTItem) PageGetItemId(lpage, itemid);

    new_item = (BTItem) palloc(itemsz);
    bcopy((char *) item, (char *) new_item, itemsz);
    ItemPointerSet(&(new_item->bti_itup.t_tid), 0, lbkno, 0, 1);

    /* insert the left page pointer */
    PageAddItem(rootpage, new_item, itemsz, 1, LP_USED);

    itemid = PageGetItemId(rpage, 1);
    itemsz = ItemIdGetLength(itemid);
    item = (BTItem) PageGetItem(rpage, itemid);

    if (itemsz > PSIZE(new_item)) {
	pfree(new_item);
	new_item = (BTItem) palloc(itemsz);
    }

    bcopy((char *) item, (char *) new_item, itemsz);
    ItemPointerSet(&(new_item->bti_itup.t_tid), 0, rbkno, 0, 1);

    /* insert the right page pointer */
    PageAddItem(rootpage, new_item, itemsz, 2, LP_USED);

    /*
     *  New root page is correct.  Now update the metadata page.
     */

    rootbknum = BufferGetBlockNumber(rootbuf);

    /* write and let go of the root buffer */
    _bt_wrtbuf(rel, rootbuf);

    /* update metadata page with new root block number */
    _bt_metaproot(rel, rootbknum);
}

/*
 *  _bt_pgaddtup() -- add a tuple to a particular page in the index.
 *
 *	This routine adds the tuple to the page as requested, and drops the
 *	write lock and reference associated with the page's buffer.  It is
 *	an error to call pgaddtup() without a write lock and reference.
 */

OffsetIndex
_bt_pgaddtup(rel, buf, keysz, itup_scankey, itemsize, btitem)
    Relation rel;
    Buffer buf;
    int keysz;
    ScanKey itup_scankey;
    int itemsize;
    BTItem btitem;
{
    OffsetIndex itup_off;
    Page page;

    itup_off = _bt_binsrch(rel, buf, keysz, itup_scankey, BT_INSERTION) + 1;
    page = BufferGetPage(buf, 0);

    PageAddItem(page, btitem, itemsize, itup_off, LP_USED);

    /* write the buffer and release our locks */
    _bt_wrtnorelbuf(rel, buf);

    return (itup_off);
}
