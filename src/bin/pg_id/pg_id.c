/*
 * $Header$
 */

/*
 * Print the user ID for the login name passed as argument,
 * or the real user ID of the caller if no argument.  If the
 * login name doesn't exist, print "NOUSER" and exit 1.
 */
#include <sys/types.h>
#include <pwd.h>
#include <stdio.h>

main(argc, argv)
	char **argv;
{
	struct passwd *pw;
	int ch;
	extern int optind;

	while ((ch = getopt(argc, argv, "")) != EOF)
		switch (ch) {
		case '?':
		default:
			fprintf(stderr, "usage: pg_id [login]\n");
			exit(1);
		}
	argc -= optind;
	argv += optind;

	if (argc > 0) {
		if (argc > 1) {
                        fprintf(stderr, "usage: pg_id [login]\n");
                        exit(1);
		}
		if ((pw = getpwnam(argv[0])) == NULL) {
			printf("NOUSER\n");
			exit(1);
		}
		printf("%d\n", pw->pw_uid);
	} else {
		printf("%d\n", getuid());
	}

	exit(0);
}
