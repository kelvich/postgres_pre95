/*---------------------------------------------------------------
 *
 * FILE: main.c
 *
 * $Header$
 *
 * The reason that `main()' is defined in a separate file in a
 * separate directory is to be able to easily link all the postgres
 * code (as opposed to just cinterface.a) with test programs (which
 * have their own `main').
 *
 */

char *DataDir;
extern char *GetPGData();

main(argc, argv)
int argc;
char *argv[];
{
    /* set up DataDir here; it's used by the bootstrap and regular systems */
    DataDir = GetPGData();

    if (argc > 1)
    {
    	if (!strcmp(argv[1], "-boot"))
	{
	    BootstrapMain(argc, argv);
        }
        else
        {
	    PostgresMain(argc, argv);
        }
    }
    else
    {
        PostgresMain(argc, argv);
    }
}
