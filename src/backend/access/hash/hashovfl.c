/*
 *  Hashovfl.c -- Overflow page management code for the Postgres hash
 *	        access method.
 *
 *  Overflow pages look like ordinary relation pages.
 *
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
 *  _hash_addovflpage
 *
 *  Add an overflow page to the page currently pointed to by the buffer 
 *  argument 'buf'. 
 *
 *  *Metabufp has a read lock upon entering the function; buf has a 
 *  write lock. 
 *  
 */

Buffer
_hash_addovflpage(rel, metabufp, buf)
     Relation rel;
     Buffer *metabufp;
     Buffer buf;
{

    OverflowPageAddress oaddr;
    BlockNumber ovflblkno;
    Buffer ovflbuf;
    HashMetaPage metap;
    HashPageOpaque ovflopaque;
    HashPageOpaque opaque;
    HashPageOpaque pageopaque;
    Page page;
    Page ovflpage;

    page = BufferGetPage(buf, 0);
    pageopaque = (HashPageOpaque) PageGetSpecialPointer(page);

    metap = (HashMetaPage) BufferGetPage(*metabufp, 0);

    oaddr = _hash_getovfladdr(rel, metabufp);
    if (oaddr == InvalidOvflAddress)
	elog(WARN, "_hash_addovflpage: problem with _hash_getovfladdr.");

    ovflblkno = OADDR_TO_BLKNO(OADDR_OF(SPLITNUM(oaddr), OPAGENUM(oaddr)));

    Assert(BlockNumberIsValid(ovflblkno));

    ovflbuf = _hash_getbuf(rel, ovflblkno, HASH_WRITE);
    Assert(BufferIsValid(ovflbuf));
    ovflpage = BufferGetPage(ovflbuf, 0);
    _hash_pageinit(ovflpage);
    ovflopaque = (HashPageOpaque) PageGetSpecialPointer(ovflpage);
    ovflopaque->hasho_prevblkno = BufferGetBlockNumber(buf);
    ovflopaque->hasho_nextblkno = InvalidBlockNumber;
    ovflopaque->hasho_flag |= OVERFLOW_PAGE;
    ovflopaque->hasho_oaddr = oaddr;
    ovflopaque->hasho_bucket = pageopaque->hasho_bucket;
    _hash_wrtnorelbuf(rel, ovflbuf);

    /* logically chain overflow page to previous page */
    pageopaque->hasho_nextblkno = ovflblkno;
    _hash_wrtnorelbuf(rel, buf);
    return (ovflbuf);
}

/*
 *  _hash_getovfladdr()
 *
 *  Find an available overflow page and return it's address. 
 *
 *  When we enter this function, we have a read lock on *metabufp which
 *  we change to a write lock immediately. Before exiting, the write lock
 *  is exchanged for a read lock. 
 *
 */

