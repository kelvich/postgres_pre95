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

extern char *getenv();

void usage();

main(argc,argv)
	int argc;
	char **argv;
{
	int errors = 0;
	int dflag, ch;
	char *dbname;
	extern int optind;
	extern char *optarg;
	extern char *PQhost, *PQport;
	extern int p_errno;

	dflag = 0;
	while ((ch = getopt(argc, argv, "H:P:D:")) != EOF)
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
	    case '?':
	    default:
	      usage();
	  }

	if (!dflag) {
	    if ((dbname = getenv("DATABASE")) == (char *) NULL) {
		fprintf(stderr, "no database specified in env var DATABASE\n");
		fflush(stderr);
		exit (1);
	    }
	    PQsetdb(dbname);
	}
	{
	    extern int p_attr_caching;
	    p_attr_caching = 0;
	}

	if ((argc-optind) < 1) {
		PQfinish();
		usage();
	}

	(void) PQexec("begin");

	do {
	    if (p_rmdir(argv[optind]) < 0) {
		fprintf(stderr, "p_rmdir: p_errno=%d, ", p_errno);
		perror(argv[optind]);
		errors++;
	    }
	    optind++;
	} while (optind < argc);

	(void) PQexec("end");

	PQfinish();

	exit(errors != 0);
	/* NOTREACHED */
}

void
usage()
{
    fprintf(stderr, "usage: prmdir directory ...\n");
    exit(1);
}
