/* ----------------------------------------------------------------
 *   FILE
 *	attnum.h
 *
 *   DESCRIPTION
 *	POSTGRES attribute number definitions.
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */
#ifndef	AttNumIncluded		/* Include this file only once */
#define AttNumIncluded	1

#define ATTNUM_H	"$Header$"

#include "tmp/c.h"

typedef int16		AttributeNumber;
typedef AttributeNumber	*AttributeNumberPtr;
typedef uint16		AttributeOffset;

#define InvalidAttributeNumber	0

/* ----------------
 *	support macros
 * ----------------
 */
/*
 * AttributeNumberIsValid --
 *	True iff the attribute number is valid.
 */
#define AttributeNumberIsValid(attributeNumber) \
    ((bool) ((attributeNumber) != InvalidAttributeNumber))

/*
 * AttributeNumberIsForUserDefinedAttribute --
 *	True iff the attribute number corresponds to an user defined attribute.
 */
#define AttributeNumberIsForUserDefinedAttribute(attributeNumber) \
    ((bool) ((attributeNumber) > 0))

/*
 * AttributeNumberIsInBounds --
 *	True iff attribute number is within given bounds.
 *
 * Note:
 *	Assumes AttributeNumber is an signed type.
 *	Assumes the bounded interval to be (minumum,maximum].
 *	An invalid attribute number is within given bounds.
 */
#define AttributeNumberIsInBounds(attNum, minAttNum, maxAttNum) \
     ((bool) OffsetIsInBounds(attNum, minAttNum, maxAttNum))

/*
 * AttributeNumberGetAttributeOffset --
 *	Returns the attribute offset for an attribute number.
 *
 * Note:
 *	Assumes the attribute number is for an user defined attribute.
 */
#define AttributeNumberGetAttributeOffset(attNum) \
     (AssertMacro(AttributeNumberIsForUserDefinedAttribute(attNum)) ? \
      ((AttributeOffset) (attNum - 1)) : (AttributeOffset) 0)

/*
 * AttributeOffsetGetAttributeNumber --
 *	Returns the attribute number for an attribute offset.
 */
#define AttributeOffsetGetAttributeNumber(attributeOffset) \
     ((AttributeNumber) (1 + attributeOffset))

#endif	/* !defined(AttNumIncluded) */
