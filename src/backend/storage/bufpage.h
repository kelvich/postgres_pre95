/*
 * bufpage.h --
 *	Standard POSTGRES buffer page definitions.
 */

#ifndef	BufPageIncluded		/* Include this file only once */
#define BufPageIncluded	1

/*
 * Identification:
 */
#define BUFPAGE_H	"$Header$"

#ifndef C_H
#include "c.h"
#endif

#ifndef	BUF_H
# include "buf.h"
#endif

#ifndef	ITEM_H
# include "item.h"
#endif
#ifndef	ITEMID_H
# include "itemid.h"
#endif
#ifndef	ITEMPTR_H
# include "itemptr.h"
#endif
#ifndef	MACHINE_H
# include "machine.h"		/* for BLCKSZ */
#endif
#ifndef	PAGE_H
# include "page.h"
#endif
#ifndef	PART_H
# include "part.h"
#endif

#include "status.h"

typedef uint16		LocationIndex;

typedef Size		PageSize;
typedef uint16		SpecialSize;	/* size of special space on a page */
typedef uint16		PageFreeSpace;	/* size of free space on a page ??? */

typedef uint16	InternalFragmentation;	/* size of internal fragmentation */

typedef struct PageHeaderData {	/* disk page organization */
	LocationIndex	pd_lower;	/* offset to start of free space */
	LocationIndex	pd_upper;	/* offset to end of free space */
	LocationIndex	pd_special;	/* offset to start of special space */
	uint16		opaque;		/* miscellaneous information */
	ItemIdData	pd_linp[1];	/* line pointers */
} PageHeaderData;

typedef PageHeaderData	*PageHeader;

typedef struct ItemContinuationData {	/* tuple continuation structure */
	ItemPointerData	itemPointerData;
	uint16	filler;
	char	itemData[1];	/* data in this tuple segment */
} ItemContinuationData;

typedef ItemContinuationData	*ItemContinuation;

/*
 *	FINDTUP(DP, LP)
 *	struct	dpage	*DP;
 *	int		LP;
 */
#define FINDTUP(DP, LP)\
	((HeapTuple)((char *)(DP) + (DP)->pd_linp[LP].lp_off + \
		((DP)->pd_linp[LP].lp_flags & LP_DOCNT) ? TCONTPAGELEN : 0))

/*
 *	TSPACELEFT(DP)	- free space available for a tuple
 *	struct	dpage	*DP;
 *
 *	Should maybe include the lock TID in calculation?
 *	(For MAXTUPLEN too?)
 *	Check proper alignment of the dp_last - dp_lower.  May
 *	have a problem.
 */
#define TSPACELEFT(DP) ((int)((DP)->pd_upper - (DP)->pd_lower) \
			- sizeof (ItemIdData))

#define MAXTUPLEN	(BLCKSZ - sizeof (PageHeaderData))	/* XXX */
#define TCONTPAGELEN	(4 * sizeof (int16))
	/* 3 for the ItemId and 1 for alignment */
#define MAXTCONTLEN	(MAXTUPLEN - TCONTPAGELEN)

/*
 * PageIsValid --
 *	True iff the page is valid.
 *
 * Note:
 *	This is defined in h/page.h.
 */

/*
 * PageSizeIsValid --
 *	True iff the page size is valid.
 */
extern
bool
PageSizeIsValid ARGS((
	PageSize	pageSize
));

/*
 * PageIsUsed --
 *	True iff the page size is used.
 *
 * Note:
 *	Assumes page is valid.
 */
extern
bool
PageIsUsed ARGS((
	Page	page
));

/*
 * BufferInitPage --
 *	Initializes the logical pages associated with a buffer.
 */
extern
PagePartition
BufferInitPage ARGS((
	Buffer		buffer,
	PageSize	pageSize
));

/*
 * BufferGetPage --
 *	Returns the page associated with a buffer.
 */
extern
Page
BufferGetPage ARGS((
	Buffer		buffer,
	PageNumber	pageNumber
));

/*
 * BufferGetPageDebug --
 *	Returns the page associated with a buffer.
 *      (and asserts that the page header is valid)
 */
extern
Page
BufferGetPageDebug ARGS((
	Buffer		buffer,
	PageNumber	pageNumber
));

/*
 * BufferGetPageSize --
 *	Returns the page size within a buffer.
 *
 * Note:
 *	Assumes buffer is valid.
 *	Assumes buffer uses the standard page layout.
 */
extern
PageSize
BufferGetPageSize ARGS((
	Buffer		buffer
));

/*
 * BufferGetPagePartition --
 *	Returns the page partition for a buffer.
 *
 * Note:
 *	Assumes buffer is valid.
 *	Assumes buffer uses the standard page layout.
 */
