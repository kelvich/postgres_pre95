/*
 *  btutils.c -- Utility code for Postgres btree implementation.
 */

#include "tmp/c.h"
#include "tmp/postgres.h"

#include "storage/bufmgr.h"
#include "storage/bufpage.h"
#include "storage/page.h"

#include "utils/log.h"
#include "utils/rel.h"
#include "utils/excid.h"

#include "access/heapam.h"
#include "access/genam.h"
#include "access/ftup.h"

#include "/n/eden/users/mao/postgres/src/access/nbtree/nbtree.h"

RcsId("$Header$");

ScanKey
_bt_mkscankey(rel, itup)
    Relation rel;
    IndexTuple itup;
{
    ScanKey skey;
    TupleDescriptor itupdesc;
    int natts;
    int i;
    Boolean null;

    natts = rel->rd_rel->relnatts;
    itupdesc = RelationGetTupleDescriptor(rel);

    skey = (ScanKey) palloc(natts * sizeof(ScanKeyEntryData));

    for (i = 0; i < natts; i++) {
	skey->data[i].flags = 0x0;
	skey->data[i].attributeNumber = i + 1;
	skey->data[i].argument = index_getattr(itup, i + 1, itupdesc, &null);
	skey->data[i].procedure = index_getprocid(rel, i + 1, BTORDER_PROC);
    }

    return (skey);
}

void
_bt_freeskey(skey)
    ScanKey skey;
{
    pfree (skey);
}

void
_bt_freestack(stack)
    BTStack stack;
{
    BTStack ostack;

    while (stack != (BTStack) NULL) {
	ostack = stack;
	stack = stack->bts_parent;
	pfree(ostack->bts_btitem);
	pfree(ostack);
    }
}
