/*
 *  Hashpage.c -- Hash table page management code for the Postgres hash
 *	        access method.
 *
 *  Postgres hash pages look like ordinary relation pages.  The opaque
 *  data at high addresses includes information about the page including
 *  whether a page is an overflow page or a true bucket, the block 
 *  numbers of the preceding and following pages, and the overflow
 *  address of the page if it is an overflow page.
 *
 *  The first page in a hash relation, page zero, is special -- it stores
 *  information describing the hash table; it is referred to as teh
 *  "meta page." Pages one and higher store the actual data. 
 */

#include "tmp/c.h"
#include "tmp/postgres.h"

#include "storage/bufmgr.h"
#include "storage/bufpage.h"
#include "storage/page.h"

#include "utils/log.h"
#include "utils/rel.h"
#include "utils/excid.h"

#include "access/genam.h"
#include "access/ftup.h"
#include "access/hash.h"

RcsId("$Header$");

/*  
 *  We use high-concurrency locking on hash indices.  There are two cases in
 *  which we don't do locking.  One is when we're building the index.
 *  Since the creating transaction has not committed, no one can see
 *  the index, and there's no reason to share locks.  The second case
 *  is when we're just starting up the database system.  We use some
 *  special-purpose initialization code in the relation cache manager
 *  (see utils/cache/relcache.c) to allow us to do indexed scans on
 *  the system catalogs before we'd normally be able to.  This happens
 *  before the lock table is fully initialized, so we can't use it.
 *  Strictly speaking, this violates 2pl, but we don't do 2pl on the
 *  system catalogs anyway.
 */


#define USELOCKING	(!BuildingHash && !IsInitProcessingMode())


/*
 *  _hash_metapinit() -- Initialize the metadata page of a hash index
 *  		and the two buckets that we begin with.
 */

_hash_metapinit(rel)
    Relation rel;
{
    HashMetaPageData metad;
    HashMetaPage metap;
    HashPageOpaque pageopaque;
    Buffer metabuf;
    Buffer buf;
    Page pg;
    int nbuckets;
    uint32 nelem;			/* number elements */
    uint32 lg2nelem;			/* _hash_log2(nelem)   */
    uint32 nblocks;
    uint16 i;

    /* can't be sharing this with anyone, now... */
    if (USELOCKING)
	RelationSetLockForWrite(rel);

    if ((nblocks = RelationGetNumberOfBlocks(rel)) != 0) {
	elog(WARN, "Cannot initialize non-empty hash table %s",
		   RelationGetRelationName(rel));
    }

    metabuf = _hash_getbuf(rel, HASH_METAPAGE, HASH_WRITE);
    metap = (HashMetaPage) BufferGetPage(metabuf, 0);
  
    metad.hashm_magic 		= HASH_MAGIC;
    metad.hashm_version 	= HASH_VERSION;
    metad.hashm_nkeys 		= 0;
    metad.hashm_nmaps 		= 0;
    metad.hashm_ffactor 	= DEFAULT_FFACTOR;
    metad.hashm_bsize 		= DEFAULT_BUCKET_SIZE;
    metad.hashm_bshift 		= DEFAULT_BUCKET_SHIFT;
    metad.hashm_procid		= index_getprocid(rel, 1, HASHPROC);

    /* 
     * Make nelem = 2 rather than 0 so that we end up allocating space 
     * for the next greater power of two number of buckets. 
     */
    nelem = 2;
    lg2nelem = 1; 	 	/*_hash_log2(MAX(nelem, 2)) */
    nbuckets = 2; 		/*1 << lg2nelem */

    bzero(metad.hashm_spares, NCACHED * sizeof(uint32));
    bzero(metad.hashm_bitmaps, NCACHED * sizeof(bits16));
    bzero(metad.hashm_mapp, NCACHED * sizeof(BlockNumber));

    metad.hashm_spares[lg2nelem]     = 2;	/* lg2nelem + 1 */
    metad.hashm_spares[lg2nelem + 1] = 2;	/* lg2nelem + 1 */
    metad.hashm_ovflpoint            = 1;	/* lg2nelem */
    metad.hashm_lastfreed            = 2;

    metad.hashm_maxbucket = metad.hashm_lowmask = 1; 	/* nbuckets - 1 */
    metad.hashm_highmask  = 3;			 /* (nbuckets << 1) - 1 */

    bcopy((char *) &metad, (char *) metap, sizeof(metad));

    /* 
     * First bitmap page is at: splitpoint lg2nelem page offset 1 which
     * turns out to be page 3. Couldn't initialize page 3  until we created
     * the first two buckets above. 
     */
    if (_hash_initbitmap(rel, metabuf, OADDR_OF(lg2nelem, 1), lg2nelem + 1, 0))
	elog(WARN, "Problem with _hash_initbitmap.");
    
    /* all done */
    _hash_wrtnorelbuf(rel, metabuf);

    /* 
     * initialize the first two buckets 
     */

    for (i = 0; i <= 1; i++) {

	buf = _hash_getbuf(rel, BUCKET_TO_BLKNO(i), HASH_WRITE);
	pg = BufferGetPage(buf, 0);
	_hash_pageinit(pg);
	pageopaque = (HashPageOpaque) PageGetSpecialPointer(pg);
	pageopaque->hasho_oaddr = InvalidOvflAddress;
	pageopaque->hasho_prevblkno = InvalidBlockNumber;
	pageopaque->hasho_nextblkno = InvalidBlockNumber;
	pageopaque->hasho_flag |= BUCKET_PAGE;
	pageopaque->hasho_bucket = i;
	_hash_wrtbuf(rel, buf);
    }

    _hash_relbuf(rel, metabuf, HASH_WRITE);

    if (USELOCKING)
	RelationUnsetLockForWrite(rel);
}

