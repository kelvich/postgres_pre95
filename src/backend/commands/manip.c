/* ----------------------------------------------------------------
 * manip.c --
 *	POSTGRES argument list "manipulation" utility code.
 *
 * DESCRIPTION:
 *	These are "helper" routines for the routines in define.c.
 *
 * INTERFACE ROUTINES:
 *	DefineListRemoveOptionalIndicator
 *	DefineListRemoveOptionalAssignment
 *	DefineListRemoveRequiredAssignment
 *	DefineEntryGetString
 *	DefineEntryGetName
 *	DefineEntryGetInteger
 *	DefineEntryGetLength
 *	DefineListAssertEmpty
 *	LispRemoveMatchingString
 *	LispRemoveMatchingSymbol
 *
 * NOTES:
 *	All of the routine descriptions are in manip.h.
 *
 * IDENTIFICATION:
 *	$Header$
 *
 * ----------------------------------------------------------------
 */

#include "tmp/postgres.h"

RcsId("$Header$");

#include "utils/log.h"
#include "commands/manip.h"

LispValue
DefineListRemoveOptionalIndicator(assocListInOutP, string)
    LispValue	*assocListInOutP;
    String	string;
{
    LispValue	entry = LispRemoveMatchingString(assocListInOutP, string);

    if (!null(entry)) {
	if (length(entry) != 1) {
	    if (length(entry) == 2) {
		elog(WARN, "Define: \"%s\" takes no argument",
		     string);
	    } else {
		elog(WARN, "Define: parser error on \"%s\"",
		     string);
	    }
	}
    }
    return (entry);
}

LispValue
DefineListRemoveOptionalAssignment(assocListInOutP, string)
    LispValue	*assocListInOutP;
    String	string;
{
    LispValue	entry = LispRemoveMatchingString(assocListInOutP, string);

    if (!null(entry)) {
	if (length(entry) != 2) {
	    if (length(entry) == 1) {
		elog(WARN, "Define: \"%s\" requires an argument",
		     string);
	    } else {
		elog(WARN, "Define: parser error on \"%s\"",
		     string);
	    }
	}
    }
    return (entry);
}

LispValue
DefineListRemoveRequiredAssignment(assocListInOutP, string)
    LispValue	*assocListInOutP;
    String	string;
{
    LispValue	entry = LispRemoveMatchingString(assocListInOutP, string);

    if (null(entry)) {
	elog(WARN, "Define: \"%s\" unspecified", string);
    }
    if (length(entry) != 2) {
	if (length(entry) == 1) {
	    elog(WARN, "Define: \"%s\" requires an argument", string);
	} else {
	    elog(WARN, "Define: parser error on \"%s\"", string);
	}
    }

    return (entry);
}

String
DefineEntryGetString(entry)
    LispValue	entry;
{
    if (null(entry)) {
	return((String) NULL);
    }

    AssertArg(consp(entry) && consp(CDR(entry)));

    if (!lispStringp(CADR(entry))) {
	AssertArg(lispStringp(CAR(entry)));
	elog(WARN, "Define: \"%s\" = what?", CString(CAR(entry)));
    }

    return (CString(CADR(entry)));
}

Name
DefineEntryGetName(entry)
    LispValue	entry;
{
    String	string;
    Name	name;

    if (null(entry)) {
	return((Name) NULL);
    }

    AssertArg(consp(entry) && consp(CDR(entry)));
    
    if (!lispStringp(CADR(entry))) {
	AssertArg(lispStringp(CAR(entry)));
	elog(WARN, "Define: %s = what?", CString(CAR(entry)));
    }
    
    string = CString(CADR(entry));
    if (strlen(string) > NAMEDATALEN) {
	elog(WARN, "Define: \"%s\" is too long to be an identifier",
	     string);
    }
    if (!(name = (Name) palloc(sizeof(NameData) + 1))) {
	elog(WARN, "Define: palloc failed!");
    }
    bzero(name, sizeof(NameData) + 1);
    (void) namestrcpy(name, string);
    
    return (name);
}

