/*
 * part.h --
 *	POSTGRES "partition" definitions.
 *
 * Identification:
 *	$Header$
 */

#ifndef	PartIncluded	/* Include this file only once. */
#define PartIncluded	1

#include "c.h"

#include "page.h"

typedef uint32	PagePartition;	/* subpage partition indicator */

#define SinglePagePartition	0	/* partition when 1 page per block */

/*
 * PagePartitionIsValid --
 *	True iff the page partition is valid.
 */
extern
bool
PagePartitionIsValid ARGS((
	PagePartition	partition
));

/*
 * CreatePagePartition --
 *	Returns a new page partition.
 */
extern
PagePartition
CreatePagePartition ARGS((
	BlockSize	blockSize,
	PageSize	pageSize
));

/*
 * PagePartitionGetPagesPerBlock --
 *	Returns the count pages for the disk block associated with
 *	this page partition.
 */
extern
Count
PagePartitionGetPagesPerBlock ARGS((
	PagePartition	partition
));

#endif	/* !defined(PartIncluded) */
