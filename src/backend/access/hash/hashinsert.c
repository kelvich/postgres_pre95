/*
 *  Hashinsert.c -- Item insertion in hash tables for Postgres.
 */

#include "tmp/c.h"
#include "tmp/postgres.h"

#include "storage/bufmgr.h"
#include "storage/bufpage.h"
#include "storage/page.h"

#include "utils/log.h"
#include "utils/palloc.h"
#include "utils/rel.h"
#include "utils/excid.h"

#include "access/heapam.h"
#include "access/genam.h"
#include "access/ftup.h"
#include "access/hash.h"

RcsId("$Header$");

/*
 *  _hash_doinsert() -- Handle insertion of a single HashItem in the table.
 *
 *	This routine is called by the public interface routines, hashbuild
 *	and hashinsert.  By here, hashitem is filled in, and has a unique
 *	(xid, seqno) pair. The datum to be used as a "key" is in the
 * 	hashitem. 
 */

InsertIndexResult
_hash_doinsert(rel, hitem)
    Relation rel;
    HashItem hitem;
{
    Bucket bucket;
    Buffer buf;
    Buffer metabuf;
    BlockNumber blkno;
    HashMetaPage metap;
    IndexTuple itup;
    InsertIndexResult res;
    OffsetIndex itup_off;
    ScanKey itup_scankey;
    int natts;
    itup = &(hitem->hash_itup);

    metabuf = _hash_getbuf(rel, HASH_METAPAGE, HASH_READ);
    metap = (HashMetaPage) BufferGetPage(metabuf, 0);

    /* we need a scan key to do our search, so build one */
    itup_scankey = _hash_mkscankey(rel, itup, metap);
    if ((natts = rel->rd_rel->relnatts) != 1)
	elog(WARN, "Hash indices valid for only one index key.");

    /* 
     * Find the first page in the bucket chain containing this key
     * and place it in buf. 
     */
    _hash_search(rel, natts, itup_scankey, &buf, metap);

    blkno = BufferGetBlockNumber(buf);

    /* trade in our read lock for a write lock */
    _hash_relbuf(rel, buf, HASH_READ);
    buf = _hash_getbuf(rel, blkno, HASH_WRITE);


    /*  XXX btree comment (haven't decided what to do in hash): 
     *  don't think the bucket can be split while we're reading the 
     *  metapage. 
     *
     *  If the page was split between the time that we surrendered our
     *  read lock and acquired our write lock, then this page may no
     *  longer be the right place for the key we want to insert. 
     */

    /* do the insertion */
    res = _hash_insertonpg(rel, buf, natts, itup_scankey,
			 hitem, metabuf);

    /* be tidy */
    _hash_freeskey(itup_scankey);

    return (res);
}

/*
 *  _hash_insertonpg() -- Insert a tuple on a particular page in the table.
 *
 *	This recursive procedure does the following things:
 *
 *	    +  if necessary, splits the target page.  
 *	    +  inserts the tuple.
 *
 *	On entry, we must have the right buffer on which to do the
 *	insertion, and the buffer must be pinned and locked.  On return,
 *	we will have dropped both the pin and the write lock on the buffer.
 *
 */

