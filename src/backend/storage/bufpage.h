/* ----------------------------------------------------------------
 *   FILE
 *	bufpage.h
 *
 *   DESCRIPTION
 *	Standard POSTGRES buffer page definitions.
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef	BufPageIncluded		/* Include this file only once */
#define BufPageIncluded	1

#define BUFPAGE_H	"$Header$"

#include "tmp/c.h"

#include "machine.h"		/* for BLCKSZ */

#include "storage/buf.h"
#include "storage/item.h"
#include "storage/itemid.h"
#include "storage/itemptr.h"
#include "storage/page.h"
#include "storage/part.h"


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

typedef enum {
    ShufflePageManagerMode,
    OverwritePageManagerMode
} PageManagerMode;

/* ----------------
 *	stuff from internal_page.h
 * ----------------
 */
#define MaxInternalFragmentation	((1 << 12) - 1)

typedef struct OpaqueData {
	bits16	pageSize:4,		/* page size */
		fragmentation:12;	/* internal fragmentation */
} OpaqueData;

typedef OpaqueData	*Opaque;

/* ----------------
 *	misc support macros
 * ----------------
 */

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

/* ----------------------------------------------------------------
 *			page support macros
 * ----------------------------------------------------------------
 */
/*
 * PageIsValid -- This is defined in h/page.h.
 */

/*
 * PageSizeIsValid --
 *	True iff the page size is valid.
 *
 * XXX currently all page sizes are "valid" but we only actually
 *     use BLCKSZ.
 */
#define PageSizeIsValid(pageSize) \
    (true)

/*
 * PageIsUsed --
 *	True iff the page size is used.
 *
 * Note:
 *	Assumes page is valid.
 */
#define PageIsUsed(page) \
    (AssertMacro(PageIsValid(page)) ? \
     ((bool) (((PageHeader) (page))->pd_lower != 0)) : false)

/*
 * PageIsEmpty --
 *	returns true iff no itemid has been allocated on the page
 */
#define PageIsEmpty(page) \
    (((PageHeader) (page))->pd_lower == \
     (sizeof(PageHeaderData) - sizeof(ItemIdData)) ? true : false)

/*
 * PageGetItemId --
 *	Returns an item identifier of a page.
 */
#define PageGetItemId(page, offsetIndex) \
    ((ItemId) (&((PageHeader) (page))->pd_linp[ offsetIndex ]))

/*
 * PageGetFirstItemId --
 *	Returns the first object identifier on a page.
 */
#define PageGetFirstItemId(page) \
    PageGetItemId(page, (OffsetIndex)0)

/* ----------------
 *	opaque macros for PageGetPageSize, PageGetInternalFragmentation
 * ----------------
 */
#define OpaqueGetPageSize(opaque) \
    ((PageSize) (1 << (1 + (opaque)->pageSize)))

#define OpaqueGetInternalFragmentation(opaque) \
    ((InternalFragmentation) (opaque)->fragmentation)

/*
 * PageGetPageSize --
 *	Returns the page size of a page.
 */
#define PageGetPageSize(page) \
    ((PageSize) (OpaqueGetPageSize((Opaque) &((PageHeader) (page))->opaque)))

/*
 * PageGetInternalFragmentation --
 *	Returns the internal fragmentation of a page.
 */
#define PageGetInternalFragmentation(page) \
    ((InternalFragmentation) \
     (OpaqueGetInternalFragmentation((Opaque) &((PageHeader) (page))->opaque)))

/* ----------------
 *	page special data macros
 * ----------------
 */
/*
 * PageGetSpecialSize --
 *	Returns size of special space on a page.
 *
 * Note:
 *	Assumes page is locked.
 */
#define PageGetSpecialSize(page) \
    ((uint16) (PageGetPageSize(page) - ((PageHeader)page)->pd_special))

