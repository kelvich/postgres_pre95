/*
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ken Smith of The State University of New York at Buffalo.
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
"@(#) Copyright (c) 1989 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)mv.c	5.11 (Berkeley) 4/3/91";
#endif /* not lint */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
/*#include <stdlib.h>*/
#include <strings.h>
#include "pathnames.h"
#include "tmp/libpq-fs.h"

int fflg, iflg;
extern int p_errno;
extern char *getenv();

main(argc, argv)
	int argc;
	char **argv;
{
	extern char *optarg;
	extern int optind;
	register int baselen, exitval, len;
	register char *p, *endp;
	struct pgstat sb;
	int ch;
	int dflag = 0;
	char path[MAXPATHLEN + 1];
	extern char *PQhost, *PQport;

	while (((ch = getopt(argc, argv, "H:P:D:-if")) != EOF))
		switch((char)ch) {
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
		case 'i':
			iflg = 1;
			break;
		case 'f':
			fflg = 1;
			break;
		case '-':		/* undocumented; for compatibility */
			goto endarg;
		case '?':
		default:
			usage();
		}
endarg:	argc -= optind;
	argv += optind;

	if (argc < 2)
		usage();

	if (!dflag) {
	    char *dbname;
	    if ((dbname = getenv("DATABASE")) == (char *) NULL) {
		fprintf(stderr, "no database specified with -D option or in env var DATABASE\n");
		fflush(stderr);
		exit (1);
	    }
	    PQsetdb(dbname);
	}

	(void) PQexec("begin");

	/*
	 * If the stat on the target fails or the target isn't a directory,
	 * try the move.  More than 2 arguments is an error in this case.
	 */
	if (p_stat(argv[argc - 1], &sb) || !S_ISDIR(sb.st_mode)) {
		if (argc > 2)
			usage();
		exit(do_move(argv[0], argv[1]));
	}

	/* It's a directory, move each file into it. */
	(void)strcpy(path, argv[argc - 1]);
	baselen = strlen(path);
	endp = &path[baselen];
	*endp++ = '/';
	++baselen;
	for (exitval = 0; --argc; ++argv) {
		if ((p = rindex(*argv, '/')) == NULL)
			p = *argv;
		else
			++p;
		if ((baselen + (len = strlen(p))) >= MAXPATHLEN)
			(void)fprintf(stderr,
			    "mv: %s: destination pathname too long\n", *argv);
		else {
			bcopy(p, endp, len + 1);
			exitval |= do_move(*argv, path);
		}
	}

	if (!exitval)
	    (void) PQexec("end");

	PQfinish();
	exit(exitval);
}

do_move(from, to)
	char *from, *to;
{
	struct pgstat sb;
	int ask, ch, exists = 0;

	/*
	 * Check access.  If interactive and file exists, ask user if it
	 * should be replaced.  Otherwise if file exists but isn't writable
	 * make sure the user wants to clobber it.
	 */
	if (!fflg && /*!access(to, F_OK)*/ (exists = (p_stat(to,&sb) >= 0))) {
		ask = 0;
		if (iflg) {
			(void)fprintf(stderr, "overwrite %s? ", to);
			ask = 1;
		}
#if 0
		else if (access(to, W_OK) && !stat(to, &sb)) {
			(void)fprintf(stderr, "override mode %o on %s? ",
			    sb.st_mode & 07777, to);
			ask = 1;
		}
#endif
		if (ask) {
			if ((ch = getchar()) != EOF && ch != '\n')
				while (getchar() != '\n');
			if (ch != 'y')
				return(0);
		}
	}
	if (!p_rename(from, to))
		return(0);
        Perror2(p_errno == PENOENT && exists == 0 ? to : from,
                "rename");
        return (1);

#if 0
	if (errno != EXDEV) {
		(void)fprintf(stderr,
		    "mv: rename %s to %s: %s\n", from, to, strerror(errno));
		return(1);
	}

	/*
	 * If rename fails, and it's a regular file, do the copy internally;
	 * otherwise, use cp and rm.
	 */
	if (p_stat(from, &sb)) {
		(void)fprintf(stderr, "mv: %s: %s\n", from, strerror(errno));
		return(1);
	}
	return(S_ISREG(sb.st_mode) ?
	    fastcopy(from, to, &sb) : copy(from, to));
#endif
}

error(s)
	char *s;
{
	if (s)
		(void)fprintf(stderr, "mv: %s: %s\n", s, strerror(errno));
	else
		(void)fprintf(stderr, "mv: %s\n", strerror(errno));
}

usage()
{
	(void)fprintf(stderr,
"usage: mv [-H host] [-P port] [-D database] [-if] src target;\n   or: mv [-if] src1 ... srcN directory\n");
	exit(1);
}

Perror2(s1, s2)
	char *s1, *s2;
{
	char buf[MAXPATHLEN + 20];

	sprintf(buf, "mv: %s: %s", s1, s2);
	perror(buf);
}

