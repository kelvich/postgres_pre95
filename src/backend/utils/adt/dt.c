
/*
 * 
 * POSTGRES Data Base Management System
 * 
 * Copyright (c) 1988 Regents of the University of California
 * 
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for educational, research, and non-profit purposes and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of the University of California not be used in advertising
 * or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Permission to incorporate this
 * software into commercial products can be obtained from the Campus
 * Software Office, 295 Evans Hall, University of California, Berkeley,
 * Ca., 94720 provided only that the the requestor give the University
 * of California a free licence to any derived software for educational
 * and research purposes.  The University of California makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 * 
 */



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
