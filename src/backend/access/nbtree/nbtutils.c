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
#include "access/iqual.h"
#include "access/ftup.h"
#include "access/nbtree.h"

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
    Pointer arg;
    RegProcedure proc;
    Boolean null;

    natts = rel->rd_rel->relnatts;
    itupdesc = RelationGetTupleDescriptor(rel);

    skey = (ScanKey) palloc(natts * sizeof(ScanKeyEntryData));

    for (i = 0; i < natts; i++) {
	arg = (Pointer) index_getattr(itup, i + 1, itupdesc, &null);
	proc = index_getprocid(rel, i + 1, BTORDER_PROC);
	ScanKeyEntryInitialize(&(skey->data[i]), 0x0, i + 1, proc, arg);
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

/*
 *  _bt_orderkeys() -- Put keys in a sensible order for conjunctive quals.
 *
 *	The order of the keys in the qual match the ordering imposed by
 *	the index.  This routine only needs to be called if there are
 *	more than one qual clauses using this index.
 */

void
_bt_orderkeys(relation, numberOfKeys, key)
    Relation relation;
    uint16 *numberOfKeys;
    ScanKey key;
{
    ScanKey xform;
    ScanKeyEntryData *cur;
    StrategyMap map;
    int nbytes;
    int test;
    int i, j;
    int init[BTMaxStrategyNumber];

    /* haven't looked at any strategies yet */
    for (i = 0; i <= BTMaxStrategyNumber; i++)
	init[i] = 0;

    /* get space for the modified array of keys */
    nbytes = BTMaxStrategyNumber * sizeof (ScanKeyEntryData);
    xform = (ScanKey) palloc(nbytes);
    bzero(xform, nbytes);

    /* get the strategy map for this index/attribute pair */
    /*
     *  XXX
     *  When we support multiple keys in a single index, this is what
     *  we'll want to do.  At present, the planner is hosed, so we
     *  hard-wire the attribute number below.  Postgres only does single-
     *  key indices...
     * map = IndexStrategyGetStrategyMap(RelationGetIndexStrategy(relation),
     * 				    BTMaxStrategyNumber,
     * 				    key->data[0].attributeNumber);
     */
     map = IndexStrategyGetStrategyMap(RelationGetIndexStrategy(relation),
					BTMaxStrategyNumber,
					1 /* XXX */ );

    /* check each key passed in */
    for (i = *numberOfKeys; --i >= 0; ) {
	cur = &(key->data[i]);
	for (j = BTMaxStrategyNumber; --j >= 0; ) {
	    if (cur->procedure == map->entry[j].procedure)
		    break;
	}

	/* have we seen one of these before? */
	if (init[j]) {
	    /* yup, use the appropriate value */
	    test = (int) (*(cur->func))(cur->argument,
				        xform->data[j].argument);
	    if (test)
		xform->data[j].argument = cur->argument;
	} else {
	    /* nope, use this value */
	    bcopy(cur, &xform->data[j], sizeof (*cur));
	    init[j] = 1;
	}
    }

    /* if = has been specified, no other key will be used */
    if (init[BTEqualStrategyNumber - 1]) {
	init[BTLessStrategyNumber - 1] = 0;
	init[BTLessEqualStrategyNumber - 1] = 0;
	init[BTGreaterEqualStrategyNumber - 1] = 0;
	init[BTGreaterStrategyNumber - 1] = 0;
    }

    /* only one of <, <= */
    if (init[BTLessStrategyNumber - 1]
	&& init[BTLessEqualStrategyNumber - 1]) {

	ScanKeyEntryData *lt, *le;

	lt = &xform->data[BTLessStrategyNumber - 1];
	le = &xform->data[BTLessEqualStrategyNumber - 1];

	/*
	 *  DO NOT use the cached function stuff here -- this is key
	 *  ordering, happens only when the user expresses a hokey
	 *  qualification, and gets executed only once, anyway.  The
	 *  transform maps are hard-coded, and can't be initialized
	 *  in the correct way.
	 */

	test = (int) fmgr(le->procedure, le->argument, lt->argument);

	if (test)
	    init[BTLessEqualStrategyNumber - 1] = 0;
	else
	    init[BTLessStrategyNumber - 1] = 0;
    }

    /* only one of >, >= */
    if (init[BTGreaterStrategyNumber - 1]
	&& init[BTGreaterEqualStrategyNumber - 1]) {

	ScanKeyEntryData *gt, *ge;

	gt = &xform->data[BTGreaterStrategyNumber - 1];
	ge = &xform->data[BTGreaterEqualStrategyNumber - 1];

	/* see note above on function cache */
	test = (int) fmgr(ge->procedure, gt->argument, gt->argument);

	if (test)
	    init[BTGreaterStrategyNumber - 1] = 0;
	else
	    init[BTGreaterEqualStrategyNumber - 1] = 0;
    }

    /* okay, reorder and count */
    j = 0;

    for (i = BTMaxStrategyNumber; --i >= 0; )
	if (init[i])
	    key->data[j++] = xform->data[i];

    *numberOfKeys = j;

    pfree (xform);
}

#include <stdio.h>

_bt_dump(rel)
    Relation rel;
{
    Buffer buf;
    Page page;
    BTPageOpaque opaque;
    ItemId itemid;
    BTItem item;
    OffsetIndex offind, maxoff, start;
    BlockNumber nxtbuf;
    TupleDescriptor itupdesc;
    IndexTuple itup;
    Boolean isnull;
    Datum keyvalue;
    uint16 flags;
    BlockNumber i, nblocks;

    printf("----------------------- tree dump --------------------------\n");
    nblocks = RelationGetNumberOfBlocks(rel);
    itupdesc = RelationGetTupleDescriptor(rel);

    for (i = 1; i < nblocks; i++) {
	buf = _bt_getbuf(rel, i, BT_READ);
	page = BufferGetPage(buf, 0);
	opaque = (BTPageOpaque) PageGetSpecialPointer(page);

	printf("page %d prev %d next %d", BufferGetBlockNumber(buf),
		opaque->btpo_prev, opaque->btpo_next);

	flags = opaque->btpo_flags;

	if (flags & BTP_ROOT)
	    printf (" <root>");
	if (flags & BTP_LEAF)
	    printf (" <leaf>");
	if (flags & BTP_FREE)
	    printf (" <free>");

	printf("\n");

	if (opaque->btpo_next != P_NONE) {
	    printf("    high key:");
	    _bt_dumptup(rel, itupdesc, page, 0);
	    start = 1;
	} else {
	    printf(" no high key\n");
	    start = 0;
	}

	maxoff = PageGetMaxOffsetIndex(page);
	if (!PageIsEmpty(page) &&
	     (opaque->btpo_next == P_NONE || maxoff != start)) {
	    for (offind = start; offind <= maxoff; offind++) {
		printf("\t{%d} ", offind + 1);
		_bt_dumptup(rel, itupdesc, page, offind);
	    }
	}
	_bt_relbuf(rel, buf, BT_READ);
    }
}

_bt_dumppg(rel, page)
    Relation rel;
    Page page;
{
    BTPageOpaque opaque;
    ItemId itemid;
    BTItem item;
    OffsetIndex offind, maxoff, start;
    BlockNumber nxtbuf;
    TupleDescriptor itupdesc;
    IndexTuple itup;
    Boolean isnull;
    Datum keyvalue;
    uint16 flags;

    itupdesc = RelationGetTupleDescriptor(rel);

    opaque = (BTPageOpaque) PageGetSpecialPointer(page);

    printf("prev %d next %d", opaque->btpo_prev, opaque->btpo_next);

    flags = opaque->btpo_flags;

    if (flags & BTP_ROOT)
	printf (" <root>");
    if (flags & BTP_LEAF)
	printf (" <leaf>");
    if (flags & BTP_FREE)
	printf (" <free>");

    printf("\n");

    if (opaque->btpo_next != P_NONE) {
	printf("    high key:");
	_bt_dumptup(rel, itupdesc, page, 0);
	start = 1;
    } else {
	printf(" no high key\n");
	start = 0;
    }

    maxoff = PageGetMaxOffsetIndex(page);
    if (!PageIsEmpty(page) &&
	 (opaque->btpo_next == P_NONE || maxoff != start)) {
	for (offind = start; offind <= maxoff; offind++) {
	    printf("\t{%d} ", offind + 1);
	    _bt_dumptup(rel, itupdesc, page, offind);
	}
    }
}

#include "tmp/datum.h"

_bt_dumptup(rel, itupdesc, page, offind)
    Relation rel;
    TupleDescriptor itupdesc;
    Page page;
    OffsetIndex offind;
{
    ItemId itemid;
    Size itemsz;
    BTItem btitem;
    IndexTuple itup;
    Size tuplen;
    ItemPointer iptr;
    BlockNumber blkno;
    PageNumber pgno;
    OffsetNumber offno;
    Datum idatum;
    int16 tmpkey;
    Boolean null;

    itemid = PageGetItemId(page, offind);
    itemsz = ItemIdGetLength(itemid);
    btitem = (BTItem) PageGetItem(page, itemid);
    itup = &(btitem->bti_itup);
    tuplen = IndexTupleSize(itup);
    iptr = &(itup->t_tid);
    blkno = (iptr->blockData.data[0] << 16) + (uint16) iptr->blockData.data[1];
    pgno = 0;
    offno = (OffsetNumber) (pointer->positionData & 0xffff);

    idatum = IndexTupleGetAttributeValue(itup, 1, itupdesc, &null);
    tmpkey = DatumGetInt32(idatum);

    printf("[%d/%d bytes] <%d,%d,%d> %ld (oid %ld)\n", itemsz, tuplen,
	    blkno, pgno, offno, tmpkey, btitem->bti_oid);
}

bool
_bt_checkqual(scan, itup)
    IndexScanDesc scan;
    IndexTuple itup;
{
    if (scan->numberOfKeys > 0)
	return (ikeytest(itup, scan->relation,
			 scan->numberOfKeys, &(scan->keyData)));
    else
	return (true);
}

BTItem
_bt_formitem(itup)
    IndexTuple itup;
{
    int nbytes_btitem;
    BTItem btitem;
    Size tuplen;
    extern OID newoid();

    /* make a copy of the index tuple with room for the sequence number */
    tuplen = IndexTupleSize(itup);
    nbytes_btitem = tuplen +
			(sizeof(BTItemData) - sizeof(IndexTupleData));

    btitem = (BTItem) palloc(nbytes_btitem);
    bcopy((char *) itup, (char *) &(btitem->bti_itup), tuplen);

    btitem->bti_oid = newoid();
    return (btitem);
}
