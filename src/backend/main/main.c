/*---------------------------------------------------------------
 *   FILE
 *	main.c
 *
 *   DESCRIPTION
 *	Stub main() routine for the postgres backend.
 *
 *   NOTES
 *	Old comments:
 *	The reason that `main()' is defined in a separate file in a
 *	separate directory is to be able to easily link all the postgres
 *	code (as opposed to just cinterface.a) with test programs (which
 *	have their own `main').
 *
 *   IDENTIFICATION
 *	$Header$
 *---------------------------------------------------------------
 */

#include "tmp/c.h"

RcsId("$Header$");

#include <string.h>

char *DataDir;
extern char *GetPGData();

main(argc, argv)
	int argc;
	char *argv[];
{
#if defined(NOFIXADE) || defined(NOPRINTADE)
	/*
	 * Must be first so that the bootstrap code calls it, too.
	 * (Only needed on some RISC architectures.)
	 */
	init_address_fixup();
#endif /* NOFIXADE || NOPRINTADE */

	/* 
	 * set up DataDir here; it's used by the bootstrap and regular systems 
	 */
	DataDir = GetPGData();
	if (argc > 1 && strcmp(argv[1], "-boot") == 0)
		BootstrapMain(argc, argv);
	else
		PostgresMain(argc, argv);
}
