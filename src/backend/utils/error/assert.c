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

void
AssertionFailed(assertionName, fileName, lineNumber)
const String	assertionName;
const String	fileName;
const int	lineNumber;
{
	if (!PointerIsValid(assertionName) || !PointerIsValid(fileName))
		fprintf(stderr, "AssertionFailed: bad arguments\n");
	else
		fprintf(stderr,
			"AssertionFailed(\"%s\", File: \"%s\", Line: %d)\n",
			assertionName, fileName, lineNumber);
#ifdef SABER
	stop();
#endif
	exit(-1);
}

