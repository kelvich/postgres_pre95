/*
 * $Header$
 *	tests:
 *	- 0 arguments
 *	- 1 argument
 *	- pass-by-value arguments and return-values
 *	- multiple functions per file
 */

ufp0()
{
    return(42);
}

ufp1(a)
    int a;
{
    return(a + 1);
}
