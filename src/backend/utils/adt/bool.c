
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
 * bool.c --
 * 	Functions for the built-in type "bool".
 */

#include "c.h"
#include "postgres.h"

RcsId("$Header$");


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
