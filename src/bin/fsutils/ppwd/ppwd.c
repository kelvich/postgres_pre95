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

main()
{
	char pathname[MAXPATHLEN + 1];
	
	PQsetdb(getenv("USER"));

	if (p_getwd(pathname) == NULL) {
		fprintf(stderr, "ppwd: %s\n", pathname);
		exit(1);
	}
	printf("%s\n", pathname);
	PQfinish();
	exit(0);
	/* NOTREACHED */
}
