/*
 * manip.c --
 *	POSTGRES "manipulation" utility code.
 */

#include "tmp/postgres.h"

RcsId("$Header$");

#include "nodes/pg_lisp.h"
#include "utils/log.h"	/* for elog, WARN */

#include "manip.h"

#define StringComputeLength(string)	(strlen(string))

LispValue
DefineListRemoveOptionalIndicator(assocListInOutP, string)
	LispValue	*assocListInOutP;
	String		string;
{
	LispValue	entry;

	entry = LispRemoveMatchingString(assocListInOutP, string);

	if (!null(entry)) {
		if (length(entry) != 1) {
			if (length(entry) == 2) {
				elog(WARN, "Define: %s takes no argument",
					string);
			} else {
				elog(WARN, "Define: parser error on %s",
					string);
			}
		}
	}
	return (entry);
}

LispValue
DefineListRemoveOptionalAssignment(assocListInOutP, string)
	LispValue	*assocListInOutP;
	String		string;
{
	LispValue	entry;

	entry = LispRemoveMatchingString(assocListInOutP, string);

	if (!null(entry)) {
		if (length(entry) != 2) {
			if (length(entry) == 1) {
				elog(WARN, "Define: %s requires an argument",
					string);
			} else {
				elog(WARN, "Define: parser error on %s",
					string);
			}
		}
	}
	return (entry);
}

LispValue
DefineListRemoveRequiredAssignment(assocListInOutP, string)
	LispValue	*assocListInOutP;
	String		string;
{
	LispValue	entry;

	entry = LispRemoveMatchingString(assocListInOutP, string);
	if (null(entry)) {
		elog(WARN, "Define: %s unspecified", string);
	}
	if (length(entry) != 2) {
		if (length(entry) == 1) {
			elog(WARN, "Define: %s requires an argument", string);
		} else {
			elog(WARN, "Define: parser error on %s", string);
		}
	}

	return (entry);
}

String
DefineEntryGetString(entry)
	LispValue	entry;
{
	AssertArg(consp(entry) && consp(CDR(entry)));

	if (!lispStringp(CADR(entry))) {
		AssertArg(lispStringp(CAR(entry)));
		elog(WARN, "Define: %s = what?", CString(CAR(entry)));
	}

	return (CString(CADR(entry)));
}

Name
DefineEntryGetName(entry)
	LispValue	entry;
{
	String	string;

	AssertArg(consp(entry) && consp(CDR(entry)));

	if (!lispStringp(CADR(entry))) {
		AssertArg(lispStringp(CAR(entry)));
		elog(WARN, "Define: %s = what?", CString(CAR(entry)));
	}

	string = CString(CADR(entry));
	if (StringComputeLength(string) >= 17) {
		elog(WARN, "Define: %s is too long to be a name", string);
	}

	return ((Name)string);
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
		if (!StringEquals(CString(CADR(entry)), "variable")) {
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
	String		string;
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
			StringEquals(CString(CAR(CAR(rest))), string)) {
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
				StringEquals(CString(CAR(CADR(rest))),
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
