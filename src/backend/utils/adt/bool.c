/*
 * bool.c --
 * 	Functions for the built-in type "bool".
 */

#include "tmp/c.h"
#include "utils/log.h"

RcsId("$Header$");

#include "utils/palloc.h"

	    /* ========== USER I/O ROUTINES ========== */


/*
 *	boolin		- converts "t" or "f" to 1 or 0
 */
int32
boolin(b)
	char	*b;
{
	if (b == NULL)
		elog(WARN, "Bad input string for type bool");
	return((int32) (*b == 't') || (*b == 'T'));
}

/*
 *	boolout		- converts 1 or 0 to "t" or "f"
 */
char *
boolout(b)
	long	b;
{
	char	*result = (char *) palloc(2);

	*result = (b) ? 't' : 'f';
	result[1] = '\0';
	return(result);
}


	     /* ========== PUBLIC ROUTINES ========== */

int32 booleq(arg1, arg2)	int8 arg1, arg2; { return(arg1 == arg2); }
int32 boolne(arg1, arg2)	int8 arg1, arg2; { return(arg1 != arg2); }

	     /* ========== PRIVATE ROUTINES ========== */

