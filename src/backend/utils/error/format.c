/*LINTLIBRARY*/

#ifndef lint
#ifdef	SCCS
static char sccsid[] = "%W% (serge) %G%";
#endif	/* defined(SCCS) */
#ifdef	RCS
static char rcsid[] = "$Header$";
#endif	/* defined(RCS) */
#endif	/* !defined(lint) */

#include <varargs.h>
#include <stdio.h>

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

/*VARARGS1*/
char *
#ifdef	lint
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

#ifdef	lint
	fmt = fmt;
#endif

	if (FormBufP + FormMinSize > FormBuf + FormMaxSize)
		FormBufP = FormBuf;

	fake._cnt  = ((FormBuf + FormMaxSize) - FormBufP) - 1;
	fake._base = fake._ptr = (iochar *)FormBufP;
	fake._flag = _IOWRT | _IOSTRG;
	fake._file = _NFILE;

	va_start(args);

	format = va_arg(args, char *);

	_doprnt(format, args, &fake);

	va_end(args);

#ifndef	lint
	(void) putc('\0', &fake);
#endif

	str = FormBufP;

	FormBufP += strlen(FormBufP) + 1;

	return (str);
}
