/* ----------------------------------------------------------------
 *   FILE
 *	bufpage.c
 *
 *   DESCRIPTION
 *	POSTGRES standard buffer page code.
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */
#include <sys/types.h>
#include <sys/file.h>
#include "tmp/align.h"

#include "tmp/c.h"

#include "storage/item.h"

#include "storage/buf.h"
#include "storage/bufmgr.h"
#include "utils/log.h"
#include "storage/page.h"
#include "storage/pagenum.h"
#include "storage/part.h"

#include "storage/bufpage.h"

RcsId("$Header$");

static bool PageManagerShuffle = true;	/* default is shuffle mode */

void PageRepairFragmentation ARGS((Page	page));
    
/* ----------------------------------------------------------------
 *			Buffer support functions
 * ----------------------------------------------------------------
 */
PageSize
BufferGetPageSize(buffer)
    Buffer	buffer;
{
    PageSize	pageSize;
    
    Assert(BufferIsValid(buffer));
    pageSize = PageGetPageSize((Page)BufferGetBlock(buffer));
    
    Assert(PageSizeIsValid(pageSize));
    return (pageSize);
}

Page
BufferGetPage(buffer, pageNumber)
    Buffer	buffer;
    PageNumber	pageNumber;
{
    Assert(pageNumber == FirstPageNumber);
    return (Page) BufferGetBlock(buffer);
}

Page
BufferGetPageDebug(buffer, pageNumber)
    Buffer	buffer;
    PageNumber	pageNumber;
{
    Page 		page;
    LocationIndex	lower, upper, special;
   
    page = BufferGetPage(buffer, pageNumber);

    lower =   ((PageHeader)page)->pd_lower;
    upper =   ((PageHeader)page)->pd_upper;
    special = ((PageHeader)page)->pd_special;

    Assert((lower > 7));
    Assert((lower <= upper));
    Assert((upper <= special));
   
    return page;
}

/* ----------------------------------------------------------------
 *			Page support functions
 * ----------------------------------------------------------------
 */

#define OpaqueSetInternalFragmentation(opaque, frag) \
    Assert(frag >= 0); \
    Assert(frag <= MaxInternalFragmentation); \
    (opaque)->fragmentation = (frag)

static
void
OpaqueSetPageSize(opaque, pageSize)
    Opaque	opaque;
    PageSize	pageSize;
{
    int	i;
    Assert(PageSizeIsValid(pageSize));

    pageSize >>= 1;
    for (i = 0; pageSize >>= 1; i++);
    opaque->pageSize = i;
}

/* ----------------
 *	PageInit
 * ----------------
 */
void
PageInit(page, pageSize, specialSize)
    Page		page;
    PageSize	pageSize;
    SpecialSize	specialSize;
{
    Assert(pageSize == BLCKSZ);

    Assert(pageSize>specialSize+sizeof(PageHeaderData)-sizeof (ItemIdData));

    ((PageHeader)page)->pd_lower =
	sizeof (PageHeaderData) - sizeof(ItemIdData);
    
    ((PageHeader)page)->pd_upper = pageSize - LONGALIGN(specialSize);
    ((PageHeader)page)->pd_special = pageSize - specialSize;

    OpaqueSetPageSize((Opaque)&((PageHeader)page)->opaque, pageSize);
    
    OpaqueSetInternalFragmentation((Opaque)&((PageHeader)page)->opaque,
				   (InternalFragmentation)0);
}

/* ----------------
 *	PageGetItem
 * ----------------
 */
Item
PageGetItem(page, itemId)
    Page	page;
    ItemId	itemId;
{
    Item	item;

    Assert(PageIsValid(page));
    Assert((*itemId).lp_flags & LP_USED);
    Assert(!((*itemId).lp_flags & LP_CTUP));

    if ((*itemId).lp_flags & LP_DOCNT) {
	item = (Item)(((char *)page) + (*itemId).lp_off + TCONTPAGELEN);
    } else {
	item = (Item)(((char *)page) + (*itemId).lp_off);
    }

    return (item);
}

