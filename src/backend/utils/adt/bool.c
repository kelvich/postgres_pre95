/*
 * bool.c --
 * 	Functions for the built-in type "bool".
 */

#include "c.h"

RcsId("$Header$");

#include "palloc.h"

	    /* ========== USER I/O ROUTINES ========== */


/*
 *	boolin		- converts "t" or "f" to 1 or 0
 */
int32
boolin(b)
	char	*b;
{
	if (b == NULL)
		return((int32) NULL);
	return((int32) (*b == 't'));
}

/*
 *	boolout		- converts 1 or 0 to "t" or "f"
 */
char *
boolout(b)
	long	b;
{
	char	*result = palloc(2);

	*result = (b) ? 't' : 'f';
	result[1] = '\0';
	return(result);
}


	     /* ========== PUBLIC ROUTINES ========== */

	 /* (see int.c for comparison/operation routines) */


	     /* ========== PRIVATE ROUTINES ========== */


/*
 *	boolIsValid
 */
bool
boolIsValid(b)
	bool	b;
{
	return((bool) (b == false || b == true));
}
