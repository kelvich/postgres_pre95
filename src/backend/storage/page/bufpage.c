
/*
 * 
 * POSTGRES Data Base Management System
 * 
 * Copyright (c) 1988 Regents of the University of California
 * 
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for educational, research, and non-profit purposes and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of the University of California not be used in advertising
 * or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Permission to incorporate this
 * software into commercial products can be obtained from the Campus
 * Software Office, 295 Evans Hall, University of California, Berkeley,
 * Ca., 94720 provided only that the the requestor give the University
 * of California a free licence to any derived software for educational
 * and research purposes.  The University of California makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 * 
 */



/*
 * bufpage.c --
 *	POSTGRES standard buffer page code.
 */

#include <sys/types.h>
#include <sys/file.h>
#include "context.h"
#include "align.h"

#include "c.h"
#include "clib.h"

#include "item.h"

#include "buf.h"
#include "bufmgr.h"
#include "log.h"
#include "page.h"
#include "pagenum.h"
#include "part.h"

#include "bufpage.h"
#include "internal_page.h"

RcsId("$Header$");

/* #include "status.h" */

static bool PageManagerShuffle = true;	/* default is shuffle mode */

/*
 * PageRepairFragmentation --
 *	Frees fragmented space on a page.
 */
extern
void
PageRepairFragmentation ARGS((
	Page	page
));

bool
PageSizeIsValid(pageSize)
	PageSize	pageSize;
{
	/* should check that this is a power of 2 */
	return (true);
}

bool
PageIsUsed(page)
	Page	page;
{
	Assert(PageIsValid(page));

	return ((bool)(((PageHeader)page)->pd_lower != 0));
}

PagePartition
BufferInitPage(buffer, pageSize)
	Buffer		buffer;
	PageSize	pageSize;
{
	BlockSize	blockSize;
/*	PartitionCount	partitions; */
	Page		page;
	int		i;		/* loop index */

	Assert(BufferIsValid(buffer));
	Assert(PageSizeIsValid(pageSize));

	blockSize = BufferGetBlockSize(buffer);
/*
	if (pageSize == MaximumPageSize) {
		pageSize = (PageSize)bufferSize;
	}
*/

/*
	partitions = bufferSize / pageSize;
	page = (Page)BufferGetPage(buffer);
	for (i = 0; i < partitions; i += 1) {
		PageSet	
		page += pageSize;
	}
*/
/*
	return (BufferSizeGetPagePartition(bufferSize, pageSize));
*/
}

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

PagePartition
BufferGetPagePartition(buffer)
	Buffer	buffer;
{
	/* Assert(BufferIsValid(buffer)); */

	return (CreatePagePartition(BufferGetBlockSize(buffer),
		BufferGetPageSize(buffer)));
}

Page
BufferGetPage(buffer, pageNumber)
	Buffer		buffer;
	PageNumber	pageNumber;
{
	Assert(pageNumber == FirstPageNumber);

	return ((Page)BufferGetBlock(buffer));
}

Page
BufferGetPageDebug(buffer, pageNumber)
	Buffer		buffer;
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

Page
BufferSimpleGetPage(buffer)
	Buffer		buffer;
{
	return (BufferGetPage(buffer, FirstPageNumber));
}

void
BufferSimpleInitPage(buffer)
	Buffer	buffer;
{
	PageInit((Page)BufferGetBlock(buffer), BufferGetBlockSize(buffer), 0);
}

void
PageInit(page, pageSize, specialSize)
	Page		page;
	PageSize	pageSize;
	SpecialSize	specialSize;
{
	Assert(pageSize == BLCKSZ);

	Assert(pageSize >
		specialSize + sizeof (PageHeaderData) - sizeof (ItemIdData));

	((PageHeader)page)->pd_lower = sizeof (PageHeaderData) - sizeof
		(ItemIdData);
	((PageHeader)page)->pd_upper = pageSize - specialSize;
	((PageHeader)page)->pd_special = pageSize - specialSize;

	OpaqueSetPageSize((Opaque)&((PageHeader)page)->opaque, pageSize);
	OpaqueSetInternalFragmentation((Opaque)&((PageHeader)page)->opaque,
		(InternalFragmentation)0);
}

OffsetIndex
PageGetMaxOffsetIndex(page)
	Page	page;
{
	return ((((PageHeader)page)->pd_lower - (sizeof (PageHeaderData) -
		sizeof (ItemIdData))) / sizeof (ItemIdData) - 1);
		/* XXX */
}

ItemId
PageGetItemId(page, offsetIndex)
	Page		page;
	OffsetIndex	offsetIndex;
{
	return (&((PageHeader)page)->pd_linp[offsetIndex]);
}

ItemId
PageGetFirstItemId(page)
	Page		page;
{
	return (PageGetItemId(page, (OffsetIndex)0));
}

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

uint16
PageGetSpecialSize(page)
	Page	page;
{
	return (PageGetPageSize(page) - ((PageHeader)page)->pd_special);
}