/*
 *  _hash_checkmeta() -- Verify that the metadata stored in hash table are
 *		       reasonable.
 */
_hash_checkmeta(rel)
    Relation rel;
{
    Buffer metabuf;
    HashMetaPage metap;
    int nblocks;

    /* if the relation is empty, this is init time; don't complain */
    if ((nblocks = RelationGetNumberOfBlocks(rel)) == 0)
	return;

    metabuf = _hash_getbuf(rel, HASH_METAPAGE, HASH_READ);
    metap = (HashMetaPage) BufferGetPage(metabuf, 0);

    if (metap->hashm_magic != HASH_MAGIC) {
	elog(WARN, "What are you trying to pull?  %s is not a hash table",
		   RelationGetRelationName(rel));
    }

    if (metap->hashm_version != HASH_VERSION) {
	elog(WARN, "Version mismatch on %s:  version %d file, version %d code",
		   RelationGetRelationName(rel),
		   metap->hashm_version, HASH_VERSION);
    }

    _hash_relbuf(rel, metabuf, HASH_READ);
}

/*
 *  _hash_getbuf() -- Get a buffer by block number for read or write.
 *
 *	When this routine returns, the appropriate lock is set on the
 *	requested buffer its reference count is correct.
 */

Buffer
_hash_getbuf(rel, blkno, access)
    Relation rel;
    BlockNumber blkno;
    int access;
{
    Buffer buf;
    Page page;

    /*
     *  If we want a new block, we can't set a lock of the appropriate type
     *  until we've instantiated the buffer because we won't know the
     *  blocknumber of the page until then. 
     */

    if (blkno != P_NEW) {
	if (access == HASH_WRITE)
	    _hash_setpagelock(rel, blkno, HASH_WRITE);
	else
	    _hash_setpagelock(rel, blkno, HASH_READ);

	buf = ReadBuffer(rel, blkno);
    } else {
	buf = ReadBuffer(rel, blkno);
	blkno = BufferGetBlockNumber(buf);
	page = BufferGetPage(buf, 0);
	_hash_pageinit(page);

	if (access == HASH_WRITE)
	    _hash_setpagelock(rel, blkno, HASH_WRITE);
	else
	    _hash_setpagelock(rel, blkno, HASH_READ);
    }

    /* ref count and lock type are correct */
    return (buf);
}

/*
 *  _hash_relbuf() -- release a locked buffer.
 */

void
_hash_relbuf(rel, buf, access)
    Relation rel;
    Buffer buf;
    int access;
{
    BlockNumber blkno;

    blkno = BufferGetBlockNumber(buf);

    /* access had better be one of read or write */
    if (access == HASH_WRITE)
	_hash_unsetpagelock(rel, blkno, HASH_WRITE);
    else
	_hash_unsetpagelock(rel, blkno, HASH_READ);

    ReleaseBuffer(buf);
}

