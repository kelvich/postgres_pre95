/*
 * attnum.h --
 *	POSTGRES attribute number definitions.
 */

#ifndef	AttNumIncluded		/* Include this file only once */
#define AttNumIncluded	1

/*
 * Identification:
 */
#define ATTNUM_H	"$Header$"

#ifndef C_H
#include "c.h"
#endif

typedef int16	AttributeNumber;

#define InvalidAttributeNumber	0

typedef uint16	AttributeOffset;

/*
 * AttributeNumberIsValid --
 *	True iff the attribute number is valid.
 */
extern
bool
AttributeNumberIsValid ARGS((
	AttributeNumber	attributeNumber
));

/*
 * AttributeNumberIsForUserDefinedAttribute --
 *	True iff the attribute number corresponds to an user defined attribute.
 */
extern
bool
AttributeNumberIsForUserDefinedAttribute ARGS((
	AttributeNumber	attributeNumber
));

/*
 * AttributeNumberIsInBounds --
 *	True iff attribute number is within given bounds.
 *
 * Note:
 *	Assumes AttributeNumber is an signed type.
 *	Assumes the bounded interval to be (minumum,maximum].
 *	An invalid attribute number is within given bounds.
 */
extern
bool
AttributeNumberIsInBounds ARGS((
	AttributeNumber	attributeNumber,
	AttributeNumber	minimumAttributeNumber,
	AttributeNumber	maximumAttributeNumber
));

/*
 * AttributeNumberGetAttributeOffset --
 *	Returns the attribute offset for an attribute number.
 *
 * Note:
 *	Assumes the attribute number is for an user defined attribute.
 */
extern
AttributeOffset
AttributeNumberGetAttributeOffset ARGS((
	AttributeNumber	attributeNumber
));

/*
 * AttributeOffsetGetAttributeNumber --
 *	Returns the attribute number for an attribute offset.
 */
extern
AttributeNumber
AttributeOffsetGetAttributeNumber ARGS((
	AttributeOffset	attributeOffset
));

#define ATTNUM_SYMBOLS \
	SymbolDecl(AttributeNumberIsValid, "_AttributeNumberIsValid"), \
	SymbolDecl(AttributeNumberIsForUserDefinedAttribute, "_AttributeNumberIsForUserDefinedAttribute"), \
	SymbolDecl(AttributeNumberIsInBounds, "_AttributeNumberIsInBounds"), \
	SymbolDecl(AttributeNumberGetAttributeOffset, "_AttributeNumberGetAttributeOffset"), \
	SymbolDecl(AttributeOffsetGetAttributeNumber, "_AttributeOffsetGetAttributeNumber")

#endif	/* !defined(AttNumIncluded) */
