/*
 * dt.c --
 * 	Functions for the built-in type "dt".
 */

#include "c.h"
#include "postgres.h"

RcsId("$Header$");


	    /* ========== USER I/O ROUTINES ========== */


/*
 *	dtin		- converts "nseconds" to internal representation
 *
 *	XXX Should probably take some for other than just nseconds.
 */
int32
dtin(datetime)
	char	*datetime;
{
	extern long	atol();

	if (datetime == NULL)
		return((int32) NULL);
	return((int32) atol(datetime));
}

/*
 *	dtout		- converts internal form to "..."
 *
 *	XXX Currently, just creates an integer.
 */
char *
dtout(datetime)
	int32	datetime;
{
	char		*result;
	extern int	ltoa();

	result = palloc(12);	/* assumes sign, 10 digits max, '\0' */
	ltoa((long) datetime, result);
	return(result);
}


	     /* ========== PUBLIC ROUTINES ========== */

	 /* (see int.c for comparison/operation routines) */


	     /* ========== PRIVATE ROUTINES ========== */

			     /* (none) */