/*
 *  _hash_wrtbuf() -- write a hash page to disk.
 *
 *	This routine releases the lock held on the buffer and our reference
 *	to it.  It is an error to call _hash_wrtbuf() without a write lock
 *	or a reference to the buffer.
 */

void
_hash_wrtbuf(rel, buf)
    Relation rel;
    Buffer buf;
{
    BlockNumber blkno;

    blkno = BufferGetBlockNumber(buf);
    WriteBuffer(buf);
    _hash_unsetpagelock(rel, blkno, HASH_WRITE);
}
 
/*
 *  _hash_wrtnorelbuf() -- write a hash page to disk, but do not release
 *			 our reference or lock.
 *
 *	It is an error to call _hash_wrtnorelbuf() without a write lock
 *	or a reference to the buffer.
 */

void
_hash_wrtnorelbuf(rel, buf)
    Relation rel;
    Buffer buf;
{
    BlockNumber blkno;

    blkno = BufferGetBlockNumber(buf);
    WriteNoReleaseBuffer(buf);
}

Page
_hash_chgbufaccess(rel, bufp, from_access, to_access)
    Relation rel;
    Buffer *bufp;
    int from_access;
    int to_access;
{
    BlockNumber blkno;

    blkno = BufferGetBlockNumber(*bufp);

    if (from_access == HASH_WRITE)
	_hash_wrtbuf(rel, *bufp);
    else
	_hash_relbuf(rel, *bufp, from_access);

    *bufp = _hash_getbuf(rel, blkno, to_access);
    return (BufferGetPage(*bufp, 0));
}

/*
 *  _hash_pageinit() -- Initialize a new page.
 */

_hash_pageinit(page)
    Page page;
{
    /*
     *  Cargo-cult programming -- don't really need this to be zero, but
     *  creating new pages is an infrequent occurrence and it makes me feel
     *  good when I know they're empty.
     */

    bzero(page, BLCKSZ);

    PageInit(page, BLCKSZ, sizeof(HashPageOpaqueData));
}

_hash_setpagelock(rel, blkno, access)
    Relation rel;
    BlockNumber blkno;
    int access;
{
    ItemPointerData iptr;

    if (USELOCKING) {
	ItemPointerSet(&iptr, 0, blkno, 0, 1);

	if (access == HASH_WRITE)
	    RelationSetSingleWLockPage(rel, 0, &iptr);
	else
	    RelationSetSingleRLockPage(rel, 0, &iptr);
    }
}

_hash_unsetpagelock(rel, blkno, access)
    Relation rel;
    BlockNumber blkno;
    int access;
{
    ItemPointerData iptr;

    if (USELOCKING) {
	ItemPointerSet(&iptr, 0, blkno, 0, 1);

	if (access == HASH_WRITE)
	    RelationUnsetSingleWLockPage(rel, 0, &iptr);
	else
	    RelationUnsetSingleRLockPage(rel, 0, &iptr);
    }
}

void
_hash_pagedel(rel, tid)
    Relation rel;
    ItemPointer tid;
{
    Buffer buf;
    Buffer metabuf;
    Page page;
    BlockNumber blkno;
    OffsetIndex offno;
    OffsetIndex maxoff;
    HashMetaPage   metap;
    HashPageOpaque opaque;
    

    blkno = ItemPointerGetBlockNumber(tid);
    offno = ItemPointerGetOffsetNumber(tid, 0);

    buf = _hash_getbuf(rel, blkno, HASH_WRITE);
    page = BufferGetPage(buf, 0);
    opaque = (HashPageOpaque) PageGetSpecialPointer(page);

    PageIndexTupleDelete(page, offno);
    
    if ((opaque->hasho_flag & OVERFLOW_PAGE) && PageIsEmpty(page))
	    buf = _hash_freeovflpage(rel, buf);
	    
    /* write the buffer and release the lock */
    _hash_wrtbuf(rel, buf);

    metabuf = _hash_getbuf(rel, HASH_METAPAGE, HASH_WRITE);
    metap = (HashMetaPage) BufferGetPage(metabuf, 0);
    ++metap->hashm_nkeys;
    _hash_wrtbuf(rel, metabuf);
}



