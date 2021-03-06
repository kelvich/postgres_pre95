/*
 * part.c --
 *	POSTGRES "partition" code.
 */

#include "tmp/c.h"

#include "storage/block.h"
#include "storage/bufpage.h"	/* XXX for PageSize */
#include "storage/off.h"
#include "storage/part.h"

RcsId("$Header$");

/*
 * PagePartitionIsValid is now a macro in part.h -cim 4/27/91
 */

PagePartition
CreatePagePartition(blockSize, pageSize)
	BlockSize	blockSize;
	PageSize	pageSize;
{
	Assert(BlockSizeIsValid(blockSize));
	Assert(PageSizeIsValid(pageSize));

	if (pageSize > blockSize || blockSize % pageSize != 0) {
		return (InvalidPagePartition);
	}
	{
		Count		numberOfPages = blockSize / pageSize;
		PagePartition	partition = 0;

		while (numberOfPages > 1) {
			partition += 1;
			numberOfPages /= 2;
		}
		return (partition);
	}
}

Count
PagePartitionGetPagesPerBlock(partition)
	PagePartition	partition;
{
	Count	numberOfPages = 1;

	while (partition > 0) {
		numberOfPages *= 2;
		partition -= 1;
	}
	return (numberOfPages);
}
