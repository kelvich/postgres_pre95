
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
 * char.c --
 * 	Functions for the built-in type "char".
 * 	Functions for the built-in type "char16".
 */

#include "c.h"
#include "postgres.h"

#include <strings.h>

RcsId("$Header$");


	    /* ========== USER I/O ROUTINES ========== */


/*
 *	charin		- converts "x" to 'x'
 */
int32
charin(ch)
	char	*ch;
{
	if (ch == NULL)
		return((int32) NULL);
	return((int32) *ch);
}

/*
 *	charout		- converts 'x' to "x"
 */
char *
charout(ch)
	int32	ch;
{
	char	*result = palloc(2);

	result[0] = (char) ch;
	result[1] = '\0';
	return(result);
}

/*
 *	char16in	- converts "..." to internal reprsentation
 *
 *	Note:
 *		Currently if strlen(s) < 14, the extra chars are garbage.
 */
char *
char16in(s)
	char	*s;
{
	char	*result;

	if (s == NULL)
		return(NULL);
	result = palloc(16);
	strncpy(result, s, 16);
	return(result);
}

/*
 *	char16out	- converts internal reprsentation to "..."
 */
char *
char16out(s)
	char	*s;
{
	char	*result = palloc(17);

	if (s == NULL) {
		result[0] = '-';
		result[1] = '\0';
	} else {
		strncpy(result, s, 16);
		result[16] = '\0';
	}
	return(result);
}


	     /* ========== PUBLIC ROUTINES ========== */


/*
 *	char16eq	- returns 1 iff arguments are equal
 *	char16ne	- returns 1 iff arguments are not equal
 *
 *	BUGS:
 *		Assumes that "xy\0\0a" should be equal to "xy\0b".
 *		If not, can do the comparison backwards for efficiency.
 */
int32
char16eq(arg1, arg2)
	char	*arg1, *arg2;
{

	if (arg1 == NULL || arg2 == NULL)
		return((int32) NULL);
	return((int32) (strncmp(arg1, arg2, 16) == 0));
/*
	register char	*a1p, *a2p;
	
	a1p = arg1 + 15;
	a2p = arg2 + 15;
	while (*a1p-- == *a2p--)
		if (a1p < arg1)
			return(1L);
	return(0L);
*/
}

int32
char16ne(arg1, arg2)
	char	*arg1, *arg2;
{
	return((int32) !char16eq(arg1, arg2));
}


	     /* ========== PRIVATE ROUTINES ========== */

			     /* (none) */
