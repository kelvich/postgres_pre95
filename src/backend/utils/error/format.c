/* ----------------------------------------------------------------
 *   FILE
 *	format.c
 *	
 *   DESCRIPTION
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
