/* RcsId("$Header$"); */

#include <sys/file.h>
#include "tmp/c.h"
#include "tmp/postgres.h"
#include "tmp/datum.h"
#include "catalog/pg_type.h"

/*-------------------------------------------------------------------------
 * Check if data is Null 
 */


bool NullValue(value, isNull)
Datum value;
Boolean *isNull;
{
	if (*isNull) {
		*isNull = false;
		return(true);
	}
	return(false);
	
}

/*----------------------------------------------------------------------*
 *     check if data is not Null                                        *
 *--------------------------------------------------------------------- */

bool NonNullValue(value, isNull)
Datum value;
Boolean *isNull;
{
	if (*isNull) {
		*isNull = false;
		return(false);
	}
	return(true);
		
}

int32
userfntest(i)
    int i;
{
    return (i);
}
