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

#ifndef C_H
#include "c.h"
#endif

#ifndef	CAT_H
#include "cat.h"
#endif

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

#define ATT_SYMBOLS \
	SymbolDecl(AttributeIsValid, "_AttributeIsValid")

#endif	/* !defined(AttIncluded) */
