/*
 * att.h --
 *	POSTGRES attribute definitions.
 *
 * Identification:
 *	$Header$
 */

#ifndef	AttIncluded	/* Include this file only once. */
#define AttIncluded	1

#include "cat.h"

#ifndef C_H
#include "c.h"
#endif

typedef AttributeTupleForm	Attribute;

/*
 * AttributeIsValid
 *	True iff the attribute is valid.
 */
extern
bool
AttributeIsValid ARGS((
	Attribute	attribute
));

#endif	/* !defined(AttIncluded) */
