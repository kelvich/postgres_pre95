/* ----------------------------------------------------------------
 *   FILE
 *	fe-pqstubs.c
 *	
 *   DESCRIPTION
 *	These are frontend versions of various error handling and
 *	memory mgt routines used in libpq.  
 *
 *   INTERFACE ROUTINES
 *	palloc, pfree, ExceptionalCondition, ExcRaise
 *	elog, AssertionFailed
 *	
 *   NOTES
 *	These routines are NOT compiled into the postgres backend,
 *	rather they end up in libpq.a.
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */


#include <stdio.h>
#include <sys/types.h>
#include <varargs.h>

#include "tmp/c.h"
#undef palloc
#undef pfree
#include "utils/exc.h"

RcsId("$Header$");

Exception FailedAssertion;

/* ----------------
 *	palloc (frontend version)
 *	pfree (frontend version)
 * ----------------
 */
Pointer
palloc(size)
    Size size;
{
    Pointer p;

    p = malloc(size);
    if (!p)
	return((Pointer) NULL);
    bzero(p, size);
    return(p);
}

void
pfree(pointer)
    Pointer	pointer;
{
    free(pointer);
}

Pointer
palloc_debug(file, line, size)
    String file;
    int	   line;
    Size   size;
{
    return
	malloc(size);
}

void
pfree_debug(file, line, pointer)
    String  file;
    int	    line;
    Pointer pointer;
{
    free(pointer);
}

/* ----------------
 *	ExceptionalCondition (frontend version)
 * ----------------
 */
void
ExceptionalCondition(conditionName, exceptionP, detail, fileName, lineNumber)
    const String    conditionName;
    const Exception *exceptionP;
    const String    detail;
    const String    fileName;
    const int       lineNumber;
{
    /*
     * for picky compiler purposes
     */
    conditionName = conditionName;
    exceptionP = exceptionP;
    detail = detail;
    fileName = fileName;
    lineNumber = lineNumber;

    fprintf(stderr, "ExceptionalCondition called (%s,%s,file %s, line %d)!\n",conditionName,detail,fileName,lineNumber);
    exit(1);
}

/* ----------------
 *	ExcRaise (frontend version)
 * ----------------
 */
void
ExcRaise(arg1, string,ignore,ignore2)
     Exception *arg1;
     ExcDetail string;
     ExcData ignore;
     ExcMessage ignore2;
{
    fprintf(stderr, "Error: %s\n", (char *)string);
    exit(1);
}

/* ----------------
 *	elog (frontend version)
 * ----------------
 */
void
elog(va_alist)
    va_dcl
{
    va_list ap;
    int lev;
    char *fmt;

    va_start(ap);
    lev = va_arg(ap, int);
    fmt = va_arg(ap, char *);
    va_end(ap);
    fprintf(stderr, "FATAL: error level %d: %s\n", lev, fmt);
    if (lev == FATAL)
	exit(1);
}

/* ----------------
 *	AssertionFailed (frontend version)
 * ----------------
 */
void
AssertionFailed(assertionName, fileName, lineNumber)
    const String	assertionName;
    const String	fileName;
    const int		lineNumber;
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
    exit(1);
}
