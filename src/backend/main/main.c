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

main(argc, argv)
int argc;
char *argv[];
{
    PostgresMain(argc, argv);
}