/* ----------------
 *	PageAddItem
 *
 *	add an item to a page.
 *
 *   Notes on interface:
 *  	If offsetNumber is valid, shuffle ItemId's down to make room
 * 	to use it, if PageManagerShuffle is true.  If PageManagerShuffle is
 *  	false, then overwrite the specified ItemId.  (PageManagerShuffle is
 *  	true by default, and is modified by calling PageManagerModeSet.)
 *  	If offsetNumber is not valid, then assign one by finding the first 
 *  	one that is both unused and deallocated.
 *
 *   NOTE: If offsetNumber is valid, and PageManagerShuffle is true, it
 *  	is assumed that there is room on the page to shuffle the ItemId's
 *  	down by one.
 * ----------------
 */
OffsetNumber
PageAddItem(page, item, size, offsetNumber, flags)
    Page		page;
    Item		item;
    Size		size;
    OffsetNumber	offsetNumber;
    ItemIdFlags		flags;
{
    register 		i;
    Size		alignedSize;
    Offset		lower;
    Offset		upper;
    ItemId		itemId;
    ItemId		fromitemId, toitemId;
    OffsetNumber 	limit;
    
    bool shuffled = false;

    /*
     *  Find first unallocated offsetNumber
     */
    limit = 2 + PageGetMaxOffsetIndex(page);

    /* was offsetNumber passed in? */
    if (OffsetNumberIsValid(offsetNumber)) {
	if (PageManagerShuffle == true) {
	    /* shuffle ItemId's (Do the PageManager Shuffle...) */
	    for (i = (limit - 1); i >= offsetNumber; i--) {
		fromitemId = &((PageHeader)page)->pd_linp[i - 1];
		toitemId = &((PageHeader)page)->pd_linp[i];
		*toitemId = *fromitemId;
	    }
	    shuffled = true;	/* need to increase "lower" */
	} else { /* overwrite mode */
	    itemId = &((PageHeader)page)->pd_linp[offsetNumber - 1];
	    if (((*itemId).lp_flags & LP_USED)  || 
		((*itemId).lp_len != 0)) {
		elog(WARN, "PageAddItem: tried overwrite of used ItemId");
		return (InvalidOffsetNumber);
	    }
	}
    } else {	/* offsetNumber was not passed in, so find one */
	/* look for "recyclable" (unused & deallocated) ItemId */
	for (offsetNumber = 1; offsetNumber < limit; offsetNumber++) {
	    itemId = &((PageHeader)page)->pd_linp[offsetNumber - 1];
	    if ((((*itemId).lp_flags & LP_USED) == 0) && 
		((*itemId).lp_len == 0)) 
		break;
	}
    }
    if (offsetNumber > limit)
	lower = (Offset) (((char *) (&((PageHeader)page)->pd_linp[offsetNumber])) - ((char *) page));
    else if (offsetNumber == limit || shuffled == true)
	lower = ((PageHeader)page)->pd_lower + sizeof (ItemIdData);
    else
	lower = ((PageHeader)page)->pd_lower;

    alignedSize = LONGALIGN(size);

    upper = ((PageHeader)page)->pd_upper - alignedSize;
    
    if (lower > upper) {
	return (InvalidOffsetNumber);
    }

    itemId = &((PageHeader)page)->pd_linp[offsetNumber - 1];
    (*itemId).lp_off = upper;
    (*itemId).lp_len = size;
    (*itemId).lp_flags = flags;
    bcopy(item, (char *)page + upper, size);
    ((PageHeader)page)->pd_lower = lower;
    ((PageHeader)page)->pd_upper = upper;

    return (offsetNumber);
}

/* ----------------
 *	PageRemoveItem
 * ----------------
 */
ReturnStatus
PageRemoveItem(page, itemId)
    Page		page;
    ItemId		itemId;
{
    Size		alignedSize;
    Offset		lower;
    Offset		upper;

    Assert(PageIsValid(page));
    Assert(ItemIdIsUsed(itemId));
    Assert(!ItemIdIsContinuation(itemId) || !ItemIdIsContinuing(itemId));
    /* cannot handle long tuples yet. */

    itemId->lp_flags &= ~LP_USED;	/* XXX should have function call */

    PageRepairFragmentation(page);
}

/* ----------------
 *	PageGetTempPage
 * ----------------
 */
