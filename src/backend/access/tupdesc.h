/*
 * tupdesc.h --
 *	POSTGRES tuple descriptor definitions.
 */

#ifndef	TupDescIncluded		/* Include this file only once */
#define TupDescIncluded	1

/*
 * Identification:
 */
#define TUPDESC_H	"$Header$"

#include "tmp/postgres.h"
#include "access/att.h"
#include "access/attnum.h"

typedef struct TupleDescriptorData {
	AttributeTupleForm	data[1];	/* VARIABLE LENGTH ARRAY */
} TupleDescriptorData;

typedef TupleDescriptorData	*TupleDescriptor;

typedef TupleDescriptorData	TupleDescD;

typedef TupleDescD		*TupleDesc;

/*
 * TupleDescIsValid --
 *	True iff tuple descriptor is valid.
 */
#define	TupleDescIsValid(desc) PointerIsValid(desc)

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
	AttributeNumber	numberOfAttributes
));

/*
 * TupleDescInitEntry --
 *	 Initializes attribute of a (template) tuple descriptor.
 *
 * Exceptions:
 *	BadArg if desc is invalid.
 *	BadArg if attributeNumber is non-positive.
 *	BadArg if typeName is invalid.
 *	BadArg if "entry" is already initialized.
 *
 * returns true if attribute is valid or false if attribute information
 * is empty (this is the case when the type name does not exist)
 */
extern
bool
TupleDescInitEntry ARGS((
	TupleDesc	desc,
	AttributeNumber	attributeNumber,
	Name		attributeName,
	Name		typeName,
	int         attdim
));

#endif	/* !defined(TupDescIncluded) */
