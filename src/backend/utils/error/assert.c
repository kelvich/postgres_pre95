/*
 * assert.c --
 *	Assert code.
 *
 * Note:
 *	This should eventually work with elog(), dlog(), etc.
 */

#include <stdio.h>

#include "c.h"

RcsId("$Header$");

#include "exc.h"
#include "excid.h"	/* for FailedAssertion */
#include "pinit.h"	/* for AbortPostgres */


/* This may be removed someday. */
void
AssertionFailed(assertionName, fileName, lineNumber)
	const String	assertionName;
	const String	fileName;
	int		lineNumber;
{
	ExceptionalCondition(assertionName, &FailedAssertion, (String)NULL,
		fileName, lineNumber);
}

void
ExceptionalCondition(conditionName, exceptionP, detail, fileName, lineNumber)
	const String	conditionName;
	const Exception	*exceptionP;
	const String	detail;
	const String	fileName;
	const int	lineNumber;
{
	extern String ExcFileName;	/* XXX */
	extern Index ExcLineNumber;	/* XXX */

	ExcFileName = fileName;
	ExcLineNumber = lineNumber;

	if (!PointerIsValid(conditionName)
			|| !PointerIsValid(fileName)
			|| !PointerIsValid(exceptionP)) {
		fprintf(stderr, "ExceptionalCondition: bad arguments\n");

		AbortPostgres();
	} else {
		fprintf(stderr,
			"%s(\"%s:%s\", File: \"%s\", Line: %d)\n",
			exceptionP->message, conditionName, detail,
			fileName, lineNumber);
	}

	/*
	 * XXX Depending on the Exception and tracing conditions, you will
	 * XXX want to stop here immediately and maybe dump core.
	 * XXX This may be especially true for Assert(), etc.
	 */

	/* XXX FIXME: detail is lost */
	ExcRaise(exceptionP, (ExcDetail)0, (ExcData)NULL, conditionName);
}
