/*
 * defrem.h --
 *	POSTGRES define and remove utility definitions.
 *
 * Identification:
 *	$Header$
 */

#ifndef	DefRemIncluded		/* Include this file only once */
#define DefRemIncluded	1

#include "tmp/postgres.h"
#include "nodes/pg_lisp.h"

/*
 * DefineIndex --
 *	Creates a new index.
 *
 * Exceptions:
 *	XXX
 */
extern
void
DefineIndex ARGS((
	Name		heapRelationName,
	Name		indexRelationName,
	Name		accessMethodName,
	LispValue	attributeList,
	LispValue	parameterList,
	LispValue	predicate
));

/*
 * RemoveIndex --
 *	Deletes an index.
 *
 * Exceptions:
 *	BadArg if name is invalid.
 *	"WARN" if index nonexistant.
 *	...
 */
extern
void
RemoveIndex ARGS((
	Name	name
));

/*
 * DefineFunction --
 *	Registers a new function.
 *
 * Exceptions:
 *	XXX
 */
extern
void
DefineFunction ARGS((
	LispValue	nameargsexe
));

extern
void
DefineCFunction ARGS((
	Name            name,
	LispValue       parameters,
        String          filename,
        String          languageName
));

/*
 * RemoveFunction --
 *	Deletes a function.
 *
 * Exceptions:
 *	BadArg if name is invalid.
 *	"WARN" if function nonexistant.
 *	...
 */
extern
void
RemoveFunction ARGS((
	Name	name
));

/*
 * DefineType --
 *	Registers a new type.
 *
 * Exceptions:
 *	XXX
 */
extern
void
DefineType ARGS((
	Name		name,
	LispValue	parameters
));

/*
 * RemoveType --
 *	Deletes a type.
 *
 * Exceptions:
 *	BadArg if name is invalid.
 *	"WARN" if type nonexistant.
 *	...
 */
extern
void
RemoveType ARGS((
	Name	name
));

/*
 * DefineOperator --
 *	Registers a new operator.
 *
 * Exceptions:
 *	XXX
 */
extern
void
DefineOperator ARGS((
	Name		name,
	LispValue	parameters
));

/*
 * RemoveOperator --
 *	Deletes an operator.
 *
 * Exceptions:
 *	BadArg if name is invalid.
 *	BadArg if type1 is invalid.
 *	"WARN" if operator nonexistant.
 *	...
 */
extern
void
RemoveOperator ARGS((
	Name	name,
	Name	type1,
	Name	type2
));

void DefinePFunction ARGS((char *pname, List parameters, List query_tree));


#endif	/* !defined(DefRemIncluded) */