OverflowPageAddress
_hash_getovfladdr(rel, metabufp)
    Relation rel;
    Buffer *metabufp;
{
    HashMetaPage metap;
    Buffer buf;
    Buffer mapbuf;
    BlockNumber blkno;
    PageOffset offset;
    OverflowPageAddress oaddr;
    SplitNumber splitnum;
    uint32 *freep;
    int32 max_free; 
    int32 bit;
    int32 first_page; 
    int32 free_bit; 
    int32 free_page; 
    int32 in_use_bits;
    int32 i, j;

    metap = (HashMetaPage) _hash_chgbufaccess(rel, metabufp, HASH_READ, HASH_WRITE);

    splitnum = metap->OVFL_POINT;
    max_free = metap->SPARES[splitnum];
    
    free_page = (max_free - 1) >> (metap->BSHIFT + BYTE_SHIFT);
    free_bit = (max_free - 1) & ((metap->BSIZE << BYTE_SHIFT) - 1);
    
    /* Look through all the free maps to find the first free block */
    first_page = metap->LAST_FREED >> (metap->BSHIFT + BYTE_SHIFT);
    for ( i = first_page; i <= free_page; i++ ) {

	blkno = metap->hashm_mapp[i];
	mapbuf = _hash_getbuf(rel, blkno, HASH_WRITE);
	freep = (uint32 *) BufferGetPage(mapbuf, 0);
	Assert(freep);

	if (i == free_page)
	    in_use_bits = free_bit;
	else
	    in_use_bits = (metap->BSIZE << BYTE_SHIFT) - 1;
	
	if (i == first_page) {
	    bit = metap->LAST_FREED &
		((metap->BSIZE << BYTE_SHIFT) - 1);
	    j = bit / BITS_PER_MAP;
	    bit = bit & ~(BITS_PER_MAP - 1);
	} else {
	    bit = 0;
	    j = 0;
	}
	for (; bit <= in_use_bits; j++, bit += BITS_PER_MAP)
	    if (freep[j] != ALL_SET)
		goto found;
    }
    
    /* No Free Page Found - have to allocate a new page */
    metap->LAST_FREED = metap->SPARES[splitnum];
    metap->SPARES[splitnum]++;
    offset = metap->SPARES[splitnum] -
	(splitnum ? metap->SPARES[splitnum - 1] : 0);
    
#define	OVMSG	"HASH: Out of overflow pages.  Out of luck.\n"

    if (offset > SPLITMASK) {
	if (++splitnum >= NCACHED) {
	    elog(WARN, OVMSG);
	}
	metap->OVFL_POINT = splitnum;
	metap->SPARES[splitnum] = metap->SPARES[splitnum-1];
	metap->SPARES[splitnum-1]--;
	offset = 0;
    }
    
    /* Check if we need to allocate a new bitmap page */
    if (free_bit == (metap->BSIZE << BYTE_SHIFT) - 1) {
	/* won't be needing old map page */
	_hash_relbuf(rel, mapbuf);
	free_page++;
	if (free_page >= NCACHED) {
	    elog(WARN, OVMSG);
	}

	/*
	 * This is tricky.  The 1 indicates that you want the new page
	 * allocated with 1 clear bit.  Actually, you are going to
	 * allocate 2 pages from this map.  The first is going to be
	 * the map page, the second is the overflow page we were
	 * looking for.  The init_bitmap routine automatically, sets
	 * the first bit of itself to indicate that the bitmap itself
	 * is in use.  We would explicitly set the second bit, but
	 * don't have to if we tell init_bitmap not to leave it clear
	 * in the first place.
	 */

	if (_hash_initbitmap(rel, metap, OADDR_OF(splitnum, offset), 1, free_page)) {
	    elog(WARN, "overflow_page: problem with _hash_initbitmap.");
	}
	metap->SPARES[splitnum]++;
	offset++;
	if (offset > SPLITMASK) {
	    if (++splitnum >= NCACHED) {
		elog(WARN, OVMSG);
	    }
	    metap->OVFL_POINT = splitnum;
	    metap->SPARES[splitnum] = metap->SPARES[splitnum-1];
	    metap->SPARES[splitnum-1]--;
	    offset = 0;
	}
    } else {

	/*
	 * Free_bit addresses the last used bit.  Bump it to address
	 * the first available bit.
	 */
	free_bit++;
	SETBIT(freep, free_bit);
	_hash_wrtbuf(rel, mapbuf);
    }
    
    /* Calculate address of the new overflow page */
    oaddr = OADDR_OF(splitnum, offset);
    _hash_chgbufaccess(rel, metabufp, HASH_WRITE, HASH_READ);
    return (oaddr);
    
 found:
    bit = bit + _hash_firstfreebit(freep[j]);
    SETBIT(freep, bit);
    _hash_wrtbuf(rel, mapbuf);

    /*
     * Bits are addressed starting with 0, but overflow pages are addressed
     * beginning at 1. Bit is a bit addressnumber, so we need to increment
     * it to convert it to a page number.
     */

    bit = 1 + bit + (i * (metap->BSIZE << BYTE_SHIFT));
    if (bit >= metap->LAST_FREED) {
	metap->LAST_FREED = bit - 1;
    }
    
    /* Calculate the split number for this page */
    for (i = 0; (i < splitnum) && (bit > metap->SPARES[i]); i++);
    offset = (i ? bit - metap->SPARES[i - 1] : bit);
    if (offset >= SPLITMASK) {
	elog(WARN, OVMSG);
    }

    /* initialize this page */
    oaddr = OADDR_OF(i, offset);
    _hash_chgbufaccess(rel, metabufp, HASH_WRITE, HASH_READ);
    return (oaddr);
}

/*
 *  _hash_firstfreebit()
 *
 *  Return the first bit that is not set in the argument 'map'. This
 *  function is used to find an available overflow page within a
 *  splitnumber. 
 * 
 */

uint32
_hash_firstfreebit(map)
    uint32 map;
{
    uint32 i, mask;
    
    mask = 0x1;
    for (i = 0; i < BITS_PER_MAP; i++) {
	if (!(mask & map))
	    return (i);
	mask = mask << 1;
    }
    return (i);
}

/*
 *  _hash_freeovflpage() - 
 *
 *  Mark this overflow page as free and return a buffer with 
 *  the page that follows it (which may be defined as
 *  InvalidBuffer). 
 *
 */