/*
 * PageGetSpecialPointer --
 *	Returns pointer to special space on a page.
 *
 * Note:
 *	Assumes page is locked.
 */
#define PageGetSpecialPointer(page) \
    (AssertMacro(PageIsValid(page)) ? \
     (Pointer) ((char *) (page) + ((PageHeader) (page))->pd_special) \
     : (Pointer) 0)

/* ----------------------------------------------------------------
 *		      buffer page support macros
 * ----------------------------------------------------------------
 */
/*
 * BufferInitPage --
 *	Initializes the logical pages associated with a buffer.
 *
 * XXX does nothing at present
 */
#define BufferInitPage(buffer, pageSize) \
    Assert(BufferIsValid(buffer)); \
    Assert(PageSizeIsValid(pageSize)); \
    (void) BufferGetBlockSize(buffer)

/*
 * BufferGetPagePartition --
 *	Returns the page partition for a buffer.
 *
 * Note:
 *	Assumes buffer is valid.
 *	Assumes buffer uses the standard page layout.
 */
#define BufferGetPagePartition(buffer) \
    CreatePagePartition(BufferGetBlockSize(buffer), \
			BufferGetPageSize(buffer))

/*
 * BufferSimpleGetPage --
 *	Returns the "simple" page associated with a buffer.
 */
#define BufferSimpleGetPage(buffer) \
    BufferGetPage(buffer, FirstPageNumber)

/*
 * BufferSimpleInitPage --
 *	Initialize the "simple" physical page associated with a buffer
 */
#define BufferSimpleInitPage(buffer) \
    PageInit((Page)BufferGetBlock(buffer), BufferGetBlockSize(buffer), 0)

/* ----------------
 *	misc macros.  these aren't currently used by anything
 * ----------------
 */
/*
 * PagePartitionGetNumberOfPageBits --
 *	Returns the number of bits needed to represent a page.
 */
#define PagePartitionGetNumberOfPageBits(partition) (partition)

/*
 * PagePartitionGetNumberOfOffsetBits --
 *	Returns the number of bits needed to represent a offset.
 */
#define PagePartitionGetNumberOfOffsetBits(partition) \
    (16 - PagePartitionGetNumberOfPageBits(partition))

/*
 * PageNumberMask --
 *	Mask for a page number.
 */
#define PageNumberMask(partition) \
    (0xffff & (0xffff << PagePartitionGetNumberOfOffsetBits(partition)))

/*
 * OffsetNumberMask --
 *	Mask for an offset number.
 */
#define OffsetNumberMask(partition) \
    (0xffff >> PagePartitionGetNumberOfPageBits(partition))

/* ----------------------------------------------------------------
 *	extern declarations
 * ----------------------------------------------------------------
 */
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
extern
OffsetNumber    
PageAddItem ARGS((
	Page		page,
	Item		item,
	Size		size,
	OffsetIndex	offsetIndex,
	ItemIdFlags	flags
));

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
 * PageGetTempPage --
 *	Get a temporary page in local memory for special processing
 */
extern
Page
PageGetTempPage ARGS((
	Page	oldPage,
	Size	specialSize
));

/*
 * PageRestoreTempPage --
 *	Copy temporary page back to permanent page after special processing
 *	and release the temporary page.
 */
extern
void
PageRestoreTempPage ARGS((
	Page	tempPage,
	Page	oldPage
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
 *
 *   Sets mode to either: ShufflePageManagerMode (the default) or
 *   OverwritePageManagerMode.  For use by access methods code
 *   for determining semantics of PageAddItem when the offsetNumber
 *   argument is passed in.
 */
extern
void
PageManagerModeSet ARGS((
	PageManagerMode	mode
));

/*
 * BufferGetPage --
 *	Returns the page associated with a buffer.
 */
extern
Page
BufferGetPage ARGS((
    Buffer	buffer,
    PageNumber	pageNumber
));		    

#endif	/* !defined(BufPageIncluded) */