extern
PagePartition
BufferGetPagePartition ARGS((
	Buffer		buffer
));

/*
 * BufferSimpleGetPage --
 *	Returns the "simple" page associated with a buffer.
 */
extern
Page
BufferSimpleGetPage ARGS((
	Buffer		buffer
));

/*
 * PageInit --
 *	Initializes the contents of a page.
 */
extern
void
PageInit ARGS((
	Page		page,
	PageSize	pageSize,
	SpecialSize	specialSize
));

/*
 * BufferSimpleInitPage --
 *	Initialize the "simple" physical page associated with a buffer
 */
extern
void
BufferSimpleInitPage ARGS((
	Buffer	buffer
));

/*
 * PageGetMaxOffsetIndex --
 *	Returns the maximum offset index used by the given page.
 */
extern
OffsetIndex
PageGetMaxOffsetIndex ARGS((
	Page	page
));

/*
 * PageGetItemId --
 *	Returns an item identifier of a page.
 */
extern
ItemId
PageGetItemId ARGS((
	Page		page,
	OffsetIndex	offsetIndex
));

/*
 * PageGetFirstItemId --
 *	Returns the first object identifier on a page.
 */
extern
ItemId
PageGetFirstItemId ARGS((
	Page	page
));

/*
 * PageGetItem --
 *	Retrieves an item on the given page.
 *
 * Note:
 *	This does change the status of any of the resources passed.
 *	The semantics may change in the future.
 */
extern
Item
PageGetItem ARGS((
	Page	page,
	ItemId	itemId
));

/*
 * PageGetSpecialSize --
 *	Returns size of special space on a page.
 *
 * Note:
 *	Assumes page is locked.
 */
extern
uint16
PageGetSpecialSize ARGS((
	Page	page
));

/*
 * PageGetSpecialPointer --
 *	Returns pointer to special space on a page.
 *
 * Note:
 *	Assumes page is locked.
 */
extern
Pointer
PageGetSpecialPointer ARGS((
	Page	page
));

/*
 * PageAddItem --
 *	Adds item to the given page.
 *
 * Note:
 *	This assumes that the item resides on a single page.
 *	It is the responsiblity of the caller to act appropriately
 *	if this is not true.
 *	
 *	The semantics may change in the future.
 */
extern
OffsetNumber
PageAddItem ARGS((
	Page		page,
	Item		item,
	Size		size,
	OffsetNumber	offsetNumber,
	ItemIdFlags	flags
));

/*
 * PageAddItem --
 *	Adds item to the given page.
 *
 * Note:
 *	This does not assume that the item resides on a single page.
 *	It is the responsiblity of the caller to act appropriately
 *	depending on this fact.  The "pskip" routines provide a
 *	friendlier interface, in this case.
 *	
 *	This does change the status of any of the resources passed.
 *	The semantics may change in the future.
 *
 *	This routine should probably be combined with others?
 */
/*
extern
ReturnStatus
PageAddItem ARGS((
	Page		page,
	Item		item,
	Size		size,
	OffsetIndex	offsetIndex,
	ItemIdFlags	flags
));
*/

/*
 * PageRemoveItem --
 *	Removes an item from the given page.
 */
extern
ReturnStatus
PageRemoveItem ARGS((
	Page		page,
	ItemId		itemId
));

/*
 * PageRepairFragmentation --
 *	Frees fragmented space on a page.
 */
extern
void
PageRepairFragmentation ARGS((
	Page	page
));

/*
 * PageGetPageSize --
 *	Returns the page size of a page.
 */
extern
PageSize
PageGetPageSize ARGS((
	Page	page
));

/*
 * PageGetInternalFragmentation --
 *	Returns the internal fragmentation of a page.
 */
extern
InternalFragmentation
PageGetInternalFragmentation ARGS((
	Page	page
));

/*
 * PageGetFreeSpace --
 *	Returns the size of the free (allocatable) space on a page.
 */
extern
PageFreeSpace
PageGetFreeSpace ARGS((
	Page	page
));

/*
 * PageManagerModeSet --
 *  Sets mode to either: ShufflePageManagerMode (the default) or
 *   OverwritePageManagerMode.  For use by access methods code
 *   for determining semantics of PageAddItem when the offsetNumber
 *   argument is passed in.
 */
typedef enum {
	ShufflePageManagerMode,
	OverwritePageManagerMode
} PageManagerMode;

extern
void
PageManagerModeSet ARGS((
	PageManagerMode	mode
));

/*
 * PageGetMaxItemIndex --
 *	Returns the index of the maximum disk item currently allocated.
 */
/*
extern
ItemIndex
PageGetMaxItemIndex ARGS((
	Page	page
));
*/

#endif	/* !defined(BufPageIncluded) */
