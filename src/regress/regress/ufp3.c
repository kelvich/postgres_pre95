/*
 * $Header$
 *	tests:
 *	- pass-by-reference variable-length arguments and return-values
 *	- linking to non-library functions (e.g., palloc)
 */

#include "tmp/postgres.h"

text *
ufp3(t, c)
    text *t;
    int c;
{
    text *newt;
    unsigned i, size;
    char *p;

    if (!t)
	return((text *) NULL);
    for (i = 0, p = VARDATA(t); i < VARSIZE(t) - sizeof(VARSIZE(t)); ++i)
	if (p[i] == c)
	    break;
    size = i + sizeof(VARSIZE(t));
    newt = (text *) palloc(size);
    if (!newt)
	return((text *) NULL);
    VARSIZE(newt) = size;
    (void) strncpy(VARDATA(newt), VARDATA(t), i);
    return(newt);
}