int32
DefineEntryGetInteger(entry)
    LispValue	entry;
{
    AssertArg(consp(entry) && consp(CDR(entry)));

    if (!lispIntegerp(CADR(entry))) {
	AssertArg(lispStringp(CAR(entry)));
	elog(WARN, "Define: %s = what?", CString(CAR(entry)));
    }

    return (CInteger(CADR(entry)));
}

int16
DefineEntryGetLength(entry)
    LispValue	entry;
{
    int16	length;

    AssertArg(consp(entry) && consp(CDR(entry)));

    if (lispStringp(CADR(entry))) {
	if (strcmp(CString(CADR(entry)), "variable")) {
	    AssertArg(lispStringp(CAR(entry)));
	    elog(WARN, "Define: %s = %s?", CString(CAR(entry)),
		 CString(CADR(entry)));
	}
	length = -1;
    } else if (!lispIntegerp(CADR(entry))) {
	AssertArg(lispStringp(CAR(entry)));
	elog(WARN, "Define: %s = what?", CString(CADR(entry)));
    } else {
	if (CInteger(CADR(entry)) < 0) {
	    elog(WARN, "Define: non-positive lengths disallowed");
	}
	length = CInteger(CADR(entry));
    }

    return (length);
}

void
DefineListAssertEmpty(assocList)
    LispValue	assocList;
{
    AssertArg(listp(assocList));

    if (!null(assocList)) {
	if (lispStringp(CAR(CAR(assocList)))) {
	    elog(WARN, "Define: %s is unknown or repeated",
		 CString(CAR(CAR(assocList))));
	} else if (lispAtomp(CAR(CAR(assocList)))) {
	    elog(WARN, "Define: symbol %d is unknown or repeated",
		 CAtom(CAR(CAR(assocList))));
	} else {
	    elog(WARN, "Define: strange parse");
	}
    }
}

/*
 * private?
 */

LispValue
LispRemoveMatchingString(assocListInOutP, string)
    LispValue	*assocListInOutP;
    String	string;
{
    LispValue	rest;

    AssertArg(PointerIsValid(assocListInOutP));
    AssertArg(listp(*assocListInOutP));
    AssertArg(StringIsValid(string));

    if (null(*assocListInOutP)) {
	return (LispNil);
    }
    rest = *assocListInOutP;
    if (stringp(CAR(CAR(rest))) &&
	!strcmp(CString(CAR(CAR(rest))), string)) {
	*assocListInOutP = CDR(rest);
	/*
	 * rplacd(rest, LispNil);
	 * freeit(rest);
	 */
	return (CAR(rest));
    }

    while (!null(CDR(rest))) {
	/*
	 * process next element
	 */
	if (stringp(CAR(CADR(rest))) &&
	    !strcmp(CString(CAR(CADR(rest))),
			 string)) {
	    LispValue	result;

	    result = CDR(rest);
	    rplacd(rest, CDR(result));
	    /*
	     * rplacd(result, LispNil);
	     * freeit(result);
	     */
	    return (CAR(result));
	}
	rest = CDR(rest);
    }
    return (LispNil);
}

LispValue
LispRemoveMatchingSymbol(assocListInOutP, symbol)
    LispValue	*assocListInOutP;
    int		symbol;
{
    LispValue	rest;

    AssertArg(PointerIsValid(assocListInOutP));
    AssertArg(listp(*assocListInOutP));
    /* AssertArg(atom(symbol)); */

    if (null(*assocListInOutP)) {
	return (LispNil);
    }
    rest = *assocListInOutP;
    if (atom(CAR(CAR(rest))) && (CAtom(CAR(CAR(rest))) == symbol)) {
	*assocListInOutP = CDR(rest);
	/*
	 * rplacd(rest, LispNil);
	 * freeit(rest);
	 */
	return (CAR(rest));
    }

    while (!null(CDR(rest))) {
	/*
	 * process next element
	 */
	if (atom(CAR(CADR(rest))) &&
	    (CAtom(CAR(CADR(rest))) == symbol)) {
	    LispValue	result;

	    result = CDR(rest);
	    rplacd(rest, CDR(result));
	    /*
	     * rplacd(result, LispNil);
	     * freeit(result);
	     */
	    return (CAR(result));
	}
	rest = CDR(rest);
    }
    return (LispNil);
}
