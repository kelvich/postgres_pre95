
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
