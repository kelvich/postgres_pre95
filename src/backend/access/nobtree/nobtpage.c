/*
 *  btpage.c -- BTree-specific page management code for the Postgres btree
 *	        access method.
 *
 *  Postgres btree pages look like ordinary relation pages.  The opaque
 *  data at high addresses includes pointers to left and right siblings
 *  and flag data describing page state.  The first page in a btree, page
 *  zero, is special -- it stores meta-information describing the tree.
 *  Pages one and higher store the actual tree data.
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

#include "access/genam.h"
#include "access/ftup.h"
#include "access/nobtree.h"

RcsId("$Header$");

#define BTREE_METAPAGE	0
#define BTREE_MAGIC	0x053162
#define BTREE_VERSION	0

typedef struct BTMetaPageData {
    uint32	btm_magic;
    uint32	btm_version;
    BlockNumber	btm_root;
    BlockNumber	btm_freelist;
} BTMetaPageData;

/*
 *  _nobt_metapinit() -- Initialize the metadata page of a btree.
 */

_nobt_metapinit(rel)
    Relation rel;
{
    Buffer buf;
    Page pg;
    int nblocks;
    BTMetaPageData metad;

    /* can't be sharing this with anyone, now... */
    RelationSetLockForWrite(rel);

    if ((nblocks = RelationGetNumberOfBlocks(rel)) != 0) {
	elog(WARN, "Cannot initialize non-empty btree %s",
		   rel->rd_rel->relname);
    }

    buf = ReadBuffer(rel, P_NEW);
    pg = BufferGetPage(buf, 0);

    metad.btm_magic = BTREE_MAGIC;
    metad.btm_version = BTREE_VERSION;
    metad.btm_root = P_NONE;
    metad.btm_freelist = P_NONE;

    bcopy((char *) &metad, (char *) pg, sizeof(metad));

    WriteBuffer(buf);

    /* all done */
    RelationUnsetLockForWrite(rel);
}

/*
 *  _nobt_checkmeta() -- Verify that the metadata stored in a btree are
 *		       reasonable.
 */

_nobt_checkmeta(rel)
    Relation rel;
{
    Buffer metabuf;
    BTMetaPageData *metad;
    int nblocks;

    /* if the relation is empty, this is init time; don't complain */
    if ((nblocks = RelationGetNumberOfBlocks(rel)) == 0)
	return;

    metabuf = _nobt_getbuf(rel, BTREE_METAPAGE, NOBT_READ);
    metad = (BTMetaPageData *) BufferGetPage(metabuf, 0);

    if (metad->btm_magic != BTREE_MAGIC) {
	elog(WARN, "What are you trying to pull?  %s is not a btree",
		   rel->rd_rel->relname);
    }

    if (metad->btm_version != BTREE_VERSION) {
	elog(WARN, "Version mismatch on %s:  version %d file, version %d code",
		   rel->rd_rel->relname, metad->btm_version, BTREE_VERSION);
    }

    _nobt_relbuf(rel, metabuf, NOBT_READ);
}

/*
 *  _nobt_getroot() -- Get the root page of the btree.
 *
 *	Since the root page can move around the btree file, we have to read
 *	its location from the metadata page, and then read the root page
 *	itself.  If no root page exists yet, we have to create one.  The
 *	standard class of race conditions exists here; I think I covered
 *	them all in the Hopi Indian rain dance of lock requests below.
 *
 *	We pass in the access type (NOBT_READ or NOBT_WRITE), and return the
 *	root page's buffer with the appropriate lock type set.  Reference
 *	count on the root page gets bumped by ReadBuffer.  The metadata
 *	page is unlocked and unreferenced by this process when this routine
 *	returns.
 */

