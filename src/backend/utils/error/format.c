/* ----------------------------------------------------------------
 *   FILE
 *	format.c
 *	
 *   DESCRIPTION
 *	a wrapper around code that does what vsprintf does.
 *
 *   INTERFACE ROUTINES
 *	form
 *
 *   NOTES
 *	If this file is changed, make sure to change
 *	lib/libpq/fe-pqstubs.c file too.  That is the copy
 *	that gets compiled into libpq.a
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

/*LINTLIBRARY*/

#include <stdio.h>
#include <varargs.h>
#include "tmp/c.h"

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
#ifdef NO_VSPRINTF
    FILE	fake;

#ifdef	lint
    fmt = fmt;
#endif  /* lint */

    if (FormBufP + FormMinSize > FormBuf + FormMaxSize)
	FormBufP = FormBuf;

    fake._cnt  = ((FormBuf + FormMaxSize) - FormBufP) - 1;
    fake._base = fake._ptr = (iochar *) FormBufP;
    fake._flag = _IOWRT | _IOSTRG;
    fake._file = _NFILE;
#endif /* NO_VSPRINTF */
    
    va_start(args);
    format = va_arg(args, char *);

    str = FormBufP;
#ifdef NO_VSPRINTF
    _doprnt(format, args, &fake);
#ifndef	lint
    (void) putc('\0', &fake);
#endif  /* lint */
    FormBufP += strlen(FormBufP) + 1;
#else /* NO_VSPRINTF */
    (void) vsprintf(FormBuf, format, args);
#endif /* NO_VSPRINTF */
    va_end(args);
    
    return (str);
}
