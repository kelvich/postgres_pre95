/*
 * manip.h --
 *	POSTGRES "manipulation" utility definitions.
 *
 * Identification:
 *	$Header$
 */

#ifndef	ManipIncluded		/* Include this file only once */
#define ManipIncluded	1

#include <strings.h>

#include "tmp/c.h"

/*
 * XXX style--this should be in a more global header file
 */
#define StringEquals(x, y)	(strcmp(x, y) == 0)

#include "nodes/pg_lisp.h"
#include "tmp/name.h"

/*
 * DefineListRemoveOptionalIndicator --
 *	Returns list element with car matching given string or LispNil.
 *
 * Side effects:
 *	Returned list element is removed from *assocListInOutP.
 *
 * Exceptions:
 *	"WARN" if string is not found.
 *	...
 */
extern
LispValue
DefineListRemoveOptionalIndicator ARGS((
	LispValue	*assocListInOutP,
	String		string
));

/*
 * DefineListRemoveOptionalAssignment --
 *	Returns list element with car matching given string or LispNil.
 *
 * Side effects:
 *	Returned list element is removed from *assocListInOutP.
 *
 * Exceptions:
 *	"WARN" if string is not found.
 *	...
 */
extern
LispValue
DefineListRemoveOptionalAssignment ARGS((
	LispValue	*assocListInOutP,
	String		string
));

/*
 * DefineListRemoveRequiredAssignment --
 *	Returns list element with car matching given string or LispNil.
 *
 * Side effects:
 *	Returned list element is removed from *assocListInOutP.
 *
 * Exceptions:
 *	"WARN" if string is not found.
 *	...
 */
extern
LispValue
DefineListRemoveRequiredAssignment ARGS((
	LispValue	*assocListInOutP,
	String		string
));

/*
 * DefineEntryGetString --
 *	Returns "value" string for given define entry.
 *
 * Exceptions:
 *	BadArg if entry is an invalid "entry."
 *	"WARN" if "value" is not a string.
 */
extern
String
DefineEntryGetString ARGS((
	LispValue	entry
));

/*
 * DefineEntryGetName --
 *	Returns "value" name for given define entry.
 *
 * Exceptions:
 *	BadArg if entry is an invalid "entry."
 *	"WARN" if "value" is not a name.
 */
extern
Name
DefineEntryGetName ARGS((
	LispValue	entry
));

/*
 * DefineEntryGetInteger --
 *	Returns "value" number for given define entry.
 *
 * Exceptions:
 *	BadArg if entry is an invalid "entry."
 *	"WARN" if "value" is not a string.
 */
extern
int32
DefineEntryGetInteger ARGS((
	LispValue	entry
));

/*
 * DefineEntryGetLength --
 *	Returns "value" length for given define entry.
 *
 * Exceptions:
 *	BadArg if entry is an invalid "entry."
 *	"WARN" if "value" is not a positive number or "variable."
 */
extern
int16		/* int2 */
DefineEntryGetLength ARGS((
	LispValue	entry
));

/*
 * DefineListAssertEmpty --
 *	Causes an error if assocList is nonempty.
 *
 * Exceptions:
 *	BadArg if assocList is an invalid list.
 */
extern
void
DefineListAssertEmpty ARGS((
	LispValue	assocList
));

/*
 * private?
 */

/*
 * LispRemoveMatchingString --
 *	Returns first list element with car matching given string or LispNil.
 *
 * Side effects:
 *	Returned list element is removed from *assocListInOutP.
 *
 * Exceptions:
 *	BadArg if assocListInOutP is invalid.
 *	BadArg if *assocListInOutP is not a list.
 *	BadArg if string is invalid.
 */
extern
LispValue
LispRemoveMatchingString ARGS((
	LispValue	*assocListInOutP,
	String		string
));

/*
 * LispRemoveMatchingSymbol --
 *	Returns first list element with car matching given symbol or LispNil.
 *
 * Side effects:
 *	Returned list element is removed from *AssocListP.
 *
 * Exceptions:
 *	BadArg if assocListP is invalid.
 *	BadArg if *assocListP is not a list.
 *	BadArg if symbol is not a symbol handle.
 */
extern
LispValue
LispRemoveMatchingSymbol ARGS((
	LispValue	*assocListP,
	int		symbol
));

#endif	/* !defined(ManipIncluded) */
