/*
 * att.h --
 *	POSTGRES attribute definitions.
 */

#ifndef	AttIncluded		/* Include this file only once */
#define AttIncluded	1

/*
 * Identification:
 */
#define ATT_H	"$Header$"

#include "tmp/c.h"
#include "access/skey.h"   	/* just to reduce levels of #include */
#include "catalog/pg_attribute.h"

typedef AttributeTupleForm	Attribute;

typedef Attribute		*AttributePtr;

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
