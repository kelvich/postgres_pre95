
; /*
; * 
; * POSTGRES Data Base Management System
; * 
; * Copyright (c) 1988 Regents of the University of California
; * 
; * Permission to use, copy, modify, and distribute this software and its
; * documentation for educational, research, and non-profit purposes and
; * without fee is hereby granted, provided that the above copyright
; * notice appear in all copies and that both that copyright notice and
; * this permission notice appear in supporting documentation, and that
; * the name of the University of California not be used in advertising
; * or publicity pertaining to distribution of the software without
; * specific, written prior permission.  Permission to incorporate this
; * software into commercial products can be obtained from the Campus
; * Software Office, 295 Evans Hall, University of California, Berkeley,
; * Ca., 94720 provided only that the the requestor give the University
; * of California a free licence to any derived software for educational
; * and research purposes.  The University of California makes no
; * representations about the suitability of this software for any
; * purpose.  It is provided "as is" without express or implied warranty.
; * 
; */



/*
 * itemptr.h --
 *	POSTGRES disk item pointer definitions.
 *
 * Identification:
 *	$Header$
 */

#ifndef	ObjPtrIncluded	/* Include this file only once. */
#define ObjPtrIncluded	1

#include "c.h"

#include "block.h"
#include "off.h"
#include "pagenum.h"
#include "part.h"
#include "pos.h"

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

#endif	/* !defined(ObjPtrIncluded) */
