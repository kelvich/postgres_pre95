
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
 * name.c --
 *	Functions for the internal type "name".
 */

#include <string.h>

#include "c.h"

#include "name.h"

RcsId("$Header$");

	    /* ========== USER I/O ROUTINES ========== */

			     /* (none) */


	     /* ========== PUBLIC ROUTINES ========== */

	 /* (see char.c for comparison/operation routines) */



	     /* ========== PRIVATE ROUTINES ========== */
bool
NameIsValid(name)
	Name	name;
{
	return ((bool) PointerIsValid(name));
}

/*
 * Note:
 *	This is the same code as char16eq.
 *	Assumes that "xy\0\0a" should be equal to "xy\0b".
 *	If not, can do the comparison backwards for efficiency???
 */
bool
NameIsEqual(name1, name2)
	Name	name1;
	Name	name2;
{
	if (!PointerIsValid(name1) || !PointerIsValid(name2)) {
		return(false);
	}
	return((bool)(strncmp(name1, name2, 16) == 0));
}

uint32
NameComputeLength(name)
	Name	name;
{
	char	*charP;
	int	length;

	Assert(NameIsValid(name));

	for (length = 0, charP = (char *)name; length < 16 && *charP != '\0';
			length++, charP++) {
		;
	}
	return (uint32)length;
}

