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
 *	elog, AssertionFailed, form
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

#ifdef	_SBFSIZ
typedef unsigned char iochar;
#else
typedef char iochar;
#endif

#ifndef _IOSTRG
#define _IOSTRG	0
#endif

#ifndef	_NFILE
#define _NFILE (-1)
#endif

#define FormMaxSize	1024
#define FormMinSize	(FormMaxSize / 8)

static	char	FormBuf[FormMaxSize];
static	char	*FormBufP = FormBuf;

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
    return
	malloc(size);
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

    fprintf(stderr, "ExceptionalCondition called!\n");
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
elog()
{
    fprintf(stderr, "elog called!!\n");
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

/* ----------------
 *	form
 * ----------------
 */
/*VARARGS1*/
char *
#ifdef lint
form(fmt, va_alist)
	char	*fmt;
#else
form(va_alist)
#endif
    va_dcl
{
    va_list	args;
    char	*format;
    char	*str;
    FILE	fake;

#if !sprite
    /* ----------------
     *	non-sprite hacks
     * ----------------
     */
#ifdef	lint
    fmt = fmt;
#endif  /* lint */

    if (FormBufP + FormMinSize > FormBuf + FormMaxSize)
	FormBufP = FormBuf;

    fake._cnt  = ((FormBuf + FormMaxSize) - FormBufP) - 1;
    fake._base = fake._ptr = (iochar *)FormBufP;
    fake._flag = _IOWRT | _IOSTRG;
    fake._file = _NFILE;
#endif /* sprite */
    
    va_start(args);
    format = va_arg(args, char *);

#if !sprite
    /* ----------------
     *	when not in sprite use _doprnt.
     * ----------------
     */
    _doprnt(format, args, &fake);
    va_end(args);

#ifndef	lint
    (void) putc('\0', &fake);
#endif  /* lint */

    str = FormBufP;
    FormBufP += strlen(FormBufP) + 1;
    
#else /* sprite */
    /* ----------------
     *	when in sprite, use vsprintf.
     * ----------------
     */
    str = vsprintf(FormBuf, format, args);
    va_end(args);
#endif /* sprite */
    
    return (str);
}