Buffer
_hash_freeovflpage(rel, ovflbuf)
    Relation rel;
    Buffer ovflbuf;
{
    HashMetaPage metap;
    Buffer metabuf;
    Buffer prevbuf;
    Buffer mapbuf;
    BlockNumber blkno;
    BlockNumber nextblkno;
    HashPageOpaque ovflopaque;
    HashPageOpaque prevopaque;
    Page page;
    Page prevpage;
    Page ovflpage;
    OverflowPageAddress addr;
    SplitNumber splitnum;
    uint32 *mapp;
    int32 ovflpgno, bitmappage, bitmapbit;


    metabuf = _hash_getbuf(rel, HASH_METAPAGE, HASH_WRITE);
    metap = (HashMetaPage) BufferGetPage(metabuf, 0);

    ovflpage = BufferGetPage(ovflbuf, 0);
    ovflopaque = (HashPageOpaque) PageGetSpecialPointer(ovflpage);
    addr = ovflopaque->hasho_oaddr;
    nextblkno = ovflopaque->hasho_nextblkno;
    _hash_relbuf(rel, ovflbuf, HASH_WRITE);

    /* 
     * Fix up the bucket chain. 
     */
    
    prevbuf = _hash_getbuf(rel, ovflopaque->hasho_prevblkno, HASH_WRITE);
    prevpage = BufferGetPage(prevbuf, 0);
    prevopaque = (HashPageOpaque) PageGetSpecialPointer(prevpage);
    prevopaque->hasho_nextblkno = nextblkno;
    _hash_wrtbuf(rel, prevbuf);

    /* 
     * Fix up the overflow page bitmap that tracks this particular
     * overflow page. The bitmap can be found in the MetaPageData
     * array element hashm_mapp[bitmappage].
     */
    splitnum = (addr >> SPLITSHIFT);
    ovflpgno =
	(splitnum ? metap->SPARES[splitnum - 1] : 0) + (addr & SPLITMASK) - 1;

    if (ovflpgno < metap->LAST_FREED) {
	metap->LAST_FREED = ovflpgno;
    }

    bitmappage = (ovflpgno >> (metap->BSHIFT + BYTE_SHIFT));
    bitmapbit = ovflpgno & ((metap->BSIZE << BYTE_SHIFT) - 1);
    
    blkno = metap->hashm_mapp[bitmappage];
    mapbuf = _hash_getbuf(rel, blkno, HASH_WRITE);
    mapp = (uint32 *) BufferGetPage(mapbuf, 0);
    CLRBIT(mapp, bitmapbit);
    _hash_wrtbuf(rel, mapbuf);

    _hash_relbuf(rel, metabuf, HASH_WRITE);

    /* 
     * now instantiate the page that replaced this one, 
     * if it exists, and return that buffer with a write lock.
     */
    if (BlockNumberIsValid(nextblkno)) {
	return (_hash_getbuf(rel, nextblkno, HASH_WRITE));
    } else {
	return (InvalidBuffer);
    }
}


/*
 *  _hash_initbitmap()
 *  
 *   Initialize a new bitmap page.  The meta page buffer, metabuf, has
 *   a write-lock upon entering the function. 
 */

#define BYTE_MASK	((1 << INT_BYTE_SHIFT) -1)

int32
_hash_initbitmap(rel, metabuf, pnum, nbits, ndx)
    Relation rel;
    Buffer metabuf;
    int32 pnum, nbits, ndx;
{
    HashMetaPage metap;
    Buffer buf;
    BlockNumber blkno;
    Page page;
    u_long *ip;
    int clearbytes, clearints;

    metap = (HashMetaPage) BufferGetPage(metabuf, 0);
    
    blkno = OADDR_TO_BLKNO(pnum);
    buf = _hash_getbuf(rel, blkno, HASH_WRITE);
    page = BufferGetPage(buf, 0);

    /*
     * Bitmap page has no information on it other than the bitmap.
     */
    bzero(page, BLCKSZ);
    ip = (u_long *) BufferGetPage(buf, 0);


    /* Metabuf has a write lock */
    metap->hashm_nmaps++;

    clearints = ((nbits - 1) >> INT_BYTE_SHIFT) + 1;
    clearbytes = clearints << INT_TO_BYTE;
    (void)memset((char *)ip, 0, clearbytes);
    (void)memset(((char *)ip) + clearbytes, 0xFF,
		 metap->BSIZE - clearbytes);
    ip[clearints - 1] = ALL_SET << (nbits & BYTE_MASK);
    SETBIT(ip, 0);
    
    _hash_wrtbuf(rel, buf);

    metap->BITMAPS[ndx] = (uint16) pnum;
    metap->hashm_mapp[ndx] = blkno;
    
    return (0);
}


