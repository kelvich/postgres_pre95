/*
 * attval.h --
 *	POSTGRES attribute value definitions.
 *
 * Identification:
 *	$Header$
 */

#ifndef	AttValIncluded	/* Include this file only once. */
#define AttValIncluded	1

#include "tmp/c.h"

#include "tmp/datum.h"
#include "access/attnum.h"
#include "access/htup.h"
#include "access/itup.h"
#include "access/tupdesc.h"

typedef Datum	AttributeValue;

/*
 * AttributeValueIsValid --
 *	True iff the value is valid.
 */
extern
bool
AttributeValueIsValid ARGS((
	AttributeValue	attributeValue
));

/*
 * IndexTupleGetAttributeValue
 *	Returns the value of an index tuple attribute.
 */
extern
AttributeValue
IndexTupleGetAttributeValue ARGS((
	IndexTuple	tuple,
	AttributeNumber	attributeNumber,
	TupleDescriptor	tupleDescriptor,
	Boolean		isNullOutP
));

/*
 * AMgetattr --
 */
extern
Pointer
AMgetattr ARGS((
	IndexTuple	tuple,
	AttributeNumber	attributeNumber,
	TupleDescriptor	tupleDescriptor,
	Boolean		isNullOutP
));

#endif	/* !defined(AttValIncluded) */
