/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1983 Regents of the University of California.\n\
 All rights reserved.\n";
#endif not lint

#ifndef lint
static	char sccsid[] = "@(#)mkdir.c 1.1 90/03/23 SMI"; /* from UCB 5.1 4/30/85 */
#endif not lint

/*
 * make directory
 */
#include <stdio.h>
#include <sys/errno.h>
#include "tmp/libpq-fs.h"

int mkdir();
int mkdirp();

main(argc, argv)
	char *argv[];
{
	int errors = 0;
	int pflag = 0;
	char *cmd = argv[0];

	PQsetdb(getenv("USER"));
	argc--, argv++;
	while (argc > 0 && **argv == '-') {
		(*argv)++;
		while (**argv) switch (*(*argv)++) {

		case 'p':
			pflag++;
			break;

		default:
			errors++;
		}
		argc--; argv++;
	}
	if (argc < 1 || errors) {
		fprintf(stderr, "usage: %s [ -p ]  directory ...\n", cmd);
		PQfinish();
		exit(1);
	}
	while (argc--)
		if ((pflag ? mkdirp : p_mkdir)(*argv++, 0777) < 0) {
			fprintf(stderr, "pmkdir: ");
			perror(*(argv-1));
			errors++;
		}
	PQfinish();
	exit(errors != 0);
	/* NOTREACHED */
}

int
mkdirp(dir, mode)
	char *dir;
	int mode;
{
	int err;
	char *slash;
	char *rindex();
	extern int errno;

	if (p_mkdir(dir, mode) == 0)
		return (0);
/*	if (errno != ENOENT)
		return (-1); */
	slash = rindex(dir, '/');
	if (slash == NULL)
		return (-1);
	*slash = '\0';
	err = mkdirp(dir, 0777);
	*slash++ = '/';
	if (err || !*slash)
		return (err);
	return p_mkdir(dir, mode);
}