void
_hash_expandtable(rel, metabuf)
    Relation rel;
    Buffer metabuf;
{
    HashMetaPage metap;
    Bucket old_bucket;
    Bucket new_bucket;
    int32 new_segnum;
    int32 spare_ndx;

    metap = (HashMetaPage) BufferGetPage(metabuf, 0);

    metap = (HashMetaPage) _hash_chgbufaccess(rel, &metabuf, HASH_READ, HASH_WRITE);	
    new_bucket = ++metap->MAX_BUCKET;
    metap = (HashMetaPage) _hash_chgbufaccess(rel, &metabuf, HASH_WRITE, HASH_READ);	
    old_bucket = (metap->MAX_BUCKET & metap->LOW_MASK);
    
    /*
     * If the split point is increasing (MAX_BUCKET's log base 2
     * * increases), we need to copy the current contents of the spare
     * split bucket to the next bucket.
     */
    spare_ndx = _hash_log2(metap->MAX_BUCKET + 1);
    if (spare_ndx > metap->OVFL_POINT) {

	metap = (HashMetaPage) _hash_chgbufaccess(rel, &metabuf, HASH_READ, HASH_WRITE);	
	metap->SPARES[spare_ndx] = metap->SPARES[metap->OVFL_POINT];
	metap->OVFL_POINT = spare_ndx;
	metap = (HashMetaPage) _hash_chgbufaccess(rel, &metabuf, HASH_WRITE, HASH_READ);	
    }
    
    if (new_bucket > metap->HIGH_MASK) {

	/* Starting a new doubling */
	metap = (HashMetaPage) _hash_chgbufaccess(rel, &metabuf, HASH_READ, HASH_WRITE);	
	metap->LOW_MASK = metap->HIGH_MASK;
	metap->HIGH_MASK = new_bucket | metap->LOW_MASK;
	metap = (HashMetaPage) _hash_chgbufaccess(rel, &metabuf, HASH_WRITE, HASH_READ);	

    }
    /* Relocate records to the new bucket */
    _hash_splitpage(rel, metabuf, old_bucket, new_bucket);
}


