/* ----------------------------------------------------------------
 *   FILE
 *	off.h
 *
 *   DESCRIPTION
 *	POSTGRES disk "offset" definitions.
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef	OffIncluded		/* Include this file only once */
#define OffIncluded	1

#define OFF_H	"$Header$"

#include "tmp/c.h"
#include "machine.h"	/* (in port dir.) for BLCKSZ */
#include "storage/itemid.h"

typedef uint16		OffsetNumber;	/* offset number */
typedef uint16		OffsetIndex;	/* offset index */
typedef int16		OffsetIdData;	/* internal offset identifier */
typedef OffsetIdData	*OffsetId;	/* offset identifier */

#define InvalidOffsetNumber	0
#define FirstOffsetIndex	0

/* ----------------
 *	support macros
 * ----------------
 */
/*
 * OffsetNumberIsValid --
 *	True iff the offset number is valid.
 */
#define OffsetNumberIsValid(offsetNumber) \
    ((bool) \
     ((offsetNumber != 0) && (offsetNumber < BLCKSZ / sizeof(ItemIdData))))

/*
 * OffsetIdIsValid --
 *	True iff the offset identifier is valid.
 */
#define OffsetIdIsValid(offsetId) \
    ((bool) (PointerIsValid(offsetId) && (*(offsetId) != 0)))

/*
 * OffsetIdSet --
 *	Sets a offset identifier to the specified value.
 */
#define OffsetIdSet(offsetId, offsetNumber) \
    ((*offsetId) = (offsetNumber))

/*
 * OffsetIdGetOffsetNumber --
 *	Retrieve the offset number from a offset identifier.
 */
#define OffsetIdGetOffsetNumber(offsetId) \
    ((OffsetNumber) (*(offsetId)))

#endif	/* !defined(OffIncluded) */
