/*
 * assert.c --
 *	Assert code.
 *
 * Note:
 *	This should eventually work with elog(), dlog(), etc.
 */

#include <stdio.h>

#include "tmp/c.h"
#include "utils/module.h"

RcsId("$Header$");

#include "utils/exc.h"

/*
 * There might be a need to specially handle FailedAssertion, below.
 *
#include "excid.h"	// for FailedAssertion
 */

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

		ExcAbort(exceptionP, detail, NULL, NULL);	/* ??? */
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

	TraceDump();	/* dump the trace stack */

	/* XXX FIXME: detail is lost */
	ExcRaise(exceptionP, (ExcDetail)0, (ExcData)NULL, conditionName);
}
