/* $Header$ */
/*
 * oidchar16.c --
 *
 *	adt for multiple key indices involving oid and char16.  Used for cache
 *	index scans (could also be used in the general case with char16).
 */

#include <strings.h>

#include "tmp/postgres.h"
#include "utils/oidcompos.h"
#include "utils/log.h"


OidChar16
oidchar16in(inStr)
    char *inStr;
{
    OidChar16 oc;
    char *inptr;

    oc = (OidChar16) palloc(sizeof(OidChar16Data));

    bzero(oc, sizeof(OidChar16Data));
    for (inptr = inStr; *inptr && *inptr != ','; inptr++)
	;

    if (*inptr)
    {
	oc->id = atoi(inStr);
	(void) strncpy(oc->name.data, ++inptr, sizeof(char16));
    }
    else
	elog(WARN, "Bad input data for type oidchar16");

    return oc;
}

char *
oidchar16out(oidname)
    OidChar16 oidname;
{
    /*
     * 4294967295,char16identifier
     * 0        1         2
     * 1234567890123456789012345678
     */
    char *s, buf[28];

    sprintf(buf, "%d,%.*s", oidname->id, sizeof(char16), oidname->name.data);
    return(strcpy(palloc(strlen(buf) + 1), buf));
}

bool
oidchar16lt(o1, o2)
    OidChar16 o1, o2;
{
    return (bool)
	(o1->id < o2->id ||
	 (o1->id == o2->id && strncmp(&o1->name.data[0],
				      &o2->name.data[0],
				      sizeof(char16)) < 0));
}

bool
oidchar16le(o1, o2)
    OidChar16 o1, o2;
{
    return (bool)
	(o1->id < o2->id ||
    	 (o1->id == o2->id && strncmp(&o1->name.data[0],
				      &o2->name.data[0],
				      sizeof(char16)) <= 0));
}

bool
oidchar16eq(o1, o2)
    OidChar16 o1, o2;
{
    return (bool)
	(o1->id == o2->id &&
    	 (strncmp(&o1->name.data[0], &o2->name.data[0], sizeof(char16)) == 0));
}

bool
oidchar16ne(o1, o2)
    OidChar16 o1, o2;
{
    return (bool)
	(o1->id != o2->id ||
    	 (strncmp(&o1->name.data[0], &o2->name.data[0], sizeof(char16)) != 0));
}

bool
oidchar16ge(o1, o2)
    OidChar16 o1, o2;
{
    return (bool) (o1->id > o2->id || (o1->id == o2->id && 
					strncmp(&o1->name.data[0],
						&o2->name.data[0],
						sizeof(char16)) >= 0));
}

bool
oidchar16gt(o1, o2)
    OidChar16 o1, o2;
{
    return (bool) (o1->id > o2->id ||  (o1->id == o2->id && 
					strncmp(&o1->name.data[0],
						&o2->name.data[0],
						sizeof(char16)) > 0));
}

int
oidchar16cmp(o1, o2)
    OidChar16 o1, o2;
{
    if (o1->id == o2->id)
    	return (strncmp(&o1->name.data[0], &o2->name.data[0], sizeof(char16)));

    return (o1->id < o2->id) ? -1 : 1;
}

OidChar16
mkoidchar16(id, name)
    ObjectId id;
    char *name;
{
    OidChar16 oidchar16;

    oidchar16 = (OidChar16) palloc(sizeof(ObjectId)+sizeof(char16));

    oidchar16->id = id;
    (void) strncpy(oidchar16->name.data, name, sizeof(char16));
    return oidchar16;
}
