/*
 * $Header
 *	tests:
 *	- pass-by-reference fixed-size arguments and return-values
 *	- linking to C library
 */

#include "tmp/postgres.h"

Name
ufp2(n1, n2)
    Name n1, n2;
{
    Name newn = (Name) palloc(sizeof(NameData));

    if (!newn || !n1 || !n2)
	return((Name) NULL);
    
    (void) sprintf(newn->data, "%s%s", n1->data, n2->data);
    return(newn);
}
