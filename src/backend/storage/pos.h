/* ----------------------------------------------------------------
 *   FILE
 *	pos.h
 *
 *   DESCRIPTION
 *	POSTGRES "position" definitions.
 *
 *	A "position" ocnsists of "pageNumber ... offsetNumber"
 *	where pageNumber occupies a number of page bits determined
 *	by a PagePartition.
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef	PosIncluded		/* Include this file only once */
#define PosIncluded	1

#define POS_H	"$Header$"

#include "tmp/c.h"
#include "storage/part.h"
#include "storage/pagenum.h"

typedef bits16	PositionIdData;	/* internal position identifier */
typedef PositionIdData	*PositionId;	/* position identifier */

/* ----------------
 *	support macros
 * ----------------
 */
/*
 * PositionIdIsValid --
 *	True iff the position identifier is valid.
 */
#define PositionIdIsValid(positionId, partition) \
    ((bool) (PointerIsValid(positionId) && \
	     (*(positionId) & OffsetNumberMask(partition))))

/*
 * PositionIdSetInValid --
 *      Make an invalid postion.
 */
#define PositionIdSetInvalid(positionId) *(positionId) & 0x0

/*
 * PositionIdSet --
 *	Sets a position identifier to the specified value.
 */
#define PositionIdSet(positionId, partition, pageNumber, offsetNumber) \
   Assert((pageNumber >> PagePartitionGetNumberOfPageBits(partition)) == 0); \
   Assert((offsetNumber >> PagePartitionGetNumberOfOffsetBits(partition))==0);\
    *(positionId) = offsetNumber | \
     (pageNumber << PagePartitionGetNumberOfOffsetBits(partition))

/*
 * PositionIdGetPageNumber --
 *	Retrieve the page number from a position identifier.
 */
#define PositionIdGetPageNumber(positionId, partition) \
    ((PageNumber) \
     (0xffff & \
      (*(positionId) >> PagePartitionGetNumberOfOffsetBits(partition))))

/*
 * PositionIdGetOffsetNumber --
 *	Retrieve the offset number from a position identifier.
 */
#define PositionIdGetOffsetNumber(positionId, partition) \
    (AssertMacro(PositionIdIsValid(positionId, partition)) ? \
     ((OffsetNumber) (*(positionId) & OffsetNumberMask(partition))) : \
     (OffsetNumber) 0)

#endif	/* !defined(PosIncluded) */
