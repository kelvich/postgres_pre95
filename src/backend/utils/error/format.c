
/*
 * 
 * POSTGRES Data Base Management System
 * 
 * Copyright (c) 1988 Regents of the University of California
 * 
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for educational, research, and non-profit purposes and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of the University of California not be used in advertising
 * or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Permission to incorporate this
 * software into commercial products can be obtained from the Campus
 * Software Office, 295 Evans Hall, University of California, Berkeley,
 * Ca., 94720 provided only that the the requestor give the University
 * of California a free licence to any derived software for educational
 * and research purposes.  The University of California makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 * 
 */



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
