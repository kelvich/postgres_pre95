
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
 * oid.c --
 * 	Functions for the built-in type "ObjectId".
 */

#include "c.h"
#include "postgres.h"

#include "oid.h"

RcsId("$Header$");


	    /* ========== USER I/O ROUTINES ========== */


/*
 *	oid8in		- converts "num num ..." to internal form
 *
 *	Note:
 *		Fills any nonexistent digits with NULL oids.
 */
ObjectId *
oid8in(oidString)
	char	*oidString;
{
	register ObjectId	(*result)[];
	int			nums;

	if (oidString == NULL)
		return(NULL);
	result = (ObjectId (*)[]) palloc(sizeof(ObjectId [8]));
	if ((nums = sscanf(oidString, "%ld%ld%ld%ld%ld%ld%ld%ld",
			   *result,
			   *result + 1,
			   *result + 2,
			   *result + 3,
			   *result + 4,
			   *result + 5,
			   *result + 6,
			   *result + 7)) != 8) {
		do
			(*result)[nums++] = 0;
		while (nums < 8);
	}
	return((ObjectId *) result);
}

/*
 *	oid8out	- converts internal form to "num num ..."
 */
char *
oid8out(oidArray)
	ObjectId	(*oidArray)[];
{
	register int		num;
	register ObjectId	*sp;
	register char		*rp;
	char			*result;
	extern int		ltoa();

	if (oidArray == NULL) {
		result = palloc(2);
		result[0] = '-';
		result[1] = '\0';
		return(result);
	}
	rp = result = palloc(8 * 12);	/* assumes sign, 10 digits, ' ' */
	sp = *oidArray;
	for (num = 8; num != 0; num--) {
		ltoa((long) *sp++, rp);
		while (*++rp != '\0')
			;
		*rp++ = ' ';
	}
	*--rp = '\0';
	return(result);
}


	     /* ========== PUBLIC ROUTINES ========== */

	 /* (see int.c for comparison/operation routines) */


	     /* ========== PRIVATE ROUTINES ========== */


bool
ObjectIdIsValid(objectId)
	ObjectId	objectId;
{
	return ((bool) (objectId != InvalidObjectId));
}
