/*
 * part.c --
 *	POSTGRES "partition" code.
 */

#include "c.h"

#include "block.h"
#include "bufpage.h"	/* XXX for PageSize */
#include "off.h"

#include "part.h"
#include "internal.h"

RcsId("$Header$");

bool
PagePartitionIsValid(partition)
	PagePartition	partition;
{
	return ((bool)(partition != InvalidPagePartition));
}

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
