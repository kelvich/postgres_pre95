/*
 * name.c --
 *	Functions for the internal type "name".
 */

#include <string.h>

#include "tmp/postgres.h"

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

