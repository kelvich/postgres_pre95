/*
 * part.h --
 *	POSTGRES "partition" definitions.
 */

#ifndef	PartIncluded		/* Include this file only once */
#define PartIncluded	1

/*
 * Identification:
 */
#define PART_H	"$Header$"

#include "tmp/c.h"
#include "storage/page.h"

typedef uint32	PagePartition;	/* subpage partition indicator */

#define SinglePagePartition	0	/* partition when 1 page per block */

#define InvalidPagePartition	((PagePartition) 0)

/*
 * PagePartitionIsValid --
 *	True iff the page partition is valid.
 */
#define PagePartitionIsValid(partition) \
    ((bool) ((partition) != InvalidPagePartition))

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
