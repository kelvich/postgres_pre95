/*
 * itemptr.c --
 *	POSTGRES disk item pointer code.
 */

#include "tmp/c.h"

#include "storage/block.h"
#include "storage/off.h"
#include "storage/page.h"
#include "storage/itemptr.h"
#include "storage/bufpage.h"

RcsId("$Header$");

bool
ItemPointerEquals(pointer1, pointer2)
    ItemPointer pointer1;
    ItemPointer	pointer2;
{
    if (ItemPointerGetBlockNumber(pointer1) ==
        ItemPointerGetBlockNumber(pointer2) &&
        ItemPointerGetOffsetNumber(pointer1, SinglePagePartition) ==
        ItemPointerGetOffsetNumber(pointer2, SinglePagePartition))
	return(true);
    else
        return(false);
}

LogicalPageNumber
ItemPointerGetLogicalPageNumber(pointer, partition)
    ItemPointer		pointer;
    PagePartition	partition;
{
    int16	pagesPerBlock;	/* XXX use a named type */
    
    Assert(ItemPointerIsValid(pointer));
    
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
    PagePartition	partition;
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
