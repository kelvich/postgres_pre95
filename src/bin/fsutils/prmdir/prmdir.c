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


/*
 * Remove directory
 */
#include <stdio.h>
#include "tmp/libpq-fs.h"

main(argc,argv)
	int argc;
	char **argv;
{
	int errors = 0;

	PQsetdb(getenv("USER"));
	(void) PQexec("begin");

	if (argc < 2) {
		fprintf(stderr, "usage: %s directory ...\n", argv[0]);
		PQfinish();
		exit(1);
	}

	while (--argc) {
		if (p_rmdir(*++argv) < 0) {
			fprintf(stderr, "p_rmdir: ");
			perror(*argv);;
			errors++;
		}
	}

	if (errors == 0)
		(void) PQexec("end");
	else
		(void) PQexec("abort");

	PQfinish();

	exit(errors != 0);
	/* NOTREACHED */
}
