/*
 * off.h --
 *	POSTGRES disk "offset" definitions.
 */

#ifndef	OffIncluded		/* Include this file only once */
#define OffIncluded	1

/*
 * Identification:
 */
#define OFF_H	"$Header$"

#include "tmp/c.h"

typedef uint16	OffsetNumber;	/* offset number */
typedef uint16	OffsetIndex;	/* offset index */

typedef int16		OffsetIdData;	/* internal offset identifier */
typedef OffsetIdData	*OffsetId;	/* offset identifier */

#define InvalidOffsetNumber	0
#define FirstOffsetIndex	0

/*
 * OffsetNumberIsValid --
 *	True iff the offset number is valid.
 */
extern
bool
OffsetNumberIsValid ARGS((
	OffsetNumber	offsetNumber
));

/*
 * OffsetIdIsValid --
 *	True iff the offset identifier is valid.
 */
extern
bool
OffsetIdIsValid ARGS((
	OffsetId	offsetId
));

/*
 * OffsetIdSet --
 *	Sets a offset identifier to the specified value.
 */
extern
void
OffsetIdSet ARGS((
	OffsetId	offsetId,
	OffsetNumber	offsetNumber
));

/*
 * OffsetIdGetOffsetNumber --
 *	Retrieve the offset number from a offset identifier.
 */
extern
OffsetNumber
OffsetIdGetOffsetNumber ARGS((
	OffsetId	offsetId
));

#endif	/* !defined(OffIncluded) */