Page
PageGetTempPage(page, specialSize)
    Page	page;
    Size	specialSize;
{
    PageSize	pageSize;
    PageSize	size;
    Page		temp;
    PageHeader	thdr;

    pageSize = PageGetPageSize(page);

    if ((temp = (Page) palloc(pageSize)) == (Page) NULL)
	elog(FATAL, "Cannot allocate %d bytes for temp page.", pageSize);
    thdr = (PageHeader) temp;

    /* copy old page in */
    bcopy(page, temp, pageSize);

    /* clear out the middle */
    size = (pageSize - sizeof(PageHeaderData)) + sizeof(ItemIdData);
    size -= specialSize;
    bzero((char *) &(thdr->pd_linp[0]), size);

    /* set high, low water marks */
    thdr->pd_lower = sizeof (PageHeaderData) - sizeof (ItemIdData);
    thdr->pd_upper = pageSize - LONGALIGN(specialSize);

    return (temp);
}

/* ----------------
 *	PageRestoreTempPage
 * ----------------
 */
void
PageRestoreTempPage(tempPage, oldPage)
    Page	tempPage;
    Page	oldPage;
{
    PageSize	pageSize;

    pageSize = PageGetPageSize(tempPage);
    bcopy((char *) tempPage, (char *) oldPage, pageSize);

    pfree(tempPage);
}

/* ----------------
 *	PageGetMaxOffsetIndex
 * ----------------
 */
OffsetIndex
PageGetMaxOffsetIndex(page)
    Page	page;
{
    LocationIndex	low;
    OffsetIndex		i;

    low = ((PageHeader) page)->pd_lower;
    i = (low - (sizeof(PageHeaderData) - sizeof(ItemIdData))) / sizeof(ItemIdData);

    return(--i);
}	

/* ----------------
 *	idemid stuff for PageRepairFragmentation
 * ----------------
 */


struct itemIdData {
	OffsetIndex offsetindex;
	ItemIdData  itemiddata;
};

static
itemidcompare(itemidp1, itemidp2)
	struct itemIdData *itemidp1, *itemidp2;
{
	if (itemidp1->itemiddata.lp_off == itemidp2->itemiddata.lp_off)
		return(0);
	else if (itemidp1->itemiddata.lp_off < itemidp2->itemiddata.lp_off)
		return(1);
	else
		return(-1);
}

/* ----------------
 *	PageRepairFragmentation
 * ----------------
 */
void
PageRepairFragmentation(page)
    Page	page;
{
    int 		i;
    struct itemIdData 	*itemidbase, *itemidptr;
    ItemId 		lp;
    int 		nline, nused;
    OffsetIndex 	offsetindex;
    int 		itemidcompare();
    Offset 		upper;
    Size 		alignedSize;

    nline = 1 + (int16)PageGetMaxOffsetIndex(page);
    nused = 0;
    for (i=0; i<nline; i++) {
	lp = ((PageHeader)page)->pd_linp + i;
	if ((*lp).lp_flags & LP_USED)
	    nused++;
    }

    if (nused == 0) {
	for (i=0; i<nline; i++) {
	    lp = ((PageHeader)page)->pd_linp + i;
	    if ((*lp).lp_len > 0) 	/* unused, but allocated */
		(*lp).lp_len = 0;	/* indicate unused & deallocated */
	}

	((PageHeader)page)->pd_upper = ((PageHeader)page)->pd_special;

	/* the following function may need to be added to bufpage.[hc] */
	/* PageSetInternalFragmentation(page, (InternalFragmentation)0); */

    } else {	/* nused != 0 */
	itemidbase = (struct itemIdData *) 
	    palloc(sizeof(struct itemIdData) * nused);
	bzero((char *) itemidbase, sizeof(struct itemIdData) * nused);
	itemidptr = itemidbase;
	for (i=0; i<nline; i++) {
	    lp = ((PageHeader)page)->pd_linp + i;
	    if ((*lp).lp_flags & LP_USED) {
		itemidptr->offsetindex = i;
		itemidptr->itemiddata = *lp;
		itemidptr++;
	    } else {
		if ((*lp).lp_len > 0) 	/* unused, but allocated */
		    (*lp).lp_len = 0;	/* indicate unused & deallocated */
	    }
	}

	/* sort itemIdData array...*/
	qsort((char *) itemidbase, nused, sizeof(struct itemIdData),
	      itemidcompare);

	/* compactify page */
	((PageHeader)page)->pd_upper = ((PageHeader)page)->pd_special;

	/* the following function may need to be added to bufpage.[hc] */
	/* PageSetInternalFragmentation(page, (InternalFragmentation)0); */

	for (i=0, itemidptr = itemidbase; i<nused; i++, itemidptr++) {
	    lp = ((PageHeader)page)->pd_linp + itemidptr->offsetindex;
	    alignedSize = LONGALIGN((*lp).lp_len);
	    upper = ((PageHeader)page)->pd_upper - alignedSize;
	    bcopy((char *)page + (*lp).lp_off, (char *) page + upper,
		  (*lp).lp_len);
	    (*lp).lp_off = upper;
	    ((PageHeader)page)->pd_upper = upper;
	}

	pfree(itemidbase);
    }
}