Buffer
_nobt_getroot(rel, access)
    Relation rel;
    int access;
{
    Buffer metabuf;
    Buffer rootbuf;
    Page rootpg;
    NOBTPageOpaque rootopaque;
    BlockNumber rootblkno;
    BTMetaPageData *metad;

    metabuf = _nobt_getbuf(rel, BTREE_METAPAGE, NOBT_READ);
    metad = (BTMetaPageData *) BufferGetPage(metabuf, 0);

    /* if no root page initialized yet, do it */
    if (metad->btm_root == P_NONE) {

	/* turn our read lock in for a write lock */
	_nobt_relbuf(rel, metabuf, NOBT_READ);
	metabuf = _nobt_getbuf(rel, BTREE_METAPAGE, NOBT_WRITE);
	metad = (BTMetaPageData *) BufferGetPage(metabuf, 0);

	/*
	 *  Race condition:  if someone else initialized the metadata between
	 *  the time we released the read lock and acquired the write lock,
	 *  above, we want to avoid doing it again.
	 */

	if (metad->btm_root == P_NONE) {

	    /*
	     *  Get, initialize, write, and leave a lock of the appropriate
	     *  type on the new root page.  Since this is the first page in
	     *  the tree, it's a leaf.
	     */

	    rootbuf = _nobt_getbuf(rel, P_NEW, NOBT_WRITE);
	    rootblkno = BufferGetBlockNumber(rootbuf);
	    rootpg = BufferGetPage(rootbuf, 0);
	    metad->btm_root = rootblkno;
	    _nobt_pageinit(rootpg);
	    rootopaque = (NOBTPageOpaque) PageGetSpecialPointer(rootpg);
	    rootopaque->nobtpo_flags |= (NOBTP_LEAF | NOBTP_ROOT);
	    _nobt_wrtnorelbuf(rel, rootbuf);

	    /* swap write lock for read lock, if appropriate */
	    if (access != NOBT_WRITE) {
		_nobt_setpagelock(rel, rootblkno, NOBT_READ);
		_nobt_unsetpagelock(rel, rootblkno, NOBT_WRITE);
	    }

	    /* okay, metadata is correct */
	    _nobt_wrtbuf(rel, metabuf);
	} else {

	    /*
	     *  Metadata initialized by someone else.  In order to guarantee
	     *  no deadlocks, we have to release the metadata page and start
	     *  all over again.
	     */

	    _nobt_relbuf(rel, metabuf, NOBT_WRITE);
	    return (_nobt_getroot(rel));
	}
    } else {
	rootbuf = _nobt_getbuf(rel, metad->btm_root, access);

	/* done with the meta page */
	_nobt_relbuf(rel, metabuf, NOBT_READ);
    }

    /*
     *  Race condition:  If the root page split between the time we looked
     *  at the metadata page and got the root buffer, then we got the wrong
     *  buffer.
     */

    rootpg = BufferGetPage(rootbuf, 0);
    rootopaque = (NOBTPageOpaque) PageGetSpecialPointer(rootpg);

#ifdef	SHADOW
    if (!(rootopaque->nobtpo_flags & NOBTP_ROOT)
	|| (rootopaque->nobtpo_replaced != P_NONE)) {
#endif	/* SHADOW */
#ifdef	NORMAL
    if (!(rootopaque->nobtpo_flags & NOBTP_ROOT)) {
#endif	/* NORMAL */
#ifdef	REORG
    if (!(rootopaque->nobtpo_flags & NOBTP_ROOT)) {
#endif	/* REORG */

	/* it happened, try again */
	_nobt_relbuf(rel, rootbuf, access);
	return (_nobt_getroot(rel));
    }

    /*
     *  By here, we have a correct lock on the root block, its reference
     *  count is correct, and we have no lock set on the metadata page.
     *  Return the root block.
     */

    return (rootbuf);
}

/*
 *  _nobt_getbuf() -- Get a buffer by block number for read or write.
 *
 *	When this routine returns, the appropriate lock is set on the
 *	requested buffer its reference count is correct.
 */

Buffer
_nobt_getbuf(rel, blkno, access)
    Relation rel;
    BlockNumber blkno;
    int access;
{
    Buffer buf;
    Page page;

    /*
     *  If we want a new block, we can't set a lock of the appropriate type
     *  until we've instantiated the buffer.
     */

    if (blkno != P_NEW) {
	_nobt_setpagelock(rel, blkno, access);
	buf = ReadBuffer(rel, blkno);
    } else {
	buf = ReadBuffer(rel, blkno);
	blkno = BufferGetBlockNumber(buf);
	page = BufferGetPage(buf, 0);
	_nobt_pageinit(page);
	_nobt_setpagelock(rel, blkno, access);
    }

    /* ref count and lock type are correct */
    return (buf);
}

/*
 *  _nobt_relbuf() -- release a locked buffer.
 */

void
_nobt_relbuf(rel, buf, access)
    Relation rel;
    Buffer buf;
    int access;
{
    BlockNumber blkno;

    blkno = BufferGetBlockNumber(buf);

    /* access had better be one of read or write */
    if (access == NOBT_WRITE)
	_nobt_unsetpagelock(rel, blkno, NOBT_WRITE);
    else
	_nobt_unsetpagelock(rel, blkno, NOBT_READ);

    ReleaseBuffer(buf);
}

/*
 *  _nobt_wrtbuf() -- write a btree page to disk.
 *
 *	This routine releases the lock held on the buffer and our reference
 *	to it.  It is an error to call _nobt_wrtbuf() without a write lock
 *	or a reference to the buffer.
 */

void
_nobt_wrtbuf(rel, buf)
    Relation rel;
    Buffer buf;
{
    BlockNumber blkno;

    blkno = BufferGetBlockNumber(buf);
    WriteBuffer(buf);
    _nobt_unsetpagelock(rel, blkno, NOBT_WRITE);
}

/*
 *  _nobt_wrtnorelbuf() -- write a btree page to disk, but do not release
 *			 our reference or lock.
 *
 *	It is an error to call _nobt_wrtnorelbuf() without a write lock
 *	or a reference to the buffer.
 */

void
_nobt_wrtnorelbuf(rel, buf)
    Relation rel;
    Buffer buf;
{
    BlockNumber blkno;

    blkno = BufferGetBlockNumber(buf);
    WriteNoReleaseBuffer(buf);
}

/*
 *  _nobt_pageinit() -- Initialize a new page.
 */

_nobt_pageinit(page)
    Page page;
{
    /*
     *  Cargo-cult programming -- don't really need this to be zero, but
     *  creating new pages is an infrequent occurrence and it makes me feel
     *  good when I know they're empty.
     */

    bzero(page, BLCKSZ);

    PageInit(page, BLCKSZ, sizeof(NOBTPageOpaqueData));
}

/*
 *  _nobt_metaproot() -- Change the root page of the btree.
 *
 *	Lehman and Yao require that the root page move around in order to
 *	guarantee deadlock-free short-term, fine-granularity locking.  When
 *	we split the root page, we record the new parent in the metadata page
 *	for the relation.  This routine does the work.
 *
 *	No direct preconditions, but if you don't have the a write lock on
 *	at least the old root page when you call this, you're making a big
 *	mistake.  On exit, metapage data is correct and we no longer have
 *	a reference to or lock on the metapage.
 */

_nobt_metaproot(rel, rootbknum)
    Relation rel;
    BlockNumber rootbknum;
{
    Buffer metabuf;
    BTMetaPageData *metap;

    uint32	btm_magic;
    uint32	btm_version;
    BlockNumber	btm_root;
    BlockNumber	btm_freelist;

    metabuf = _nobt_getbuf(rel, BTREE_METAPAGE, NOBT_WRITE);
    metap = (BTMetaPageData *) BufferGetPage(metabuf, 0);
    metap->btm_root = rootbknum;
    _nobt_wrtbuf(rel, metabuf);
}

/*
 *  _nobt_getstackbuf() -- Walk back up the tree one step, and find the item
 *			 we last looked at in the parent.
 *
 *	This is possible because we save a bit image of the last item
 *	we looked at in the parent, and the update algorithm guarantees
 *	that if items above us in the tree move, they only move right.
 */

Buffer
_nobt_getstackbuf(rel, stack, access)
    Relation rel;
    NOBTStack stack;
    int access;
{
    Buffer buf;
    BlockNumber blkno;
    OffsetIndex start, offind, maxoff;
    OffsetIndex i;
    Page page;
    ItemId itemid;
    int nbytes;
    NOBTIItem iitem;
    NOBTPageOpaque opaque;

    blkno = stack->nobts_blkno;
    buf = _nobt_getbuf(rel, blkno, access);
    page = BufferGetPage(buf, 0);
    opaque = (NOBTPageOpaque) PageGetSpecialPointer(page);
    maxoff = PageGetMaxOffsetIndex(page);

    nbytes = sizeof(NOBTIItemData);

    if (maxoff >= stack->nobts_offset) {
	itemid = PageGetItemId(page, stack->nobts_offset);
	iitem = (NOBTIItem) PageGetItem(page, itemid);

	/* if the item is where we left it, we're done */
	if (bcmp((char *) iitem, (char *) (stack->nobts_btitem), nbytes) == 0)
	    return (buf);

	/* if the item has just moved right on this page, we're done */
	for (i = stack->nobts_offset + 1; i <= maxoff; i++) {
	    itemid = PageGetItemId(page, stack->nobts_offset);
	    iitem = (NOBTIItem) PageGetItem(page, itemid);

	    /* if the item is where we left it, we're done */
	    if (bcmp((char *) iitem, (char *) (stack->nobts_btitem), nbytes) == 0)
		return (buf);
	}
    }

    /* by here, the item we're looking for moved right at least one page */
    for (;;) {
	if ((blkno = opaque->nobtpo_next) == P_NONE)
	    elog(FATAL, "my bits moved right off the end of the world!");

	_nobt_relbuf(rel, buf, access);
	buf = _nobt_getbuf(rel, blkno, access);
	page = BufferGetPage(buf, 0);
	maxoff = PageGetMaxOffsetIndex(page);
	opaque = (NOBTPageOpaque) PageGetSpecialPointer(page);

	/* if we have a right sibling, step over the high key */
	if (opaque->nobtpo_next != P_NONE)
	    start = 1;
	else
	    start = 0;

	/* see if it's on this page */
	for (offind = start; offind <= maxoff; offind++) {
	    itemid = PageGetItemId(page, offind);
	    iitem = (NOBTIItem) PageGetItem(itemid);
	    if (bcmp((char *) iitem, (char *) (stack->nobts_btitem), nbytes) == 0)
		return (buf);
	}
    }
}

_nobt_setpagelock(rel, blkno, access)
    Relation rel;
    BlockNumber blkno;
    int access;
{
    ItemPointerData iptr;

    ItemPointerSet(&iptr, 0, blkno, 0, 1);

    switch (access) {

      case NOBT_WRITE:
	RelationSetLockForWritePage(rel, 0, &iptr);
	break;

      case NOBT_READ:
	RelationSetLockForReadPage(rel, 0, &iptr);
	break;

      default:
	break;
    }
}

_nobt_unsetpagelock(rel, blkno, access)
    Relation rel;
    BlockNumber blkno;
    int access;
{
    ItemPointerData iptr;

    ItemPointerSet(&iptr, 0, blkno, 0, 1);

    switch (access) {

      case NOBT_WRITE:
	RelationUnsetLockForWritePage(rel, 0, &iptr);
	break;

      case NOBT_READ:
	RelationUnsetLockForReadPage(rel, 0, &iptr);
	break;

      default:
	break;
    }
}

void
_nobt_pagedel(rel, tid)
    Relation rel;
    ItemPointer tid;
{
    Buffer buf;
    Page page;
    BlockNumber blkno;
    OffsetIndex offno;

    blkno = ItemPointerGetBlockNumber(tid);
    offno = ItemPointerGetOffsetNumber(tid, 0);

    buf = _nobt_getbuf(rel, blkno, NOBT_WRITE);
    page = BufferGetPage(buf, 0);

    PageIndexTupleDelete(page, offno);

    /* write the buffer and release the lock */
    _nobt_wrtbuf(rel, buf);
}
#endif /* NOBTREE */
