/*
 * off.h --
 *	POSTGRES disk "offset" definitions.
 *
 * Identification:
 *	$Header$
 */

#ifndef	OffIncluded	/* Include this file only once. */
#define OffIncluded	1

#ifndef C_H
#include "c.h"
#endif

typedef uint16	OffsetNumber;	/* offset number */
typedef uint16	OffsetIndex;	/* offset index */

typedef int16		OffsetIdData;	/* internal offset identifier */
typedef OffsetIdData	*OffsetId;	/* offset identifier */

#define InvalidOffsetNumber	0
#define FirstOffsetIndex	0

/*
 * OffsetIndexIsValid --
 *	True iff the offset index is valid.
 */
extern
bool
OffsetIndexIsValid ARGS((
	OffsetIndex	offsetIndex
));

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
