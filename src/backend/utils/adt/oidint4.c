/*
 *  oidseq.c --
 * 	Functions for the built-in type "oidseq".
 */

#include "tmp/c.h"
#include "tmp/postgres.h"

RcsId("$Header$");

#include "utils/palloc.h"


OidSeq
oidseqin(o)
	char *o;
{
	OidSeq os;
	char *p;

	os = (OidSeq) palloc(sizeof(OidSeqData));

	for (p = o; *p != '\0' && *p != '/'; p++)
		continue;
    
	os->os_oid = atoi(o);
	if (*p == '\0') {
		os->os_seq = 0;
	} else {
		os->os_seq = atoi(++p);
	}

	return (os);
}

char *
oidseqout(o)
	OidSeq o;
{
	char *r;

	r = (char *) palloc(20);
	sprintf(r, "%d/%ud", o->os_oid, o->os_seq);

	return (r);
}

bool
oidseqlt(o1, o2)
	OidSeq o1, o2;
{
	return ((bool) (o1->os_oid < o2->os_oid ||
		       (o1->os_oid == o2->os_oid && o1->os_seq < o2->os_seq)));
}

bool
oidseqle(o1, o2)
	OidSeq o1, o2;
{
	return ((bool) (o1->os_oid < o2->os_oid ||
		       (o1->os_oid == o2->os_oid && o1->os_seq <= o2->os_seq)));
}

bool
oidseqeq(o1, o2)
	OidSeq o1, o2;
{
	return ((bool) (o1->os_oid == o2->os_oid && o1->os_seq == o2->os_seq));
}

bool
oidseqge(o1, o2)
	OidSeq o1, o2;
{
	return ((bool) (o1->os_oid > o2->os_oid ||
		       (o1->os_oid == o2->os_oid && o1->os_seq >= o2->os_seq)));
}

bool
oidseqgt(o1, o2)
	OidSeq o1, o2;
{
	return ((bool) (o1->os_oid > o2->os_oid ||
		       (o1->os_oid == o2->os_oid && o1->os_seq > o2->os_seq)));
}

bool
oidseqne(o1, o2)
	OidSeq o1, o2;
{
	return ((bool) (o1->os_oid != o2->os_oid || o1->os_seq != o2->os_seq));
}

bool
oidseqcmp(o1, o2)
	OidSeq o1, o2;
{
	if (oidseqlt(o1, o2))
		return (-1);
	else if (oidseqeq(o1, o2))
		return (0);
	else
		return (1);
}

OidSeq
mkoidseq(v_oid, v_seq)
	ObjectId v_oid;
	uint32 v_seq;
{
	OidSeq o;

	o = (OidSeq) palloc(sizeof(OidSeqData));
	o->os_oid = v_oid;
	o->os_seq = v_seq;
	return (o);
}
