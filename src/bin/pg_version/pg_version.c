
/* $Header$ */

#include <stdio.h>

int Noversion = 0;
char *DataDir = (char *) NULL;

main(argc, argv)

int argc;
char **argv;

{
	char *path = argv[1];

    SetPgVersion(path);
}

elog()

{}

GetDataHome()

{
	return(NULL);
}
