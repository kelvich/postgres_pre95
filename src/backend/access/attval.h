/*
 * attval.h --
 *	POSTGRES attribute value definitions.
 *
 * Identification:
 *	$Header$
 */

#ifndef	AttValIncluded	/* Include this file only once. */
#define AttValIncluded	1

#include "tmp/postgres.h"

typedef Datum	AttributeValue;

#include "access/attnum.h"
#include "access/htup.h"
#include "access/itup.h"
#include "access/tupdesc.h"

/* ----------------
 *	support macros
 * ----------------
 */

/*
 * AMgetattr --
 */
#define AMgetattr(tuple, attNum, tupleDescriptor, isNullOutP) \
    index_getattr(tuple, attNum, tupleDescriptor, isNullOutP)

/* ----------------
 *	extern decls
 * ----------------
 */

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
	Boolean		*isNullOutP
));

#endif	/* !defined(AttValIncluded) */
