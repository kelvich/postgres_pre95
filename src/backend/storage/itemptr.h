/*
 * itemptr.h --
 *	POSTGRES disk item pointer definitions.
 */

#ifndef	ItemPtrIncluded		/* Include this file only once */
#define ItemPtrIncluded	1

/*
 * Identification:
 */
#define ITEMPTR_H	"$Header$"

#ifndef C_H
#include "c.h"
#endif

#ifndef	BLOCK_H
# include "block.h"
#endif
#ifndef	OFF_H
# include "off.h"
#endif
#ifndef	PAGENUM_H
# include "pagenum.h"
#endif
#ifndef PAGE_H
# include "page.h"
#endif
#ifndef	PART_H
# include "part.h"
#endif
#ifndef	POS_H
# include "pos.h"
#endif

typedef struct ItemPointerData {
	BlockIdData	blockData;
	PositionIdData	positionData;
} ItemPointerData;

typedef ItemPointerData	*ItemPointer;

/*
 * ItemPointerIsUserDefined --
 *	True iff the disk item pointer is in a user defined format.
 */
extern
bool
ItemPointerIsUserDefined ARGS((
	ItemPointer	pointer
));

/*
 * ItemPointerIsValid --
 *	True iff the disk item pointer is not NULL.
 *
 * Note:
 *	Assumes that the disk item pointer is not in a user defined format.
 */
extern
bool
ItemPointerIsValid ARGS((
	ItemPointer	pointer
));

/*
 * ItemPointerGetBlockNumber --
 *	Returns the block number of a disk item pointer.
 *
 * Note:
 *	Assumes that the disk item pointer is not in a user defined format.
 */
extern
BlockNumber
ItemPointerGetBlockNumber ARGS((
	ItemPointer	pointer
));

/*
 * ItemPointerGetPageNumber --
 *	Returns the page number of a disk item pointer.
 *
 * Note:
 *	Assumes that the disk item pointer is not in a user defined format.
 */
extern
PageNumber
ItemPointerGetPageNumber ARGS((
	ItemPointer	pointer,
	PagePartition	partition
));

/*
 * ItemPointerGetOffsetNumber --
 *	Returns the offset number of a disk item pointer.
 *
 * Note:
 *	Assumes that the disk item pointer is not in a user defined format.
 */
extern
OffsetNumber
ItemPointerGetOffsetNumber ARGS((
	ItemPointer	pointer,
	PagePartition	partition
));

/*
 * ItemPointerGetOffsetIndex --
 *	Returns the offset index of a disk item pointer.
 *
 * Note:
 *	Assumes that the disk item pointer is not in a user defined format.
 */
extern
OffsetIndex
ItemPointerGetOffsetIndex ARGS((
	ItemPointer	pointer,
	PagePartition	partition
));

/*
 * ItemPointerSet --
 *	Sets a disk item pointer to the specified block, page, and offset.
 *
 * Note:
 *	Assumes that the disk item pointer is not NULL.
 */
extern
void
ItemPointerSet ARGS((
	ItemPointer	pointer,
	PagePartition	partition,
	BlockNumber	blockNumber,
	PageNumber	pageNumber,
	OffsetNumber	offsetNumber
));

/*
 * ItemPointerCopy --
 *	Copies the contents of one disk item pointer to another.
 *
 * Note:
 *	Assumes that the disk item pointer is not NULL.
 */
extern
void
ItemPointerCopy ARGS((
	ItemPointer	fromPointer,
	ItemPointer	toPointer
));

/*
 * ItemPointerSetInvalid --
 *	Sets a disk item pointer to be invalid.
 *
 * Note:
 *	Assumes that the disk item pointer is not NULL.
 */
extern
void
ItemPointerSetInvalid ARGS((
	ItemPointer	pointer
));

/*
 * ItemPointerSimpleGetPageNumber --
 *	Returns the page number of a "simple" disk item pointer.
 *
 * Note:
 *	Assumes that the disk item pointer is not in a user defined format.
 */
extern
PageNumber
ItemPointerSimpleGetPageNumber ARGS((
	ItemPointer	pointer
));

/*
 * ItemPointerSimpleGetOffsetNumber --
 *	Returns the offset number of a "simple" disk item pointer.
 *
 * Note:
 *	Assumes that the disk item pointer is not in a user defined format.
 */
extern
OffsetNumber
ItemPointerSimpleGetOffsetNumber ARGS((
	ItemPointer	pointer
));

/*
 * ItemPointerSimpleGetOffsetIndex --
 *	Returns the offset index of a "simple" disk item pointer.
 *
 * Note:
 *	Assumes that the disk item pointer is not in a user defined format.
 */
extern
OffsetIndex
ItemPointerSimpleGetOffsetIndex ARGS((
	ItemPointer	pointer
));

/*
 * ItemPointerSimpleSet --
 *	Sets a disk item pointer to the specified block and offset.
 *
 * Note:
 *	Assumes that the disk item pointer is not NULL.
 */
extern
void
ItemPointerSimpleSet ARGS((
	ItemPointer	pointer,
	BlockNumber	blockNumber,
	OffsetNumber	offsetNumber
));

/*
 * ItemPointerEquals --
 *  Returns true if both item pointers point to the same item, 
 *   otherwise returns false.
 *
 * Note:
 *  Assumes that the disk item pointers are not NULL.
 */
extern
bool
ItemPointerEquals ARGS((
	ItemPointer	pointer1,
	ItemPointer	pointer2
));

/*
 * ItemPointerGetLogicalPageNumber --
 *	Returns the logical page number in a disk item pointer.
 *
 * Note:
 *	Assumes that the disk item pointer is not in a user defined format.
 */
extern
LogicalPageNumber
ItemPointerGetLogicalPageNumber ARGS((
	ItemPointer	pointer,
	PagePartition	partition
));

/*
 * ItemPointerSetLogicalPageNumber --
 *	Sets a disk item pointer to the specified logical page number.
 *
 * Note:
 *	Assumes that the disk item pointer is not NULL.
 */
extern
void
ItemPointerSetLogicalPageNumber ARGS((
	ItemPointer		pointer,
	PagePartition		partition,
	LogicalPageNumber	pageNumber
));

#endif	/* !defined(ItemPtrIncluded) */
