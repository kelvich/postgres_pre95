/* ----------------------------------------------------------------
 *   FILE
 *	itemptr.h
 *
 *   DESCRIPTION
 *	POSTGRES disk item pointer definitions.
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef	ItemPtrIncluded		/* Include this file only once */
#define ItemPtrIncluded	1

#define ITEMPTR_H	"$Header$"

#include "tmp/c.h"

#include "storage/block.h"
#include "storage/off.h"
#include "storage/pagenum.h"
#include "storage/page.h"
#include "storage/part.h"
#include "storage/pos.h"

typedef struct ItemPointerData {
	BlockIdData	blockData;
	PositionIdData	positionData;
} ItemPointerData;

typedef ItemPointerData	*ItemPointer;

/* ----------------
 *	support macros
 * ----------------
 */
/*
 * ItemPointerIsUserDefined --
 *	True iff the disk item pointer is in a user defined format.
 */
#define ItemPointerIsUserDefined(pointer) \
    ((bool) ((pointer)->blockData.data[0] & 0x8000))

/*
 * ItemPointerIsValid --
 *	True iff the disk item pointer is not NULL.
 *
 * Note:
 *	Assumes that the disk item pointer is not in a user defined format.
 */
#define ItemPointerIsValid(pointer) \
    ((bool) (PointerIsValid(pointer) && \
	     (! ItemPointerIsUserDefined(pointer)) && \
	     ((pointer)->positionData != 0)))

/*
 * ItemPointerGetBlockNumber --
 *	Returns the block number of a disk item pointer.
 *
 * Note:
 *	Assumes that the disk item pointer is not in a user defined format.
 */
#define ItemPointerGetBlockNumber(pointer) \
    (AssertMacro(ItemPointerIsValid(pointer)) ? \
     BlockIdGetBlockNumber(&(pointer)->blockData) : (BlockNumber) 0)

/*
 * ItemPointerGetPageNumber --
 *	Returns the page number of a disk item pointer.
 *
 * Note:
 *	Assumes that the disk item pointer is not in a user defined format.
 */
#define ItemPointerGetPageNumber(pointer, partition) \
    (AssertMacro(ItemPointerIsValid(pointer)) ? \
     PositionIdGetPageNumber(&(pointer)->positionData, partition) : \
     (PageNumber) 0)

/*
 * ItemPointerGetOffsetNumber --
 *	Returns the offset number of a disk item pointer.
 *
 * Note:
 *	Assumes that the disk item pointer is not in a user defined format.
 */
#define ItemPointerGetOffsetNumber(pointer, partition) \
    (AssertMacro(ItemPointerIsValid(pointer)) ? \
     PositionIdGetOffsetNumber(&(pointer)->positionData, partition) : \
     (OffsetNumber) 0)

/*
 * ItemPointerGetOffsetIndex --
 *	Returns the offset index of a disk item pointer.
 *
 * Note:
 *	Assumes that the disk item pointer is not in a user defined format.
 */
#define ItemPointerGetOffsetIndex(pointer, partition) \
    (AssertMacro(ItemPointerIsValid(pointer)) ? \
     ItemPointerGetOffsetNumber(pointer, partition) - 1 : \
     (OffsetIndex) 0)

/*
 * ItemPointerSet --
 *	Sets a disk item pointer to the specified block, page, and offset.
 *
 * Note:
 *	Assumes that the disk item pointer is not NULL.
 */
#define ItemPointerSet(pointer, partition, blockNumber, pageNumber, offNum) \
    Assert(PointerIsValid(pointer)); \
    BlockIdSet(&(pointer)->blockData, blockNumber); \
    PositionIdSet(&(pointer)->positionData, partition, pageNumber, offNum)

/*
 * ItemPointerCopy --
 *	Copies the contents of one disk item pointer to another.
 *
 * Note:
 *	Assumes that the disk item pointer is not NULL.
 */
#define ItemPointerCopy(fromPointer, toPointer) \
    Assert(PointerIsValid(toPointer)); \
    Assert(PointerIsValid(fromPointer)); \
    *(toPointer) = *(fromPointer)

/*
 * ItemPointerSetInvalid --
 *	Sets a disk item pointer to be invalid.
 *
 * Note:
 *	Assumes that the disk item pointer is not NULL.
 *
 * XXX byte ordering 
 */
#define ItemPointerSetInvalid(pointer) \
    Assert(PointerIsValid(pointer)); \
    (pointer)->blockData.data[0] &= 0x7fff; \
    (pointer)->positionData = 0

/*
 * ItemPointerSimpleGetPageNumber --
 *	Returns the page number of a "simple" disk item pointer.
 *
 * Note:
 *	Assumes that the disk item pointer is not in a user defined format.
 */
#define ItemPointerSimpleGetPageNumber(pointer) \
    ItemPointerGetPageNumber(pointer, SinglePagePartition)

/*
 * ItemPointerSimpleGetOffsetNumber --
 *	Returns the offset number of a "simple" disk item pointer.
 *
 * Note:
 *	Assumes that the disk item pointer is not in a user defined format.
 */
#define ItemPointerSimpleGetOffsetNumber(pointer) \
    ItemPointerGetOffsetNumber(pointer, SinglePagePartition)

/*
 * ItemPointerSimpleGetOffsetIndex --
 *	Returns the offset index of a "simple" disk item pointer.
 *
 * Note:
 *	Assumes that the disk item pointer is not in a user defined format.
 */
#define ItemPointerSimpleGetOffsetIndex(pointer) \
    ItemPointerGetOffsetIndex(pointer, SinglePagePartition)

/*
 * ItemPointerSimpleSet --
 *	Sets a disk item pointer to the specified block and offset.
 *
 * Note:
 *	Assumes that the disk item pointer is not NULL.
 */
#define ItemPointerSimpleSet(pointer, blockNumber, offsetNumber) \
    ItemPointerSet(pointer, SinglePagePartition, blockNumber, \
		   FirstPageNumber, offsetNumber)

/* ----------------
 *	externs
 * ----------------
 */
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
