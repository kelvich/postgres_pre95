/*
 * tupdesc.h --
 *	POSTGRES tuple descriptor definitions.
 *
 * Identification:
 *	$Header$
 */

#ifndef	TupDescIncluded		/* Include this file only once */
#define TupDescIncluded	1

#ifndef C_H
#include "c.h"
#endif

#include "att.h"
#include "attnum.h"
#include "name.h"

typedef struct TupleDescriptorData {
	AttributeTupleForm	data[1];	/* VARIABLE LENGTH ARRAY */
} TupleDescriptorData;

typedef TupleDescriptorData	*TupleDescriptor;

typedef TupleDescriptorData	TupleDescD;

typedef TupleDescD	*TupleDesc;

/*
 * TupleDescIsValid --
 *	True iff tuple descriptor is valid.
 */
extern
bool
TupleDescIsValid ARGS((
	TupleDesc	desc
));

/*
 * CreateTemplateTupleDesc --
 *	Returns template of a tuple descriptor.
 *
 * Exceptions:
 *	BadArg if numberOfAttributes is non-positive.
 */
extern
TupleDesc
CreateTemplateTupleDesc ARGS((
	AttributeNumber	numberOfAttributes;
));

/*
 * TupleDescInitEntry --
 *	 Initializes attribute of a (template) tuple descriptor.
 *
 * Exceptions:
 *	BadArg if desc is invalid.
 *	BadArg if attributeNumber is non-positive.
 *	BadArg if typeName is invalid.
 *	BadArg if attributeName is invalid.
 *	BadArg if "entry" is already initialized.
 */
extern
void
TupleDescInitEntry ARGS((
	TupleDesc	desc,
	AttributeNumber	attributeNumber,
	Name		attributeName,
	Name		typeName
));

#endif	/* !defined(TupDescIncluded) */
