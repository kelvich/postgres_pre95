#ifndef lint
static	char sccsid[] = "@(#)pwd.c 1.1 90/03/23 SMI"; /* from UCB 4.4 83/01/05 */
#endif
/*
 * pwd
 */
#include <stdio.h>
#include <sys/param.h>
#include "tmp/libpq-fs.h"

char *getwd();
extern char *getenv();

main()
{
	char pathname[MAXPATHLEN + 1];
	char *dbname;

	if ((dbname = getenv("DATABASE")) == (char *) NULL) {
	    fprintf(stderr, "no database specified in env var DATABASE\n");
	    fflush(stderr);
	    exit (1);
	}

	PQsetdb(dbname);
	
	if (p_getwd(pathname) == NULL) {
		fprintf(stderr, "ppwd: %s\n", pathname);
		exit(1);
	}
	printf("%s\n", pathname);
	PQfinish();
	exit(0);
	/* NOTREACHED */
}