void
_hash_splitpage(rel, metabuf, obucket, nbucket)
    Relation rel;
    Buffer metabuf;
    Bucket obucket; 
    Bucket nbucket;
{
    Bucket bucket;
    Buffer obuf;
    Buffer nbuf;
    Buffer ovflbuf;
    BlockNumber oblkno;
    BlockNumber nblkno;
    Boolean null;
    Datum datum;
    HashItem hitem;
    HashPageOpaque oopaque;
    HashPageOpaque nopaque;
    HashMetaPage metap;
    IndexTuple itup;
    ItemPointer itemptr;
    int itemsz;
    OffsetIndex ooffind;
    OffsetIndex noffind;
    OffsetIndex omaxoffind;
    OverflowPageAddress oaddr;
    Page opage;
    Page npage;
    TupleDescriptor itupdesc;

    metap = (HashMetaPage) BufferGetPage(metabuf, 0);
    
    /* get the buffers & pages */
    oblkno = BUCKET_TO_BLKNO(obucket);
    nblkno = BUCKET_TO_BLKNO(nbucket);
    obuf = _hash_getbuf(rel, oblkno, HASH_WRITE);
    nbuf = _hash_getbuf(rel, nblkno, HASH_WRITE);
    opage = BufferGetPage(obuf, 0);
    npage = BufferGetPage(nbuf, 0);

    /* initialize the new bucket */
    _hash_pageinit(npage);
    nopaque = (HashPageOpaque) PageGetSpecialPointer(npage);
    nopaque->hasho_prevblkno = InvalidBlockNumber;
    nopaque->hasho_nextblkno = InvalidBlockNumber;
    nopaque->hasho_flag |= BUCKET_PAGE;
    nopaque->hasho_oaddr = InvalidOvflAddress;
    nopaque->hasho_bucket = nbucket;
    _hash_wrtnorelbuf(rel, nbuf);

    /* XXX only need this once */
    /* make sure the old bucket isn't empty */
    oopaque = (HashPageOpaque) PageGetSpecialPointer(opage);
    while (PageIsEmpty(opage)) {
	oblkno = oopaque->hasho_nextblkno;
	_hash_relbuf(rel, obuf, HASH_WRITE);
	if (BlockNumberIsValid(oblkno)) {

	    _hash_getbuf(rel, obuf, HASH_WRITE);
	    opage = BufferGetPage(obuf, 0);
	    oopaque = (HashPageOpaque) PageGetSpecialPointer(opage);
	}
	else {
	    _hash_relbuf(rel, nbuf, HASH_WRITE);
	    return;
	}
    }

    /* 
     * Below, omaxoffind is an offset index which starts at 0.
     * Further, PageGetMaxOffsetIndex will return 65535 for an    XXX
     * empty page & 0 for a page with one tuple. We've made sure
     * above that the page isn't empty. 
     */ 

    omaxoffind = PageGetMaxOffsetIndex(opage); 
    
    ooffind = 0;
    for(;;) {

	/* check if we're at the end of the page */
	if (ooffind > omaxoffind) {

	    /* at end of page, but check for overflow page */

	    oblkno = oopaque->hasho_nextblkno;		
	    if (BlockNumberIsValid(oblkno)) {
		
		/* have ovflpage; re-init values */
		_hash_wrtbuf(rel, obuf);
		obuf = _hash_getbuf(rel, oblkno, HASH_WRITE);
		opage = BufferGetPage(obuf, 0);
		oopaque = (HashPageOpaque) PageGetSpecialPointer(opage); /*XXX*/

		/* we're guaranteed that an ovfl page has at least 1 tuple */
		ooffind = 0;
		omaxoffind = PageGetMaxOffsetIndex(opage);
		
	    } else {
		
		/* we're at end of bucket chain */
		_hash_wrtbuf(rel, obuf);
		_hash_wrtbuf(rel, nbuf);
		_hash_squeezebucket(rel, metap, obucket);
		return;
	    }
	}
	
	/* hash on the tuple */
	/* below, oofind must start at 0:  page->pd_linp[OffsetIndex ooffind] */
	hitem = (HashItem) PageGetItem(opage, PageGetItemId(opage, ooffind));
	itup = &(hitem->hash_itup);
	itupdesc = RelationGetTupleDescriptor(rel);
	datum = (Datum) index_getattr(itup, 1, itupdesc, &null);

	bucket = _hash_call(rel, metap, datum);
	
	if (bucket == nbucket) {
	    
	    /* insert tuple on new page */
	    itemsz = IndexTupleDSize(hitem->hash_itup) 
		        + (sizeof(HashItemData) - sizeof(IndexTupleData));

	    if (PageGetFreeSpace(npage) < itemsz) {

		ovflbuf = _hash_addovflpage(rel, metabuf, nbuf);
		_hash_wrtbuf(rel, nbuf);
		nbuf = ovflbuf;
		npage = BufferGetPage(nbuf, 0);
	    }

	    noffind = PageGetMaxOffsetIndex(npage) + 2;        
	    PageAddItem(npage, (Item) hitem, itemsz, noffind, LP_USED);
	    _hash_wrtnorelbuf(rel, nbuf);
	    	
	    /* delete the tuple from old page */
	    /* PageIndexTupleDelete takes an OffsetNumber (v. an OffIndex)*/
	    PageIndexTupleDelete(opage, ooffind + 1);
	    _hash_wrtnorelbuf(rel, obuf);
	    --omaxoffind;

	    if (PageIsEmpty(opage) &&(oopaque->hasho_flag & OVERFLOW_PAGE)) {

		obuf = _hash_freeovflpage(rel, obuf);
		
		/* check that we're not through the bucket chain */
		if (BufferIsInvalid(obuf)) {
		    _hash_wrtbuf(rel, nbuf);
		    return;
		}

		/* 
		 * re-init. again, we're guaranteed that an ovfl page
		 * has at least one tuple, so re-init to 0. 
		 */
		ooffind = 0;
		omaxoffind = PageGetMaxOffsetIndex(opage);

	    }

	} else { 	

	    /* bucket == obucket */
	    ++ooffind;
	}
    }

}
