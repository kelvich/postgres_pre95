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

#ifndef C_H
#include "c.h"
#endif

#ifndef	ATT_H
# include "att.h"
#endif
#ifndef	ATTNUM_H
# include "attnum.h"
#endif
#ifndef	NAME_H
# include "name.h"
#endif

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

#define TUPDESC_SYMBOLS \
	SymbolDecl(TupleDescIsValid, "_TupleDescIsValid"), \
	SymbolDecl(CreateTemplateTupleDesc, "_CreateTemplateTupleDesc"), \
	SymbolDecl(TupleDescInitEntry, "_TupleDescInitEntry")

#endif	/* !defined(TupDescIncluded) */
