/*
 *  oidint4.c --
 * 	Functions for the built-in type "oidint4".
 */

#include "tmp/c.h"
#include "tmp/postgres.h"

RcsId("$Header$");

#include "utils/palloc.h"


OidInt4
oidint4in(o)
	char *o;
{
	OidInt4 oi;
	char *p;

	oi = (OidInt4) palloc(sizeof(OidInt4Data));

	for (p = o; *p != '\0' && *p != '/'; p++)
		continue;
    
	oi->oi_oid = atoi(o);
	if (*p == '\0') {
		oi->oi_int4 = 0;
	} else {
		oi->oi_int4 = atoi(++p);
	}

	return (oi);
}

char *
oidint4out(o)
	OidInt4 o;
{
	char *r;

	r = (char *) palloc(20);
	sprintf(r, "%d/%ld", o->oi_oid, o->oi_int4);

	return (r);
}

bool
oidint4lt(o1, o2)
	OidInt4 o1, o2;
{
	return
	    ((bool) (o1->oi_oid < o2->oi_oid ||
	            (o1->oi_oid == o2->oi_oid && o1->oi_int4 < o2->oi_int4)));
}

bool
oidint4le(o1, o2)
	OidInt4 o1, o2;
{
	return ((bool) (o1->oi_oid < o2->oi_oid ||
		       (o1->oi_oid == o2->oi_oid && o1->oi_int4 <= o2->oi_int4)));
}

bool
oidint4eq(o1, o2)
	OidInt4 o1, o2;
{
	return ((bool) (o1->oi_oid == o2->oi_oid && o1->oi_int4 == o2->oi_int4));
}

bool
oidint4ge(o1, o2)
	OidInt4 o1, o2;
{
	return ((bool) (o1->oi_oid > o2->oi_oid ||
		       (o1->oi_oid == o2->oi_oid && o1->oi_int4 >= o2->oi_int4)));
}

bool
oidint4gt(o1, o2)
	OidInt4 o1, o2;
{
	return ((bool) (o1->oi_oid > o2->oi_oid ||
		       (o1->oi_oid == o2->oi_oid && o1->oi_int4 > o2->oi_int4)));
}

bool
oidint4ne(o1, o2)
	OidInt4 o1, o2;
{
	return ((bool) (o1->oi_oid != o2->oi_oid || o1->oi_int4 != o2->oi_int4));
}

bool
oidint4cmp(o1, o2)
	OidInt4 o1, o2;
{
	if (oidint4lt(o1, o2))
		return (-1);
	else if (oidint4eq(o1, o2))
		return (0);
	else
		return (1);
}

OidInt4
mkoidint4(v_oid, v_int4)
	ObjectId v_oid;
	uint32 v_int4;
{
	OidInt4 o;

	o = (OidInt4) palloc(sizeof(OidInt4Data));
	o->oi_oid = v_oid;
	o->oi_int4 = v_int4;
	return (o);
}
