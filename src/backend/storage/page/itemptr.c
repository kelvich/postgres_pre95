
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
 * itemptr.c --
 *	POSTGRES disk item pointer code.
 */

#include "c.h"

#include "block.h"
#include "off.h"

#include "page.h"

#include "itemptr.h"
#include "internal.h"

RcsId("$Header$");

bool
ItemPointerIsUserDefined(pointer)
	ItemPointer	pointer;
{
	if (!PointerIsValid(pointer)) {
		return (false);
	} else {
		return ((bool)(pointer->blockData.data[0] & 0x8000)); /* XXX */
	}
}

bool
ItemPointerIsValid(pointer)
	ItemPointer	pointer;
{
	Assert(!ItemPointerIsUserDefined(pointer));

	if (!PointerIsValid(pointer)) {
		return (false);
	} else {
		return ((bool)(pointer->positionData != 0));
	}
}

BlockNumber
ItemPointerGetBlockNumber(pointer)
	ItemPointer	pointer;
{
	Assert(!ItemPointerIsUserDefined(pointer));

	return (BlockIdGetBlockNumber(&pointer->blockData));
}

PageNumber
ItemPointerGetPageNumber(pointer, partition)
	ItemPointer	pointer;
	PagePartition	partition;
{
	Assert(!ItemPointerIsUserDefined(pointer));

	return (PositionIdGetPageNumber(&pointer->positionData, partition));
}

OffsetNumber
ItemPointerGetOffsetNumber(pointer, partition)
	ItemPointer	pointer;
	PagePartition	partition;
{
	Assert(!ItemPointerIsUserDefined(pointer));

	return (PositionIdGetOffsetNumber(&pointer->positionData, partition));
}

OffsetIndex
ItemPointerGetOffsetIndex(pointer, partition)
	ItemPointer	pointer;
	PagePartition	partition;
{
	Assert(!ItemPointerIsUserDefined(pointer));

	return (ItemPointerGetOffsetNumber(pointer, partition) - 1);
}

void
ItemPointerSet(pointer, partition, blockNumber, pageNumber, offsetNumber)
	ItemPointer	pointer;
	PagePartition	partition;
	BlockNumber	blockNumber;
	PageNumber	pageNumber;
	OffsetNumber	offsetNumber;
{
	Assert(PointerIsValid(pointer));

	BlockIdSet(&pointer->blockData, blockNumber);
	PositionIdSet(&pointer->positionData, partition, pageNumber,
		offsetNumber);
}

void
ItemPointerCopy(fromPointer, toPointer)
	ItemPointer	fromPointer;
	ItemPointer	toPointer;
{
	Assert(PointerIsValid(toPointer));
	Assert(PointerIsValid(fromPointer));

	*toPointer = *fromPointer;
}

void
ItemPointerSetInvalid(pointer)
	ItemPointer	pointer;
{
	Assert(PointerIsValid(pointer));

	pointer->blockData.data[0] &= 0x7fff;	/* XXX byte ordering */
	pointer->positionData = 0;
}

PageNumber
ItemPointerSimpleGetPageNumber(pointer)
	ItemPointer	pointer;
{
	return (ItemPointerGetPageNumber(pointer, SinglePagePartition));
}


OffsetNumber
ItemPointerSimpleGetOffsetNumber(pointer)
	ItemPointer	pointer;
{
	return (ItemPointerGetOffsetNumber(pointer, SinglePagePartition));
}

OffsetIndex
ItemPointerSimpleGetOffsetIndex(pointer)
	ItemPointer	pointer;
{
	return (ItemPointerGetOffsetIndex(pointer, SinglePagePartition));
}

void
ItemPointerSimpleSet(pointer, blockNumber, offsetNumber)
	ItemPointer	pointer;
	BlockNumber	blockNumber;
	OffsetNumber	offsetNumber;
{
	ItemPointerSet(pointer, SinglePagePartition, blockNumber,
		FirstPageNumber, offsetNumber);
}

bool
ItemPointerEquals(pointer1, pointer2)
    ItemPointer pointer1, pointer2;
{
    if (ItemPointerGetBlockNumber(pointer1) ==
        ItemPointerGetBlockNumber(pointer2) &&
        ItemPointerGetOffsetNumber(pointer1, SinglePagePartition) ==
        ItemPointerGetOffsetNumber(pointer2, SinglePagePartition))
            return(true);
    else
        return(false);
}

#ifndef	POSTMASTER

LogicalPageNumber
ItemPointerGetLogicalPageNumber(pointer, partition)
	ItemPointer	pointer;
	PagePartition	partition;
{
	int16	pagesPerBlock;	/* XXX use a named type */

	Assert(!ItemPointerIsUserDefined(pointer));

	pagesPerBlock = PagePartitionGetPagesPerBlock(partition);
	if (pagesPerBlock == 1) {
		return (1 + ItemPointerGetBlockNumber(pointer));
	}
	return (1 + ItemPointerGetPageNumber(pointer, partition) +
		pagesPerBlock * ItemPointerGetBlockNumber(pointer));
}

void
ItemPointerSetLogicalPageNumber(pointer, partition, pageNumber)
	ItemPointer		pointer;
	PagePartition		partition;
	LogicalPageNumber	pageNumber;
{
	int16	pagesPerBlock;	/* XXX use a named type */

	Assert(PointerIsValid(pointer));
	Assert(LogicalPageNumberIsValid(pageNumber));

	pagesPerBlock = PagePartitionGetPagesPerBlock(partition);

	if (pagesPerBlock == 1) {
		ItemPointerSimpleSet(pointer,
			(LogicalPageNumber)(-1 + pageNumber),
			InvalidOffsetNumber);
	} else {
		ItemPointerSet(pointer, partition,
			(LogicalPageNumber)(-1 + pageNumber) / pagesPerBlock,
			(LogicalPageNumber)(-1 + pageNumber) % pagesPerBlock,
			InvalidOffsetNumber);
	}
}
#endif	/* !defined(POSTMASTER) */
