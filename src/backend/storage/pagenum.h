/*
 * pagenum.h --
 *	POSTGRES page number definitions.
 *
 * Identification:
 *	$Header$
 */

#ifndef	PageNumIncluded	/* Include this file only once. */
#define PageNumIncluded	1

#ifndef C_H
#include "c.h"
#endif

#include "part.h"

typedef uint16	PageNumber;

#define FirstPageNumber	0	/* page nubmer for the first page of a block */

typedef uint32	LogicalPageNumber;

#define InvalidLogicalPageNumber	0

/*
 * PageNumberIsValid --
 *	True iff the page number is valid.
 */
extern
bool
PageNumberIsValid ARGS((
	PageNumber	pageNumber,
	PagePartition	pagePartition
));

/*
 * LogicalPageNumberIsValid --
 *	True iff the logical page number is valid.
 */
extern
bool
PageNumberIsValid ARGS((
	PageNumber	pageNumber,
	PagePartition	pagePartition
));

#endif	/* !defined(PageNumIncluded) */
