#ifndef lint
static	char sccsid[] = "@(#)pwd.c 1.1 90/03/23 SMI"; /* from UCB 4.4 83/01/05 */
#endif
/*
 * pwd
 */
#include <stdio.h>
#include <sys/param.h>
#include "tmp/libpq-fs.h"

extern char *getwd();
extern char *getenv();

main(argc,argv)
char *argv[];
int argc;
{
	char pathname[MAXPATHLEN + 1];
	char *dbname;

	if ((dbname = getenv("DATABASE")) == (char *) NULL) {
	    fprintf(stderr, "no database specified in env var DATABASE\n");
	    fflush(stderr);
	    exit (1);
	}

	PQsetdb(dbname);
	
	if (argc > 1 || argc == 0) {
		fprintf(stderr,"wrong # of arguments.\nusage: %s directory\n",argv[0]);
		exit(1);
	} else if (argc == 1) {
		argv[1] = "/";
	}

	if (p_chdir(argv[1]) < 0) {
		fprintf(stderr, "pcd: %s\n", pathname);
		exit(1);
	}
	p_getwd(pathname);
	printf("setenv PFCWD %s\n",pathname);
	PQfinish();
	exit(0);
	/* NOTREACHED */
}
