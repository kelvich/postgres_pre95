/*
 * pos.h --
 *	POSTGRES "position" definitions.
 */

#ifndef	PosIncluded		/* Include this file only once */
#define PosIncluded	1

/*
 * Identification:
 */
#define POS_H	"$Header$"

#ifndef C_H
#include "c.h"
#endif

#ifndef	PART_H
# include "part.h"
#endif
#ifndef	PAGENUM_H
# include "pagenum.h"
#endif

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

#define POS_SYMBOLS \
	SymbolDecl(PositionIdIsValid, "_PositionIdIsValid"), \
	SymbolDecl(PositionIdSet, "_PositionIdSet"), \
	SymbolDecl(PositionIdGetPageNumber, "_PositionIdGetPageNumber"), \
	SymbolDecl(PositionIdGetOffsetNumber, "_PositionIdGetOffsetNumber")

#endif	/* !defined(PosIncluded) */
