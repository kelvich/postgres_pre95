/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1983 Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char origsccsid[] = "@(#)mkdir.c	5.7 (Berkeley) 5/31/90";
static char rcsid[] = "$Id$";
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "tmp/libpq-fs.h"


extern int errno;
extern int p_errno;
extern char *getenv();

main(argc, argv)
	int argc;
	char **argv;
{
	extern int optind;
	int ch, exitval, pflag, dflag;
	char * dbname;
	extern char *optarg;
	extern char *PQhost, *PQport;

	pflag = dflag = 0;
	while ((ch = getopt(argc, argv, "H:P:D:p")) != EOF)
		switch(ch) {
		case 'H':
			PQhost = optarg;
			break;
		case 'P':
			PQport = optarg;
			break;
		case 'D':
			PQsetdb(optarg);
			dflag = 1;
			break;
		case 'p':
			pflag = 1;
			break;
		case '?':
		default:
			usage();
		}

	if (!dflag) {
	    if ((dbname = getenv("DATABASE")) == (char *) NULL) {
		fprintf(stderr, "no database specified with -D option or in env var DATABASE\n");
		fflush(stderr);
		exit (1);
	    }
	    PQsetdb(dbname);
	}

	{
	    extern int p_attr_caching;
	    p_attr_caching = 0;
	}

	if (!*(argv += optind))
		usage();

	(void) PQexec("begin");

	for (exitval = 0; *argv; ++argv)
		if (pflag)
			exitval |= build(*argv);
		else if (p_mkdir(*argv, 0777) < 0) {
			(void)fprintf(stderr, "mkdir: %s: %s\n",
			    *argv, strerror(p_errno));
			exitval = 1;
		}

	(void) PQexec("end");

#if defined(PORTNAME_aix)
	/*
	 * XXX This is a crock.
	 * There's something broken such that the result of p_mkdir
	 * is sometimes not visible until after *two* xacts.  (This
	 * is not true of a simple p_creat.)  It is visible to the
	 * current xact and the N+2'th xact but not the N+1'th xact..
	 *
	 * Until I figure out what's going on, this is a cheap 
	 * workaround.
	 */
	(void) PQexec("begin");
	(void) PQexec(" ");
	(void) PQexec("end");
	(void) PQexec("begin");
	(void) PQexec(" ");
	(void) PQexec("end");
#endif /* PORTNAME_aix */

	PQfinish();
	exit(exitval);
}

build(path)
	char *path;
{
	register char *p;
	struct pgstat sb;
	int create, ch;

	for (create = 0, p = path;; ++p)
		if (!*p || *p  == '/') {
			ch = *p;
			*p = '\0';
			if (p_stat(path, &sb)) {
				if (p_errno != PENOENT || p_mkdir(path, 0777) < 0) {
					(void)fprintf(stderr, "mkdir: %s: %s\n",
					    path, strerror(p_errno));
					return(1);
				}
				create = 1;
			}
			if (!(*p = ch))
				break;
		}
	if (!create) {
		(void)fprintf(stderr, "mkdir: %s: %s\n", path,
		    strerror(EEXIST));
		return(1);
	}
	return(0);
}

usage()
{
	(void)fprintf(stderr, "usage: mkdir [-H host] [-P port] [-D database] [-p] dirname ...\n");
	exit(1);
}