/*
 *  _hash_squeezebucket(rel, bucket)
 *
 *  Try to squeeze the tuples onto pages occuring earlier in the bucket
 *  chain in an attempt to free overflow pages. When we start the "squeezing",
 *  the page which we start taking tuples from (the "read" page) is the
 *  last bucket in the bucket chain and the page which we start squeezing
 *  tuples onto (the "write" page) is the first page in the bucket chain.
 *  The procedure terminates when the read page and write page are the same
 *  page. 
 */

void
_hash_squeezebucket(rel, metap, bucket)
    Relation rel;
    HashMetaPage metap; 
    Bucket bucket;
{
    Buffer wbuf;
    Buffer rbuf;
    BlockNumber wblkno;		
    BlockNumber rblkno;		
    Page wpage;
    Page rpage;
    HashPageOpaque wopaque;
    HashPageOpaque ropaque;
    OffsetIndex woffind;
    OffsetIndex roffind;
    HashItem hitem;
    int itemsz;

    wblkno = BUCKET_TO_BLKNO(bucket);
    wbuf = _hash_getbuf(rel, wblkno, HASH_WRITE);
    wpage = BufferGetPage(wbuf, 0);
    wopaque = (HashPageOpaque) PageGetSpecialPointer(wpage);
    
    /* make sure there's at least one ovfl page */
    if (!BlockNumberIsValid(wopaque->hasho_nextblkno)) {
	_hash_relbuf(rel, wbuf, HASH_WRITE);
	return;
    }
   
    /* find the first overflow page */
    rblkno = wopaque->hasho_nextblkno;
    rbuf = _hash_getbuf(rel, rblkno, HASH_WRITE);
    rpage = BufferGetPage(rbuf, 0);
    ropaque = (HashPageOpaque) PageGetSpecialPointer(rpage);

    /* find the last page in the bucket chain */
    while (BlockNumberIsValid(ropaque->hasho_nextblkno)) {

	rblkno = ropaque->hasho_nextblkno;
	_hash_relbuf(rel, rbuf, HASH_WRITE);
	rbuf = _hash_getbuf(rel, rblkno, HASH_WRITE);
	rpage = BufferGetPage(rbuf, 0);
	ropaque = (HashPageOpaque) PageGetSpecialPointer(rpage);

    }

    /* squeeze the tuples */
    roffind = 0;
    for(;;) {
	
	/* PageGetItemId takes an Offset Index */
	hitem = (HashItem) PageGetItem(rpage, PageGetItemId(rpage, roffind));
	itemsz = IndexTupleDSize(hitem->hash_itup) 
	       + (sizeof(HashItemData) - sizeof(IndexTupleData));

	while (PageGetFreeSpace(wpage) < itemsz) {
	    /* no room, so get the next page up */
	    wblkno = wopaque->hasho_nextblkno;
	    if (!BufferIsValid(wblkno) || (wblkno == rblkno)) {
		_hash_wrtbuf(rel, rbuf);
		_hash_wrtbuf(rel, wbuf);
		return;
	    }

	    _hash_wrtbuf(rel, wbuf);
	    wbuf = _hash_getbuf(rel, wblkno, HASH_WRITE);
	    wpage = BufferGetPage(wbuf, 0);
	    wopaque = (HashPageOpaque) PageGetSpecialPointer(wpage);
	    
	}
	
	
	/* 
	 * if we're here, we have found room so insert on the 
	 * "write" page. The constant 2 below takes into account that
	 * offind is an Offset Index and we need an Offset Number and
	 * the fact that we want one higher index than the current max index.
	 */

	woffind = PageGetMaxOffsetIndex(wpage) + 2;
	PageAddItem(wpage, (Item) hitem, itemsz, woffind, LP_USED);
	
	/* 
	 * Delete the tuple from the "read" page. 
	 * PageIndexTupleDelete takes an OffsetNumber.
	 */
	PageIndexTupleDelete(rpage, roffind + 1);
	_hash_wrtnorelbuf(rel, rbuf);


	if (PageIsEmpty(rpage) && (ropaque->hasho_flag & OVERFLOW_PAGE)) {

	    /* page is empty and it's an overflow page */
	    rblkno = ropaque->hasho_prevblkno;
	    rbuf = _hash_freeovflpage(rel, rbuf);
	    /* _hash_freeovflpage returns next page, we want previous */
	    if (BufferIsValid(rbuf))
		_hash_relbuf(rel, rbuf, HASH_WRITE);
	    
	    if (rblkno == wblkno) {
		/* freeovflpage releases the rbuf buffer so we don't */
		_hash_wrtbuf(rel, wbuf);
		return;
	    }

	    rbuf = _hash_getbuf(rel, rblkno, HASH_WRITE);
	    rpage = BufferGetPage(rbuf, 0);
	    ropaque = (HashPageOpaque) PageGetSpecialPointer(rpage);
	    roffind = 0;
	}
    }
}














