/*
 *  oidint2.c --
 * 	Functions for the built-in type "oidint2".
 */

#include "tmp/c.h"
#include "tmp/postgres.h"

#include "utils/palloc.h"


OidInt2
oidint2in(o)
	char *o;
{
	OidInt2 oi;
	char *p;

	oi = (OidInt2) palloc(sizeof(OidInt2Data));

	for (p = o; *p != '\0' && *p != '/'; p++)
		continue;
    
	oi->oi_oid = (ObjectId) pg_atoi(o, sizeof(ObjectId), '/');
	if (*p == '\0') {
		oi->oi_int2 = 0;
	} else {
		oi->oi_int2 = (int16) pg_atoi(++p, sizeof(int2), '\0');
	}

	return (oi);
}

char *
oidint2out(o)
	OidInt2 o;
{
	char *r;

	/*
	 * -2147483647/-32767
	 * 0        1
	 * 1234567890123456789
	 */
	r = (char *) palloc(19);
	sprintf(r, "%d/%d", o->oi_oid, o->oi_int2);

	return (r);
}

bool
oidint2lt(o1, o2)
	OidInt2 o1, o2;
{
	return
	    ((bool) (o1->oi_oid < o2->oi_oid ||
	            (o1->oi_oid == o2->oi_oid && o1->oi_int2 < o2->oi_int2)));
}

bool
oidint2le(o1, o2)
	OidInt2 o1, o2;
{
	return ((bool) (o1->oi_oid < o2->oi_oid ||
		       (o1->oi_oid == o2->oi_oid && o1->oi_int2 <= o2->oi_int2)));
}

bool
oidint2eq(o1, o2)
	OidInt2 o1, o2;
{
	return ((bool) (o1->oi_oid == o2->oi_oid && o1->oi_int2 == o2->oi_int2));
}

bool
oidint2ge(o1, o2)
	OidInt2 o1, o2;
{
	return ((bool) (o1->oi_oid > o2->oi_oid ||
		       (o1->oi_oid == o2->oi_oid && o1->oi_int2 >= o2->oi_int2)));
}

bool
oidint2gt(o1, o2)
	OidInt2 o1, o2;
{
	return ((bool) (o1->oi_oid > o2->oi_oid ||
		       (o1->oi_oid == o2->oi_oid && o1->oi_int2 > o2->oi_int2)));
}

bool
oidint2ne(o1, o2)
	OidInt2 o1, o2;
{
	return ((bool) (o1->oi_oid != o2->oi_oid || o1->oi_int2 != o2->oi_int2));
}

int
oidint2cmp(o1, o2)
	OidInt2 o1, o2;
{
	if (oidint2lt(o1, o2))
		return (-1);
	else if (oidint2eq(o1, o2))
		return (0);
	else
		return (1);
}

OidInt2
mkoidint2(v_oid, v_int2)
	ObjectId v_oid;
	uint16 v_int2;
{
	OidInt2 o;

	o = (OidInt2) palloc(sizeof(OidInt2Data));
	o->oi_oid = v_oid;
	o->oi_int2 = v_int2;
	return (o);
}

