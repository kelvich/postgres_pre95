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

#ifndef C_H
#include "c.h"
#endif

#ifndef	PART_H
# include "part.h"
#endif

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
LogicalPageNumberIsValid ARGS((
	PageNumber	pageNumber,
	PagePartition	pagePartition
));

#define PAGENUM_SYMBOLS \
	SymbolDecl(PageNumberIsValid, "_PageNumberIsValid"), \
	SymbolDecl(LogicalPageNumberIsValid, "_LogicalPageNumberIsValid")

#endif	/* !defined(PageNumIncluded) */
