/*
 * pagenum.h --
 *	POSTGRES page number definitions.
 */

#ifndef	PageNumIncluded		/* Include this file only once */
#define PageNumIncluded	1

/*
 * Identification:
 */
#define PAGENUM_H	"$Header$"

#include "tmp/c.h"
#include "storage/page.h"
#include "storage/part.h"

typedef uint16	PageNumber;

#define FirstPageNumber	0	/* page nubmer for the first page of a block */

typedef uint32	LogicalPageNumber;

#define InvalidLogicalPageNumber	0

/*
 * PageNumberIsValid --
 *	True iff the page number is valid.
 */
#define PageNumberIsValid(pageNumber, partition) \
    ((bool)((pageNumber) < PagePartitionGetPagesPerBlock(partition)))

/*
 * LogicalPageNumberIsValid --
 *	True iff the logical page number is valid.
 */
#define LogicalPageNumberIsValid(pageNumber) \
    ((bool)((pageNumber) != InvalidLogicalPageNumber))


#endif	/* !defined(PageNumIncluded) */
