/*
 * dt.c --
 * 	Functions for the built-in type "dt".
 */

#include "tmp/c.h"

RcsId("$Header$");

#include "utils/palloc.h"
#include "utils/builtins.h"


	    /* ========== USER I/O ROUTINES ========== */

/*
 *	dtin		- converts "nseconds" to internal representation
 *
 *	XXX Currently, just creates an integer.
 */
int32
dtin(datetime)
	char	*datetime;
{
	if (datetime == NULL)
		return((int32) 0);
	return((int32) atol(datetime));
}

/*
 *	dtout		- converts internal form to "..."
 *
 *	XXX assumes sign, 10 digits max, '\0'
 */
char *
dtout(datetime)
	int32	datetime;
{
	char		*result;

	result = (char *) palloc(12);
	Assert(result);
	ltoa((long) datetime, result);
	return(result);
}


	     /* ========== PUBLIC ROUTINES ========== */

	 /* (see int.c for comparison/operation routines) */


	     /* ========== PRIVATE ROUTINES ========== */

			     /* (none) */
