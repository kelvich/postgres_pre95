/*
 *  hashsearch.c -- search code for postgres hash tables
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

#include "fmgr.h"

#include "access/heapam.h"
#include "access/genam.h"
#include "access/ftup.h"
#include "access/skey.h"
#include "access/sdir.h"
#include "access/hash.h"

RcsId("$Header$");


/*
 *  _hash_search() -- Finds the page/bucket that the contains the
 *  scankey and loads it into *bufP.
 */

void
_hash_search(rel, keysz, scankey, bufP, metap)
    Relation rel;
    int keysz;		   
    ScanKey scankey;
    Buffer *bufP;
    HashMetaPage metap;
{
    Buffer metabuf;
    BlockNumber blkno;
    Datum keyDatum;
    Bucket bucket;

    if ((keyDatum = scankey->data[0].argument) == (Datum) NULL) {
	/* 
	 * If the scankey argument is NULL, all tuples will
	 * satisfy the scan so we start the scan at  the
	 * first bucket (bucket #0).
	 */
	bucket = 0;
    } else {
	bucket = _hash_call(rel, metap, keyDatum);
    }

    blkno = BUCKET_TO_BLKNO(bucket);
    
    *bufP = _hash_getbuf(rel, blkno, HASH_READ);
}

/*
 *  _hash_next() -- Get the next item in a scan.
 *
 *	On entry, we have a valid currentItemData in the scan, and a
 *	read lock on the page that contains that item.  We do not have
 *	the page pinned.  We return the next item in the scan.  On
 *	exit, we have the page containing the next item locked but not
 *	pinned.
 */

RetrieveIndexResult
_hash_next(scan)
    IndexScanDesc scan;
{
    Relation rel;
    HashMetaPage metap;
    HashPageOpaque opaque;
    Buffer buf;
    Buffer metabuf;
    Page page;
    OffsetIndex offind, maxoff;
    RetrieveIndexResult res;
    BlockNumber blkno;
    ItemPointer current;
    ItemPointer iptr;
    HashItem hitem;
    IndexTuple itup;
    HashScanOpaque so;

    rel = scan->relation;
    so = (HashScanOpaque) scan->opaque; 
    current = &(scan->currentItemData);

    metabuf = _hash_getbuf(rel, HASH_METAPAGE, HASH_READ);
    metap = (HashMetaPage) BufferGetPage(metabuf, 0);

    /*
     *  XXX 10 may 91:  somewhere there's a bug in our management of the
     *  cached buffer for this scan.  wei discovered it.  the following
     *  is a workaround so he can work until i figure out what's going on.
     */

    if (!BufferIsValid(so->hashso_curbuf))
	so->hashso_curbuf = _hash_getbuf(rel, ItemPointerGetBlockNumber(current),
				     HASH_READ);

    /* we still have the buffer pinned and locked */
    buf = so->hashso_curbuf;

    /* step to next valid tuple */
    if (!_hash_step(scan, &buf, metabuf))
	return ((RetrieveIndexResult) NULL);

    /* if we're here, _hash_step found a valid tuple */
    current = &(scan->currentItemData);
    offind = ItemPointerGetOffsetNumber(current, 0) - 1;
    page = BufferGetPage(buf, 0);
    hitem = (HashItem) PageGetItem(page, PageGetItemId(page, offind));
    itup = &hitem->hash_itup;
    iptr = (ItemPointer) palloc(sizeof(ItemPointerData));
    bcopy((char *) &(itup->t_tid), (char *) iptr, sizeof(ItemPointerData));
    res = ItemPointerFormRetrieveIndexResult(current, iptr);

    return (res);
}

/*
 *  _hash_first() -- Find the first item in a scan.
 *
 *	Return the RetrieveIndexResult of the first item in the tree that
 *	satisfies the qualificatin associated with the scan descriptor. On
 * 	exit, the page containing the current index tuple is read locked
 * 	and pinned, and the scan's opaque data entry is updated to 
 *	include the buffer.  
 */

RetrieveIndexResult
_hash_first(scan)
    IndexScanDesc scan;
{
    Relation rel;
    TupleDescriptor itupdesc;
    Buffer buf;
    Buffer metabuf;
    Page page;
    OffsetIndex offind, maxoff;
    HashMetaPage metap;
    HashItem hitem;
    IndexTuple itup;
    ItemPointer current;
    ItemPointer iptr;
    BlockNumber blkno;
    StrategyNumber strat;
    RetrieveIndexResult res;
    RegProcedure proc;
    int result;
    HashScanOpaque so;

    rel = scan->relation;
    itupdesc = RelationGetTupleDescriptor(scan->relation);
    current = &(scan->currentItemData);
    so = (HashScanOpaque) scan->opaque;

    metabuf = _hash_getbuf(rel, HASH_METAPAGE, HASH_READ);
    metap = (HashMetaPage) BufferGetPage(metabuf, 0);

    /*
     *  XXX -- The attribute number stored in the scan key is the attno
     *	       in the heap relation.  We need to transmogrify this into
     *         the index relation attno here.  For the moment, we have
     *	       hardwired attno == 1.
     */

    /* find the correct bucket/page and load it into buf */
    _hash_search(rel, 1, &(scan->keyData), &buf, metap);

    /*
     *  This will happen if the table we're searching is entirely empty
     *  and there are no matching tuples in the index.
     */
    page = BufferGetPage(buf, 0);
    if (PageIsEmpty(page)) {
	_hash_relbuf(rel, buf, HASH_READ);
	ItemPointerSetInvalid(current);
	buf = so->hashso_curbuf = InvalidBuffer;
	return ((RetrieveIndexResult) NULL);
    }

    if (!_hash_step(scan, &buf, metabuf))
	return ((RetrieveIndexResult) NULL);

    /* if we're here, _hash_step found a valid tuple */
    current = &(scan->currentItemData);
    offind = ItemPointerGetOffsetNumber(current, 0) - 1;
    page = BufferGetPage(buf, 0);
    hitem = (HashItem) PageGetItem(page, PageGetItemId(page, offind));
    itup = &hitem->hash_itup;
    iptr = (ItemPointer) palloc(sizeof(ItemPointerData));
    bcopy((char *) &(itup->t_tid), (char *) iptr, sizeof(ItemPointerData));
    res = ItemPointerFormRetrieveIndexResult(current, iptr);

    return (res);
}