InsertIndexResult
_hash_insertonpg(rel, buf, keysz, scankey, hitem, metabuf)
    Relation rel;
    Buffer buf;
    int keysz;
    ScanKey scankey;
    HashItem hitem;
    Buffer metabuf;
{
    InsertIndexResult res; 
    Page page, ovflpage;
    BlockNumber itup_blkno;
    BlockNumber ovflblkno;
    BlockNumber blkno;
    OffsetIndex itup_off;
    int itemsz;
    HashPageOpaque pageopaque;
    bool do_expand = false;	 
    Buffer newbuf;
    Buffer ovflbuf;
    Buffer tmpbuf;
    HashMetaPage metap;
    OverflowPageAddress oaddr;


    metap = (HashMetaPage) BufferGetPage(metabuf, 0);

    page = BufferGetPage(buf, 0);

    itemsz = IndexTupleDSize(hitem->hash_itup)
	   + (sizeof(HashItemData) - sizeof(IndexTupleData));

    while (PageGetFreeSpace(page) < itemsz) {

	/* 
         * no space on this page; check for an overflow page 
	 */
	pageopaque = (HashPageOpaque) PageGetSpecialPointer(page);
	if (BlockNumberIsValid(pageopaque->hasho_nextblkno)) {
	    /* 
	     * ovfl page exists; go get it 
	     */

	    tmpbuf = _hash_getbuf(rel, pageopaque->hasho_nextblkno, HASH_WRITE);
	    _hash_relbuf(rel, buf, HASH_WRITE);
	    buf = tmpbuf;
	    page = BufferGetPage(buf, 0);

	} else {

	    /* 
	     * allocate an ovfl page and exit the loop
	     */

	    do_expand = true;
	    ovflbuf = _hash_addovflpage(rel, &metabuf, buf);
	    _hash_relbuf(rel, buf, HASH_WRITE);
	    buf = ovflbuf;
	    page = BufferGetPage(buf, 0);
	    if (PageGetFreeSpace(page) < itemsz) {
		elog(WARN, "Hash Item too large. Large objects not implemented for hash.");
		buf = _hash_freeovflpage(buf);
		_hash_relbuf(rel, buf, HASH_WRITE);
	    break;
	    }
	}
    }

    /* 
     * Below, _hash_pgaddtup returns an offset index number - something
     * which starts at one. For example, if this were the first tuple
     * to be inserted onto the page, _hash_pgaddtup would return 1.
     */ 

    itup_off = _hash_pgaddtup(rel, buf, keysz, scankey, itemsz, hitem);
    itup_blkno = BufferGetBlockNumber(buf);

    /* by here, the new tuple is inserted */
    res = (InsertIndexResult) palloc(sizeof(InsertIndexResultData));

    /* Below, itup_off is an offset number which starts at 1 */
    ItemPointerSet(&(res->pointerData), 0, itup_blkno, 0, itup_off);
    res->lock = (RuleLock) NULL;
    res->offset = (double) 0;

    if (InsertIndexResultIsValid(res)) {

	/* 
	 * Increment the number of keys in the table.
	 * We switch lock access type just for a moment
	 * to allow greater accessibility to the metapage. 
	 */

	metap = (HashMetaPage) _hash_chgbufaccess(rel, &metabuf, HASH_READ, HASH_WRITE);
	metap->hashm_nkeys += 1;
	metap = (HashMetaPage) _hash_chgbufaccess(rel, &metabuf, HASH_WRITE, HASH_READ);

    }

    _hash_wrtbuf(rel, buf, HASH_WRITE);

    if (do_expand || 
	(metap->hashm_nkeys / (metap->hashm_maxbucket + 1))
	    > metap->hashm_ffactor) {
	_hash_expandtable(rel, metabuf);
    }
    _hash_relbuf(rel, metabuf, HASH_READ);
    return (res);
}	

/*
 *  _hash_pgaddtup() -- add a tuple to a particular page in the index.
 *
 *	This routine adds the tuple to the page as requested, and keeps the
 *	write lock and reference associated with the page's buffer.  It is
 *	an error to call pgaddtup() without a write lock and reference.
 */

OffsetIndex
_hash_pgaddtup(rel, buf, keysz, itup_scankey, itemsize, hitem)
    Relation rel;
    Buffer buf;
    int keysz;
    ScanKey itup_scankey;
    Size itemsize;
    HashItem hitem;
{
    OffsetIndex itup_off;
    OffsetIndex maxoff;
    OffsetIndex first;
    ItemId itemid;
    HashItem olditem;
    Page page;
    HashPageOpaque opaque;
    HashItem chkitem;
    OID afteroid;

    page = BufferGetPage(buf, 0);

    /* 
     * Itup_off is an offset *number* that starts at 1, but 
     * PageGetMaxOffsetIndex is an offset *index* that starts at 0.
     * (To add further confusion, PageGetMaxOffsetIndex will 
     * return 65536 for an empty page). Below, we add 2 - one
     * to move forward to something that starts at 1, and one
     * to get the next free offset number. 
     */

    itup_off = PageGetMaxOffsetIndex(page) + 2;
    PageAddItem(page, (Item) hitem, itemsize, itup_off, LP_USED);

    /* write the buffer, but hold our lock */
    _hash_wrtnorelbuf(rel, buf);

    return (itup_off);
}