Pointer
PageGetSpecialPointer(page)
	Page	page;
{
	Assert(PageIsValid(page));
	/* XXX Assert(PageIsLocked(page)); */

	return ((char *)page + ((PageHeader)page)->pd_special);
}

/*
 * PageAddItem -- add an item to a page.  Notes on interface:
 *  If offsetNumber is valid, shuffle ItemId's down to make room
 *  to use it, if PageManagerShuffle is true.  If PageManagerShuffle is
 *  false, then overwrite the specified ItemId.  (PageManagerShuffle is
 *  true by default, and is modified by calling PageManagerModeSet.)
 *  If offsetNumber is not valid, then assign one by finding the first 
 *  one that is both unused and deallocated.
 *
 *  NOTE: If offsetNumber is valid, and PageManagerShuffle is true, it
 *  is assumed that there is room on the page to shuffle the ItemId's
 *  down by one.
 */
OffsetNumber
PageAddItem(page, item, size, offsetNumber, flags)
	Page		page;
	Item		item;
	Size		size;
	OffsetNumber	offsetNumber;
	ItemIdFlags	flags;
{
	register 	i;
	Size		alignedSize;
	Offset		lower;
	Offset		upper;
	ItemId		itemId;
	ItemId		fromitemId, toitemId;
	OffsetNumber limit;
	bool shuffled = false;

	/*
	if (!OffsetNumberIsValid(offsetNumber)) {
		offsetNumber = (int16)(2 + PageGetMaxOffsetIndex(page));
	} else {
		if (offsetNumber != (int16)(2 + PageGetMaxOffsetIndex(page))) {
			Assert(0);
		}
	}
	*/
	limit = 2 + PageGetMaxOffsetIndex(page);   /* 1st unalloc'd offsetNumber */

		/* if offsetNumber was passed in */
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
	if (offsetNumber == limit || shuffled == true)
		lower = ((PageHeader)page)->pd_lower + sizeof (ItemIdData);
	else	/* 
			 * offsetNumber != limit && shuffled == false, i.e.
			 * either overwriting or recycling an ItemId
			 */
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
/*	((PageHeader)page)->pd_nline++;
*/
}
/*
ReturnStatus
PageAddItem(page, item, size, offsetIndex, flags)
	Page		page;
	Item		item;
	Size		size;
	OffsetIndex	offsetIndex;
	ItemIdFlags	flags;
{
	Size		alignedSize;
	Offset		lower;
	Offset		upper;
	ItemId		itemId;

	Assert(offsetIndex == (OffsetIndex)(1 + PageGetMaxOffsetIndex(page)));

	alignedSize = LONGALIGN(size);

	lower = ((PageHeader)page)->pd_lower + sizeof (ItemIdData);
	upper = ((PageHeader)page)->pd_upper - alignedSize;
	itemId = &((PageHeader)page)->pd_linp[offsetIndex];
	(*itemId).lp_off = upper;
	(*itemId).lp_len = size;
	(*itemId).lp_flags = flags;
	bcopy(item, (char *)page + upper, size);
	((PageHeader)page)->pd_lower = lower;
	((PageHeader)page)->pd_upper = upper;
*/
/*	((PageHeader)page)->pd_nline++;
*/
/*
}
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

PageSize
PageGetPageSize(page)
	Page	page;
{
	return (OpaqueGetPageSize((Opaque)
		&((PageHeader)page)->opaque));
}

InternalFragmentation
PageGetInternalFragmentation(page)
	Page	page;
{
	return (OpaqueGetInternalFragmentation((Opaque)
		&((PageHeader)page)->opaque));
}

/*
ItemIndex
PageGetMaxItemIndex(page)
	Page	page;
{
	Assert(0);
}
*/

static
PageSize
OpaqueGetPageSize(opaque)
	Opaque	opaque;
{
	return ((PageSize)(1 << (1 + opaque->pageSize)));
}

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

static
InternalFragmentation
OpaqueGetInternalFragmentation(opaque)
	Opaque	opaque;
{
	return ((InternalFragmentation)opaque->fragmentation);
}

static
void
OpaqueSetInternalFragmentation(opaque, fragmentation)
	Opaque	opaque;
	InternalFragmentation	fragmentation;
{
	Assert(fragmentation >= 0);
	Assert(fragmentation <= MaxInternalFragmentation);

	opaque->fragmentation = fragmentation;
}

static
struct itemIdData {
	OffsetIndex offsetindex;
	ItemIdData itemiddata;
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

void
PageRepairFragmentation(page)
	Page	page;
{
    int i;
	struct itemIdData *itemidbase, *itemidptr;
	ItemId lp;
	int nline, nused;
	OffsetIndex offsetindex;
	int itemidcompare();
	Offset upper;
	Size alignedSize;

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

void
PageManagerModeSet(mode)
	PageManagerMode mode;
{
	if (mode == ShufflePageManagerMode)
		PageManagerShuffle = true;
	else if (mode == OverwritePageManagerMode)
		PageManagerShuffle = false;
}