/*
 *  _hash_step() -- step to the next valid item in a scan in the bucket.
 *
 *	If no valid record exists in the requested direction, return
 *	false.  Else, return true and set the CurrentItemData for the
 *	scan to the right thing.
 * 
 *	bufP points to the buffer which contains the current page
 *	that we'll step through. 
 */

bool
_hash_step(scan, bufP, metabuf)
    IndexScanDesc scan;
    Buffer *bufP;
    Buffer metabuf;
{
    Bucket nextbucket;
    BlockNumber blkno;
    BlockNumber oblknum;
    HashMetaPage metap;
    HashPageOpaque opaque;
    HashScanOpaque so;
    HashItem hitem;
    IndexTuple itup;
    ItemPointer current;
    ItemId itemid;
    OffsetIndex maxoff;
    OffsetIndex offind;
    Page page;
    Relation rel;

    rel = scan->relation;
    current = &(scan->currentItemData);
    so = (HashScanOpaque) scan->opaque;
    blkno = BufferGetBlockNumber(*bufP);

    metap = (HashMetaPage) BufferGetPage(metabuf, 0);

    /* XXX If _hash_step is called from _hash_first, 
     * current will not be valid, so can't dereference
     * it. If that's the case we'll set offind to 
     * -1 rather. Offind will then be incremented to 
     * the correct value, 0, in the do/while below. 
     * If offind were an OffsetIndex rather than a signed
     * int, this wouldn't work. 
     */

    if (ItemPointerIsValid(current)) {
	offind = ItemPointerGetOffsetNumber(current, 0);
    } else {
	    offind = 0;    
    }

    page = BufferGetPage(*bufP, 0);
    maxoff = PageGetMaxOffsetIndex(page);

    /*
     * Continue to step through tuple until:
     *
     * 	  1) we get to the end of the bucket chain or
     * 	  2) we find a valid tuple.
     * 
     * Note: if the scankey is NULL, we'll continue
     *       until all the tuples are exhausted.
     */

    do {
	/* get the next tuple */
	if ((offind > maxoff) || PageIsEmpty(page)) {
	    
	    /* at end of page, but check for overflow page */		
	    opaque = (HashPageOpaque) PageGetSpecialPointer(page);
	    blkno = opaque->hasho_nextblkno;
	    if (BlockNumberIsValid(blkno)) {
		
		/* have ovflpage; re-init values */
		_hash_relbuf(rel, *bufP, HASH_READ);
		ItemPointerSetInvalid(current);
		*bufP = _hash_getbuf(rel, blkno, HASH_READ);
		page = BufferGetPage(*bufP, 0);
		opaque = (HashPageOpaque) PageGetSpecialPointer(page);
		/* ovfl page guaranteed to have at least one tuple */
		offind = 0;
		maxoff = PageGetMaxOffsetIndex(page);
		
	    } else {
		
		/* we're at end of bucket chain, but check for vacuuming */
		if ((scan->numberOfKeys <= 0) 
		    && ((nextbucket = opaque->hasho_bucket + 1) 
			<= metap->hashm_maxbucket)) {
		    
		    /* get next bucket */
		    _hash_relbuf(rel, *bufP, HASH_READ);
		    blkno = BUCKET_TO_BLKNO(nextbucket);
		    ItemPointerSetInvalid(current);
		    *bufP = _hash_getbuf(rel, blkno, HASH_READ);
		    return(_hash_step(scan, bufP, metabuf));
		    
		} else {
		    
		    _hash_relbuf(rel, *bufP, HASH_READ);
		    _hash_relbuf(rel, metabuf, HASH_READ);
		    ItemPointerSetInvalid(current);
		    *bufP = so->hashso_curbuf = InvalidBuffer;
		    return (false);
		}
	    }
	}
	
	/* get ready to check this tuple */
	/* below, PageGetItemId takes an offset index which starts at 0 */
	hitem = (HashItem) PageGetItem(page, PageGetItemId(page, offind));
	itup = &hitem->hash_itup;
	++offind;

    } while (!_hash_checkqual(scan, itup));
   

    /* if we made it to here, we've found a valid tuple */
    _hash_relbuf(rel, metabuf, HASH_READ);
    ItemPointerSet(current, 0, blkno, 0, offind);
    so->hashso_curbuf = *bufP;
    return (true);
}






