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

namecpy(n1, n2)
    Name n1;
    Name n2;
{
    if (!n1 || !n2)
	return(-1);
    (void) strncpy(n1->data, n2->data, NAMEDATALEN);
    return(0);
}

namecat(n1, n2)
    Name n1;
    Name n2;
{
    return(namestrcat(n1, n2->data)); /* n2 can't be any longer than n1 */
}

namestrcpy(name, str)
    Name name;
    char *str;
{
    if (!name || !str)
	return(-1);
    bzero(name->data, sizeof(NameData));
    (void) strncpy(name->data, str, NAMEDATALEN);
    return(0);
}

namestrcat(name, str)
    Name name;
    char *str;
{
    int i;
    char *p, *q;

    if (!name || !str)
	return(-1);
    for (i = 0, p = name->data; i < NAMEDATALEN && *p; ++i, ++p)
	;
    for (q = str; i < NAMEDATALEN; ++i, ++p, ++q) {
	*p = *q;
	if (!*q)
	    break;
    }
    return(0);
}

namestrcmp(name, str)
    Name name;
    char *str;
{
    if (!name && !str)
	return(0);
    if (!name)
	return(-1);	/* NULL < anything */
    if (!str)
	return(1);	/* NULL < anything */
    return(strncmp(name->data, str, NAMEDATALEN));
}

	     /* ========== PRIVATE ROUTINES ========== */

/*
 * Note:
 *	This is the same code as char16eq.
 *	Assumes that "xy\0\0a" should be equal to "xy\0b".
 */
bool
NameIsEqual(name1, name2)
	Name	name1;
	Name	name2;
{
	if (! PointerIsValid(name1) || ! PointerIsValid(name2)) {
		return(false);
	}
	return((bool)(strncmp(name1->data, name2->data, NAMEDATALEN) == 0));
}

uint32
NameComputeLength(name)
	Name	name;
{
	char	*charP;
	int	length;

	Assert(NameIsValid(name));

	for (length = 0, charP = name->data;
	     length < NAMEDATALEN && *charP != '\0';
	     length++, charP++) {
	    ;
	}
	return (uint32)length;
}