/* ----------------
 *	PageGetFreeSpace
 * ----------------
 */
PageFreeSpace
PageGetFreeSpace(page)
    Page	page;
{
    PageFreeSpace	space;


    space = ((PageHeader)page)->pd_upper - ((PageHeader)page)->pd_lower;

    if (space < sizeof (ItemIdData)) {
	return (0);
    }
    space -= sizeof (ItemIdData);		/* XXX not always true */

    return (space);
}

/* ----------------
 *	PageManagerModeSet
 * ----------------
 */
void
PageManagerModeSet(mode)
    PageManagerMode mode;
{
    if (mode == ShufflePageManagerMode)
	PageManagerShuffle = true;
    else if (mode == OverwritePageManagerMode)
	PageManagerShuffle = false;
}

/*
 *----------------------------------------------------------------
 * PageIndexTupleDelete
 *----------------------------------------------------------------
 *
 *	This routine does the work of removing a tuple from an index page.
 */

PageIndexTupleDelete(page, offset)
    Page page;
    OffsetNumber offset;
{
    PageHeader 	phdr;
    char 	*addr;
    ItemId 	tup;
    Size 	size;
    char 	*locn;
    int 	nbytes;

    phdr = (PageHeader) page;

    /* change offset number to offset index */
    offset--;

    tup = PageGetItemId(page, offset);
    size = ItemIdGetLength(tup);
    size = LONGALIGN(size);

    /* location of deleted tuple data */
    locn = (char *) (page + ItemIdGetOffset(tup));

    /*
     * First, we want to get rid of the pd_linp entry for the index
     * tuple.  We copy all subsequent linp's back one slot in the
     * array.
     */

    nbytes = phdr->pd_lower -
		((char *)&phdr->pd_linp[offset+1] - (char *) phdr);
    bcopy((char *) &(phdr->pd_linp[offset + 1]),
	  (char *) &(phdr->pd_linp[offset]),
	  nbytes);

    /*
     * Now move everything between the old upper bound (beginning of tuple
     * space) and the beginning of the deleted tuple forward, so that
     * space in the middle of the page is left free.  If we've just deleted
     * the tuple at the beginning of tuple space, then there's no need
     * to do the copy (and bcopy on some architectures SEGV's if asked
     * to move zero bytes).
     */

    /* beginning of tuple space */
    addr = (char *) (page + phdr->pd_upper);

    if (locn != addr)
	bcopy(addr, addr + size, (int) (locn - addr));

    /* adjust free space boundary pointers */
    phdr->pd_upper += size;
    phdr->pd_lower -= sizeof (ItemIdData);

    /* finally, we need to adjust the linp entries that remain */
    if (!PageIsEmpty(page))
	PageIndexTupleDeleteAdjustLinePointers(phdr, locn, size);
}

/*
 *----------------------------------------------------------------
 * PageIndexTupleDeleteAdjustLinePointers
 *----------------------------------------------------------------
 *
 *	Once the line pointers and tuple data have been shifted around
 *	on the page, we need to go down the line pointer vector and
 *	adjust pointers to reflect new locations.  Anything that used
 *	to be before the deleted tuple's data was moved forward by the
 *	size of the deleted tuple.
 *
 *	This routine does the work of adjusting the line pointers.
 *	Location is where the tuple data used to lie; size is how
 *	much space it occupied.  We assume that size has been aligned
 *	as required by the time we get here.
 *
 *	This routine should never be called on an empty page.
 */

PageIndexTupleDeleteAdjustLinePointers(phdr, location, size)
    register PageHeader phdr;
    char 		*location;
    Size 		size;
{
    int i;

    /* location is an index into the page... */
    location -= (int) phdr;

    for (i = PageGetMaxOffsetIndex((Page) phdr); i >= 0; i--)	{
	if (phdr->pd_linp[i].lp_off <= (unsigned) location) {
	    phdr->pd_linp[i].lp_off += size;
	}
    }
}
