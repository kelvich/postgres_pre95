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

#ifndef C_H
#include "c.h"
#endif

#ifndef	PAGE_H
# include "page.h"
#endif

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

#define PART_SYMBOLS \
	SymbolDecl(PagePartitionIsValid, "_PagePartitionIsValid"), \
	SymbolDecl(CreatePagePartition, "_CreatePagePartition"), \
	SymbolDecl(PagePartitionGetPagesPerBlock, "_PagePartitionGetPagesPerBlock")

#endif	/* !defined(PartIncluded) */
