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
#include "access/nobtree.h"

RcsId("$Header$");

extern bool	NOBT_Building;
extern uint32	CurrentLinkToken;
extern int	NOBT_NSplits;

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
    NOBTLItem btitem;
{
    ScanKey itup_scankey;
    IndexTuple itup;
    NOBTStack stack;
    Buffer buf;
    BlockNumber blkno;
    OffsetIndex itup_off;
    int natts;
    InsertIndexResult res;

    itup = &(btitem->nobtli_itup);

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

    buf = _nobt_moveright(rel, buf, natts, itup_scankey, NOBT_WRITE, stack);

    /* do the insertion */
    res = _nobt_insertonpg(rel, buf, stack, natts, itup_scankey,
			   btitem, (NOBTIItem) NULL);

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
_nobt_insertonpg(rel, buf, stack, keysz, scankey, btvoid, afteritem)
    Relation rel;
    Buffer buf;
    NOBTStack stack;
    int keysz;
    ScanKey scankey;
    char *btvoid;
    NOBTIItem afteritem;
{
    NOBTIItem btiitem;
    NOBTLItem btlitem;
    InsertIndexResult res;
    Page page;
    bool isleaf;
    NOBTPageOpaque opaque;
    Buffer rbuf;
    Buffer pbuf;
    Page rpage;
    ScanKey newskey;
    NOBTPageOpaque rpageop;
    BlockNumber rbknum, itup_blkno;
    OffsetIndex itup_off;
    int itemsz;
    int ritemsz;
    ItemId hikey;
    InsertIndexResult newres;
    Page ppage;
    NOBTIItem lftitem, new_item;
    NOBTIItem riitem;
    NOBTLItem rlitem;
    char *datump;
    int dsize;

    page = BufferGetPage(buf, 0);
    opaque = (NOBTPageOpaque) PageGetSpecialPointer(page);
    if (opaque->nobtpo_flags & NOBTP_LEAF) {
	isleaf = true;
	btlitem = (NOBTLItem) btvoid;
	itemsz = IndexTupleDSize(btlitem->nobtli_itup)
	       + (sizeof(NOBTLItemData) - sizeof(IndexTupleData));
    } else {
	isleaf = false;
	btiitem = (NOBTIItem) btvoid;
	itemsz = NOBTIItemGetSize(btiitem);
    }

#ifdef	REORG
#define SYNC_EVERYTHING()
    /* can only split every page one time between syncs */
    if (opaque->nobtpo_linktok == CurrentLinkToken && !NOBT_Building) {
	/* have to flush dirty buffers, force everything down to disk */
	SYNC_EVERYTHING();
    }
#endif	/* REORG */

#ifndef	REORG
    if (PageGetFreeSpace(page) < itemsz) {
#else	/* REORG */
    if (PageGetFreeSpace(page) < ((2 * itemsz) + sizeof(uint16))) {
#endif	/* REORG */

	/* split the buffer into left and right halves */
	NOBT_NSplits++;
#ifdef	SHADOW
	if (NOBT_Building)
	    rbuf = _nobt_bsplit(rel, buf);
	else
	    rbuf = _nobt_nsplit(rel, &buf);
#endif	/* SHADOW */
#ifdef	REORG
	rbuf = _nobt_bsplit(rel, buf);
#endif	/* REORG */
#ifdef	NORMAL
	rbuf = _nobt_bsplit(rel, buf);
#endif	/* NORMAL */

	/* which new page (left half or right half) gets the tuple? */
	if (_nobt_goesonpg(rel, buf, keysz, scankey, afteritem)) {
	    itup_off = _nobt_pgaddtup(rel, buf, keysz, scankey,
				       itemsz, btvoid, afteritem);
	    itup_blkno = BufferGetBlockNumber(buf);
	} else {
	    itup_off = _nobt_pgaddtup(rel, rbuf, keysz, scankey,
				       itemsz, btvoid, afteritem);
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

	    if (rpageop->nobtpo_next != P_NONE) {
		if (isleaf) {
		    rlitem = (NOBTLItem) PageGetItem(rpage,
						     PageGetItemId(rpage, 1));
		} else {
		    riitem = (NOBTIItem) PageGetItem(rpage,
						     PageGetItemId(rpage, 1));
		}
	    } else {
		if (isleaf) {
		    rlitem = (NOBTLItem) PageGetItem(rpage,
						     PageGetItemId(rpage, 0));
		} else {
		    riitem = (NOBTIItem) PageGetItem(rpage,
						     PageGetItemId(rpage, 0));
		}
	    }

	    /* get a unique btitem for this key */
	    if (isleaf) {
		datump = (char *) rlitem;
		datump += sizeof(NOBTLItemData);
		dsize = IndexTupleSize(&(rlitem->nobtli_itup)) - sizeof(IndexTupleData);
	    } else {
		datump = (char *) riitem;
		datump += sizeof(NOBTIItemData);
		dsize = NOBTIItemGetSize(riitem) - sizeof(NOBTIItemData);
	    }
	    new_item = _nobt_formiitem(rbknum, datump, dsize);

	    /* find the parent buffer */
	    pbuf = _nobt_getstackbuf(rel, stack, NOBT_WRITE);

	    /* xyzzy should use standard l&y "walk right" alg here */
	    ppage = BufferGetPage(pbuf, 0);
	    lftitem = (NOBTIItem) PageGetItem(ppage,
				  	      PageGetItemId(ppage,
						  stack->nobts_offset));

#ifdef	SHADOW
	    lftitem->nobtii_oldchild = lftitem->nobtii_child;
#endif	/* SHADOW */

	    lftitem->nobtii_child = BufferGetBlockNumber(buf);

	    /* don't need the children anymore */
	    _nobt_relbuf(rel, buf, NOBT_WRITE);
	    _nobt_relbuf(rel, rbuf, NOBT_WRITE);

	    newskey = _nobt_imkscankey(rel, new_item);
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
				itemsz, btvoid, afteritem);
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
 *  _nobt_bsplit() -- split a page in the btree.
 *
 *	On entry, buf is the page to split, and is write-locked and pinned.
 *	Returns the new right sibling of buf, pinned and write-locked.  The
 *	pin and lock on buf are maintained.
 *
 *	This routine is used only when the tree is being built; for normal
 *	insertions, we must not reuse the left-hand page, and we must manage
 *	the list of free pages correctly.
 */

Buffer
_nobt_bsplit(rel, buf)
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
    NOBTIItem iitem;
    int nleft, nright;
    OffsetIndex start;
    OffsetIndex maxoff;
    OffsetIndex firstright;
    OffsetIndex i;
    Size llimit;
#ifdef	REORG
    int save_left_ind;
    LocationIndex save_left_lower;
    LocationIndex save_left_upper;
    PageHeader left_phdr;
#endif	/* REORG */

    rbuf = _nobt_getbuf(rel, P_NEW, NOBT_WRITE);
    origpage = BufferGetPage(buf, 0);
    leftpage = PageGetTempPage(origpage, sizeof(NOBTPageOpaqueData));
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
#ifndef	NORMAL
    lopaque->nobtpo_linktok = CurrentLinkToken;
#endif	/* ndef NORMAL */

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
	iitem = (NOBTIItem) PageGetItem(origpage, itemid);
	PageAddItem(rightpage, iitem, itemsz, nright++, LP_USED);
    } else {
	start = 0;
    }
    maxoff = PageGetMaxOffsetIndex(origpage);
    llimit = PageGetFreeSpace(leftpage) / 2;
    firstright = _nobt_findsplitloc(rel, origpage, start, maxoff, llimit);

    for (i = start; i <= maxoff; i++) {
	itemid = PageGetItemId(origpage, i);
	itemsz = ItemIdGetLength(itemid);
	iitem = (NOBTIItem) PageGetItem(origpage, itemid);

	/* decide which page to put it on */
	if (i < firstright) {
	    PageAddItem(leftpage, iitem, itemsz, nleft++, LP_USED);
	} else {
#ifdef	REORG
	    /* for reorg, must add to left page, too */
	    if (i == firstright) {
		left_phdr = (PageHeader) leftpage;
		save_left_ind = nleft;
		save_left_lower = left_phdr->pd_lower;
		save_left_upper = left_phdr->pd_upper;
	    }
	    PageAddItem(leftpage, iitem, itemsz, nleft++, LP_USED);
#endif	/* REORG */
	    PageAddItem(rightpage, iitem, itemsz, nright++, LP_USED);
	}
    }
#ifdef	REORG
    /* need to clean out flags in the linp entries, reset free space */
    for (i = save_left_ind; i < nleft; i++) {
	left_phdr->pd_linp[i].lp_flags &= ~LP_USED;
    }
    lopaque->nobtpo_oldlow = left_phdr->pd_lower;
    left_phdr->pd_lower = save_left_lower;
    left_phdr->pd_upper = save_left_upper;
#endif	/* REORG */

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
    iitem = (NOBTIItem) PageGetItem(rightpage, itemid);

    /*
     *  We left a hole for the high key on the left page; fill it.  The
     *  modal crap is to tell the page manager to put the new item on the
     *  page and not screw around with anything else.  Whoever designed
     *  this interface has presumably crawled back into the dung heap they
     *  came from.  No one here will admit to it.
     */

    PageManagerModeSet(OverwritePageManagerMode);
    PageAddItem(leftpage, iitem, itemsz, 1, LP_USED);
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

#ifdef	SHADOW
/*
 *  _nobt_nsplit() -- split a page in the btree.
 *
 *	On entry, buf is the page to split, and is write-locked and pinned.
 *	Returns the new right sibling of buf, pinned and write-locked.  The
 *	pin and lock on buf are maintained.
 *
 *	This routine is used only during normal page splits.  It does not
 *	reuse the left page, and manages the free list correctly.  During
 *	the initial build of the index, we don't need to bother with this,
 *	since building the index either commits or aborts atomically.
 */

Buffer
_nobt_nsplit(rel, bufP)
    Relation rel;
    Buffer *bufP;
{
    Buffer rbuf;
    Buffer buf;
    Page origpage;
    Page leftpage, rightpage;
    BlockNumber lbkno, rbkno;
    NOBTPageOpaque ropaque, lopaque, oopaque;
    Buffer sbuf;
    Page spage;
    NOBTPageOpaque sopaque;
    int itemsz;
    ItemId itemid;
    NOBTIItem iitem;
    int nleft, nright;
    OffsetIndex start;
    OffsetIndex maxoff;
    OffsetIndex firstright;
    OffsetIndex i;
    Size llimit;
    PageNumber repl;
    Buffer newsbuf;
    Page newspage;
    bool nobackup;
    NOBTPageOpaque newsopaque;

    buf = _nobt_getbuf(rel, P_NEW, NOBT_WRITE);
    rbuf = _nobt_getbuf(rel, P_NEW, NOBT_WRITE);
    origpage = BufferGetPage(*bufP, 0);
    leftpage = BufferGetPage(buf, 0);
    rightpage = BufferGetPage(rbuf, 0);

    _nobt_pageinit(leftpage);
    _nobt_pageinit(rightpage);

    /* init btree private data */
    oopaque = (NOBTPageOpaque) PageGetSpecialPointer(origpage);
    lopaque = (NOBTPageOpaque) PageGetSpecialPointer(leftpage);
    ropaque = (NOBTPageOpaque) PageGetSpecialPointer(rightpage);


    /* if we're splitting this page, it won't be the root when we're done */
    lopaque->nobtpo_flags = ropaque->nobtpo_flags
	= (oopaque->nobtpo_flags & ~NOBTP_ROOT);

    /*
     *  In order to guarantee atomicity of updates, we need a sentinel value
     *  in the peer pointers between the pages.  Note that we set these before
     *  we establish any path to the new pages.  We can set the peer pointers
     *  between the two new pages, since no one else will screw around with
     *  those.  There's no path to the new pages yet, so we don't need to
     *  bother locking the peer pointers before we update them.
     */

    lopaque->nobtpo_prev = P_NONE;
    ropaque->nobtpo_next = P_NONE;
    ropaque->nobtpo_prev = BufferGetBlockNumber(buf);
    lopaque->nobtpo_next = BufferGetBlockNumber(rbuf);

    /*
     *  No need for a critical section here, since these pages can't be
     *  written out by anybody.
     */

    lopaque->nobtpo_nexttok = ropaque->nobtpo_prevtok = CurrentLinkToken;

    /*
     *  Order of operations here is critical.  First, make sure that the
     *  link pointer tokens for the 'replaced' link agree.  Then set the
     *  'replaced' link for the original page.
     *
     *  The fake critical section calls block concurrent processes from
     *  doing a 'commit' (and associated sync) while we're in a critical
     *  section.
     */

#define CRIT_SEC_START
#define CRIT_SEC_END

    CRIT_SEC_START;
    oopaque->nobtpo_repltok = lopaque->nobtpo_linktok = CurrentLinkToken;
    CRIT_SEC_END;
    oopaque->nobtpo_replaced = BufferGetBlockNumber(buf);

    /*
     *  Now a path to the new pages exists (via the replaced link from the
     *  old page).  Concurrent updates in the tree in adjacent pages will
     *  follow this link if they want to update the peer pointers on the
     *  (old) page.  For that reason, we issue a peer lock on the peer pointer
     *  values before we update them.  If they've been updated already, then
     *  we leave them as they are, since the update that changed them knew
     *  what it was doing.
     */

    if (lopaque->nobtpo_prev == P_NONE)
	lopaque->nobtpo_prev = oopaque->nobtpo_prev;

    if (ropaque->nobtpo_next == P_NONE)
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
    if (ropaque->nobtpo_next != P_NONE) {
	start = 1;
	itemid = PageGetItemId(origpage, 0);
	itemsz = ItemIdGetLength(itemid);
	iitem = (NOBTIItem) PageGetItem(origpage, itemid);
	PageAddItem(rightpage, iitem, itemsz, nright++, LP_USED);
    } else {
	start = 0;
    }

    maxoff = PageGetMaxOffsetIndex(origpage);
    llimit = PageGetFreeSpace(leftpage) / 2;
    firstright = _nobt_findsplitloc(rel, origpage, start, maxoff, llimit);

    for (i = start; i <= maxoff; i++) {
	itemid = PageGetItemId(origpage, i);
	itemsz = ItemIdGetLength(itemid);
	iitem = (NOBTIItem) PageGetItem(origpage, itemid);

	/* decide which page to put it on */
	if (i < firstright)
	    PageAddItem(leftpage, iitem, itemsz, nleft++, LP_USED);
	else
	    PageAddItem(rightpage, iitem, itemsz, nright++, LP_USED);
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
    iitem = (NOBTIItem) PageGetItem(rightpage, itemid);

    /*
     *  We left a hole for the high key on the left page; fill it.  The
     *  modal crap is to tell the page manager to put the new item on the
     *  page and not screw around with anything else.  Whoever designed
     *  this interface has presumably crawled back into the dung heap they
     *  came from.  No one here will admit to it.
     */

    PageManagerModeSet(OverwritePageManagerMode);
    PageAddItem(leftpage, iitem, itemsz, 1, LP_USED);
    PageManagerModeSet(ShufflePageManagerMode);

    /*
     *  By here, the original data page has been split into two new halves,
     *  and these are correct.  The original buffer has had its 'replaced'
     *  entry updated as appropriate.  Write these guys out.
     */

    _nobt_wrtnorelbuf(rel, rbuf);
    _nobt_wrtnorelbuf(rel, buf);

    /* can afford to drop the lock on the original and switch to the new */
    _nobt_wrtbuf(rel, *bufP);
    *bufP = buf;

    /*
     *  Finally, we need to adjust the link pointers among siblings.  Since
     *  we've added new pages, we have to adjust the sibling pointers on
     *  either side of the new pages in the tree.
     *
     *  On-demand recovery requires that we detect and handle the case where
     *  our (left or right) sibling in the tree was split, and not all pages
     *  affected by the split made it to disk before a crash.  The major idea
     *  is to defer repairing the page for as long as possible.  To that end,
     *  what we do here is only update the peer pointer of the last page that
     *  made it safely to disk.
     *
     *  In the case that the 'replaced' link is broken, we know that the peer
     *  pointer on the source page was correctly updated, and that the 'old'
     *  value for the peer pointer in the page opaque data must be preserved
     *  until the page is fixed.
     */

    nobackup = false;
    if (ropaque->nobtpo_next != P_NONE) {
	sbuf = _nobt_getbuf(rel, ropaque->nobtpo_next, NOBT_WRITE);
	spage = BufferGetPage(sbuf, 0);
	sopaque = (NOBTPageOpaque) PageGetSpecialPointer(spage);
	while ((repl = sopaque->nobtpo_replaced) != P_NONE) {
	    newsbuf = _nobt_getbuf(rel, repl, NOBT_WRITE);
	    newspage = BufferGetPage(sbuf, 0);
	    newsopaque = (NOBTPageOpaque) PageGetSpecialPointer(newspage);

	    /*
	     *  Determine whether this is a valid link.  If so, traverse
	     *  it.  Otherwise, we've gotten as far as we need to; bust
	     *  out.  Someone later will repair the damage we've just
	     *  detected.  This will happen when an insert or read of a
	     *  datum on the page with the broken link happens.
	     */

	    if (sopaque->nobtpo_repltok != newsopaque->nobtpo_linktok) {

		/*
		 *  Link is broken.  Someone later will fix it.  For now,
		 *  we want to preserve the old peer pointer and link token,
		 *  since they may be needed to do the repair.  Store the
		 *  new peer pointer without backing up the old one.
		 *
		 *  The "break" is for the while (repl = ...) loop.
		 */

		nobackup = true;
		break;
	    } else {

		/*
		 *  Traverse the replaced link and move the lock down.
		 */

		_nobt_relbuf(rel, sbuf, NOBT_WRITE);
		sopaque = newsopaque;

		/* still have newsbuf locked with PREVLNK */
		sbuf = newsbuf;
	    }
	}

	/*
	 *  We need to hold the 'replaced' link lock until we have the
	 *  peer pointers correct.
	 */

	if (!nobackup)
	    sopaque->nobtpo_oldprev = sopaque->nobtpo_prev;

	sopaque->nobtpo_prev = BufferGetBlockNumber(rbuf);
	CRIT_SEC_START;
	ropaque->nobtpo_nexttok = sopaque->nobtpo_prevtok = CurrentLinkToken;
	CRIT_SEC_END;

	/* write and release the old right sibling */
	_nobt_wrtbuf(rel, sbuf);
    }

    nobackup = false;
    if (lopaque->nobtpo_prev != P_NONE) {
	sbuf = _nobt_getbuf(rel, ropaque->nobtpo_next, NOBT_WRITE);
	spage = BufferGetPage(sbuf, 0);
	sopaque = (NOBTPageOpaque) PageGetSpecialPointer(spage);
	while ((repl = sopaque->nobtpo_replaced) != P_NONE) {
	    newsbuf = _nobt_getbuf(rel, repl, NOBT_WRITE);
	    newspage = BufferGetPage(sbuf, 0);
	    newsopaque = (NOBTPageOpaque) PageGetSpecialPointer(newspage);

	    /*
	     *  Determine whether this is a valid link.  If so, traverse
	     *  it.  Otherwise, we've gotten as far as we need to.
	     */

	    if (sopaque->nobtpo_repltok != newsopaque->nobtpo_linktok) {

		/*
		 *  Link is broken.  Someone later will fix it.  For now,
		 *  we want to preserve the old peer pointer and link token,
		 *  since they may be needed to do the repair.  Store the
		 *  new peer pointer without backing up the old one.
		 *
		 *  The "break" is for the while (repl = ...) loop.
		 */

		nobackup = true;
		break;
	    } else {

		/*
		 *  Traverse the replaced link and move the lock down.
		 */

		_nobt_relbuf(rel, sbuf, NOBT_WRITE);
		sopaque = newsopaque;
		sbuf = newsbuf;
	    }
	}

	/*
	 *  By here, we have sopaque->nobtpo_next locked.  Need to hold the
	 *  'replaced' lock (which we also hold) unitl the peer pointers are
	 *  correct.
	 */

	if (!nobackup)
	    sopaque->nobtpo_oldnext = sopaque->nobtpo_next;

	sopaque->nobtpo_next = BufferGetBlockNumber(buf);
	CRIT_SEC_START;
	lopaque->nobtpo_prevtok = sopaque->nobtpo_nexttok = CurrentLinkToken;
	CRIT_SEC_END;

	/* write and release the old right sibling */
	_nobt_wrtbuf(rel, sbuf);
    }

    /* split's done */
    return (rbuf);
}
#endif	/* SHADOW */

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
 *
 *  For the no-overwrite implementation, we assume no duplicates.
 */

OffsetIndex
_nobt_findsplitloc(rel, page, start, maxoff, llimit)
    Relation rel;
    Page page;
    OffsetIndex start;
    OffsetIndex maxoff;
    Size llimit;
{
    OffsetIndex maxoff;
    OffsetIndex splitloc;

    maxoff = PageGetMaxOffsetIndex(page);
    splitloc = maxoff / 2;

    return (splitloc);
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
    BlockNumber lbkno, rbkno;
    Page lpage, rpage, rootpage;
    NOBTPageOpaque lopaque, ropaque;
    BlockNumber rootbknum;
    NOBTPageOpaque rootopaque;
    ItemId itemid;
    NOBTIItem iitem;
    NOBTLItem litem;
    int itemsz;
    NOBTIItem new_item;
    Size dsize;
    char *datump;

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
    lopaque = (NOBTPageOpaque) PageGetSpecialPointer(lpage);
    ropaque = (NOBTPageOpaque) PageGetSpecialPointer(rpage);

    /* step over the high key on the left page */
    itemid = PageGetItemId(lpage, 1);
    itemsz = ItemIdGetLength(itemid);
    if (lopaque->nobtpo_flags & NOBTP_LEAF) {
    	litem = (NOBTLItem) PageGetItem(lpage, itemid);
	datump = (char *) litem;
	datump += sizeof(NOBTLItemData);
	dsize = itemsz - sizeof(IndexTupleData);
    } else {
    	iitem = (NOBTIItem) PageGetItem(lpage, itemid);
	datump = (char *) iitem;
	datump += sizeof(NOBTIItemData);
	dsize = itemsz - sizeof(NOBTIItemData);
    }
    new_item = _nobt_formiitem(lbkno, datump, dsize);

    /* insert the left page pointer */
    PageAddItem(rootpage, new_item, NOBTIItemGetSize(new_item), 1, LP_USED);
    pfree(new_item);

    itemid = PageGetItemId(rpage, 0);
    itemsz = ItemIdGetLength(itemid);
    if (ropaque->nobtpo_flags & NOBTP_LEAF) {
    	litem = (NOBTLItem) PageGetItem(rpage, itemid);
	datump = (char *) litem;
	datump += sizeof(NOBTLItemData);
	dsize = itemsz - sizeof(IndexTupleData);
    } else {
    	iitem = (NOBTIItem) PageGetItem(rpage, itemid);
	datump = (char *) iitem;
	datump += sizeof(NOBTIItemData);
	dsize = itemsz - sizeof(NOBTIItemData);
    }
    new_item = _nobt_formiitem(rbkno, datump, dsize);

    /* insert the right page pointer */
    PageAddItem(rootpage, new_item, NOBTIItemGetSize(new_item), 2, LP_USED);
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
_nobt_pgaddtup(rel, buf, keysz, itup_scankey, itemsize, btvoid, voidafter)
    Relation rel;
    Buffer buf;
    int keysz;
    ScanKey itup_scankey;
    int itemsize;
    char *btvoid;
    char *voidafter;
{
    Page page;
    OffsetIndex itup_off;

    page = BufferGetPage(buf, 0);
    itup_off = _nobt_binsrch(rel, buf, keysz, itup_scankey, NOBT_INSERTION);
    PageAddItem(page, btvoid, itemsize, itup_off + 1, LP_USED);

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
    NOBTIItem afteritem;
{
    Page page;
    NOBTPageOpaque opaque;
    ItemId hikey;

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

    return (false);
}
