/* #include "tmp/postgres.h" */

/* RcsId("$Header$"); */


#include <stdio.h>

#include "tmp/postgres.h"
#include "tmp/globals.h"
#include "tmp/align.h"
#include "catalog/syscache.h"
#include "catalog/pg_type.h"

#include "access/heapam.h"
#include "access/htup.h"
#include "access/itup.h"
#include "access/relscan.h"
#include "utils/rel.h"
#include "utils/log.h"
#include "utils/fmgr.h"
#include "tmp/daemon.h"
#include "utils/dynamic_loader.h"

/*
btinsert(rel, itup)
btgettuple(scan, dir)
btbeginscan(rel, fromEnd, keysz, scankey)
btrescan(scan, fromEnd, scankey)
btendscan(scan)
btmarkpos(scan)
btrestrpos(scan)
 */

#define N 200000

int
lookup()

{
	Relation relation, irel;
	HeapTuple htup;
	RetrieveIndexResult itup;
    IndexScanDesc scan;
	int i;
	int part;
	ScanKeyData skey;
	RetrieveIndexResult res;
	int x, y;
	Attribute *att;
	char *type, isnull;

	relation = amopenr("part");
	att = &relation->rd_att;
	irel = amopenr("p");
	ScanKeyEntryInitialize(&skey, 0, 1, F_INT4EQ, 0);
	for (i = 0; i < 1000; i++)
	{
		part = rand(N);
		skey.data[0].argument = part;
	    scan = (IndexScanDesc) btbeginscan(irel, false, 1, &skey);
		itup = (RetrieveIndexResult) btgettuple(scan, 1);
		htup = (HeapTuple) heap_fetch(relation, NowTimeQual,
			   GeneralRetrieveIndexResultGetHeapItemPointer(itup), 0);
		x = (int) fastgetattr(htup, 3, att, &isnull);
		y = (int) fastgetattr(htup, 4, att, &isnull);
		type = (char *) fastgetattr(htup, 2, att, &isnull);
		nullproc(x, y, type);
		pfree(itup);
		btendscan(scan);
	}
}

static int nparts = 0;
static int partdb[1000];

int
launch_traverse(dir)

int dir;

{
    Relation part_rel, part_index, conn_rel, conn_index;
    ScanKeyData p_skey;
    ScanKeyData c_skey;
	Attribute *p_att, *c_att;
	int part;

	part_rel = amopenr("part");
	part_index = amopenr("p");
	conn_rel = amopenr("connect");
	nparts=0;

	if (dir == 1)
	{
	    conn_index = amopenr("c1");
	}
	else
	{
		conn_index = amopenr("c2");
	}

	ScanKeyEntryInitialize(&p_skey, 0, 1, F_INT4EQ, 0);
	ScanKeyEntryInitialize(&c_skey, 0, 1, F_INT4EQ, 0);

	p_att = &part_rel->rd_att;
	c_att = &conn_rel->rd_att;

	part = rand(N);

    do_traverse(0, part, &p_skey, &c_skey, p_att, c_att, part_rel,
			    part_index, conn_rel, conn_index, dir);

    call_nullproc(&p_skey, p_att, part_rel, part_index);
	return(part);
}

do_traverse(depth, part, p_skey, c_skey, p_att, c_att, part_rel,
			part_index, conn_rel, conn_index, dir)

int depth, part;
ScanKeyData *p_skey;
ScanKeyData *c_skey;
Attribute *p_att, *c_att;
Relation part_rel, part_index, conn_rel, conn_index;
int dir;

{
	HeapTuple htup;
	RetrieveIndexResult itup;
    IndexScanDesc scan;
	int i;
	int x, y;
	char *type, isnull;
	int parts[3];

	/*
	 * get the part id's connected to this part.
	 */

	c_skey->data[0].argument = part;

    scan = (IndexScanDesc) btbeginscan(conn_index, false, 1, c_skey);
	for (i = 0; i < 3; i++)
	{
	    itup = (RetrieveIndexResult) btgettuple(scan, 1);
	    htup = (HeapTuple) heap_fetch(conn_rel, NowTimeQual,
		       GeneralRetrieveIndexResultGetHeapItemPointer(itup), 0);
		parts[i] = fastgetattr(htup, 2, c_att, &isnull);
	    pfree(itup);
	}
	btendscan(scan);

	for (i = 0; i < 3; i++) partdb[nparts + i] = parts[i];
	nparts += 3;

	/*
	 * if not 7 levels down (we start at 0), execute the recursive calls
	 * to get the child parts.
	 */

	if (depth != 6)
	{
		for (i = 0; i < 3; i++)
		{
            do_traverse(depth + 1, parts[i], p_skey, c_skey, p_att, c_att,
						part_rel, part_index, conn_rel, conn_index);
		}
	}

	return;
}

call_nullproc(p_skey, p_att, part_rel, part_index)

ScanKeyData *p_skey;
Attribute *p_att;
Relation part_rel, part_index;

{
	HeapTuple htup;
	RetrieveIndexResult itup;
    IndexScanDesc scan;
	int i;
	int x, y;
	char *type, isnull;

	for (i = 0; i < nparts; i++)
	{
		p_skey->data[0].argument = partdb[i];
	    scan = (IndexScanDesc) btbeginscan(part_index, false, 1, p_skey);
		itup = (RetrieveIndexResult) btgettuple(scan, 1);
		htup = (HeapTuple) heap_fetch(part_rel, NowTimeQual,
			   GeneralRetrieveIndexResultGetHeapItemPointer(itup), 0);
		x = (int) fastgetattr(htup, 3, p_att, &isnull);
		y = (int) fastgetattr(htup, 4, p_att, &isnull);
		type = (char *) fastgetattr(htup, 2, p_att, &isnull);
		nullproc(x, y, type);
		pfree(itup);
		btendscan(scan);
	}
}


/*
 * returns 1 to mod
 */

int
rand(mod)

int mod;

{
    return(random() % mod + 1);
}

nullproc(x, y, type)

int x, y;
char *type;

{}

int32 
userfntest(i)

{
	switch(i)
	{
		case 1: lookup(); break;
		case 2: launch_traverse(1); break;
		default: return;
	}
}
