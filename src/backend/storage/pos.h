/*
 * pos.h --
 *	POSTGRES "position" definitions.
 *
 * Identification:
 *	$Header$
 */

#ifndef	PosIncluded	/* Include this file only once. */
#define PosIncluded	1

#include "c.h"

#include "part.h"
#include "pagenum.h"

typedef bits16	PositionIdData;	/* internal position identifier */
typedef PositionIdData	*PositionId;	/* position identifier */

/*
 * PositionIdIsValid --
 *	True iff the position identifier is valid.
 */
extern
bool
PositionIdIsValid ARGS((
	PositionId	positionId,
	PagePartition	partition
));

/*
 * PositionIdSet --
 *	Sets a position identifier to the specified value.
 */
extern
void
PositionIdSet ARGS((
	PositionId	positionId,
	PagePartition	partition,
	PageNumber	pageNumber,
	OffsetNumber	offsetNumber
));

/*
 * PositionIdGetPageNumber --
 *	Retrieve the page number from a position identifier.
 */
extern
PageNumber
PositionIdGetPageNumber ARGS((
	PositionId	positionId,
	PagePartition	partition
));

/*
 * PositionIdGetOffsetNumber --
 *	Retrieve the offset number from a position identifier.
 */
extern
OffsetNumber
PositionIdGetOffsetNumber ARGS((
	PositionId	positionId,
	PagePartition	partition
));

#endif	/* !defined(PosIncluded) */
