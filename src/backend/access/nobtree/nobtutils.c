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
#include "access/nobtree.h"

RcsId("$Header$");

ScanKey
_nobt_mkscankey(rel, itup)
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
	proc = index_getprocid(rel, i + 1, NOBTORDER_PROC);
	ScanKeyEntryInitialize(&(skey->data[i]), 0x0, i + 1, proc, arg);
    }

    return (skey);
}

void
_nobt_freeskey(skey)
    ScanKey skey;
{
    pfree (skey);
}

void
_nobt_freestack(stack)
    NOBTStack stack;
{
    NOBTStack ostack;

    while (stack != (NOBTStack) NULL) {
	ostack = stack;
	stack = stack->nobts_parent;
	pfree(ostack->nobts_btitem);
	if (ostack->nobts_nxtitem != (NOBTItem) NULL)
	    pfree(ostack->nobts_nxtitem);
	pfree(ostack);
    }
}

void
_nobt_orderkeys(relation, numberOfKeys, key)
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
    int init[NOBTMaxStrategyNumber];

    /* haven't looked at any strategies yet */
    for (i = 0; i <= NOBTMaxStrategyNumber; i++)
	init[i] = 0;

    /* get space for the modified array of keys */
    nbytes = NOBTMaxStrategyNumber * sizeof (ScanKeyEntryData);
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
     * 				    NOBTMaxStrategyNumber,
     * 				    key->data[0].attributeNumber);
     */
     map = IndexStrategyGetStrategyMap(RelationGetIndexStrategy(relation),
					NOBTMaxStrategyNumber,
					1 /* XXX */ );

    /* check each key passed in */
    for (i = *numberOfKeys; --i >= 0; ) {
	cur = &(key->data[i]);
	for (j = NOBTMaxStrategyNumber; --j >= 0; ) {
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
    if (init[NOBTEqualStrategyNumber - 1]) {
	init[NOBTLessStrategyNumber - 1] = 0;
	init[NOBTLessEqualStrategyNumber - 1] = 0;
	init[NOBTGreaterEqualStrategyNumber - 1] = 0;
	init[NOBTGreaterStrategyNumber - 1] = 0;
    }

    /* only one of <, <= */
    if (init[NOBTLessStrategyNumber - 1]
	&& init[NOBTLessEqualStrategyNumber - 1]) {

	ScanKeyEntryData *lt, *le;

	lt = &xform->data[NOBTLessStrategyNumber - 1];
	le = &xform->data[NOBTLessEqualStrategyNumber - 1];

	/*
	 *  DO NOT use the cached function stuff here -- this is key
	 *  ordering, happens only when the user expresses a hokey
	 *  qualification, and gets executed only once, anyway.  The
	 *  transform maps are hard-coded, and can't be initialized
	 *  in the correct way.
	 */

	test = (int) fmgr(le->procedure, le->argument, lt->argument);

	if (test)
	    init[NOBTLessEqualStrategyNumber - 1] = 0;
	else
	    init[NOBTLessStrategyNumber - 1] = 0;
    }

    /* only one of >, >= */
    if (init[NOBTGreaterStrategyNumber - 1]
	&& init[NOBTGreaterEqualStrategyNumber - 1]) {

	ScanKeyEntryData *gt, *ge;

	gt = &xform->data[NOBTGreaterStrategyNumber - 1];
	ge = &xform->data[NOBTGreaterEqualStrategyNumber - 1];

	/* see note above on function cache */
	test = (int) fmgr(ge->procedure, gt->argument, gt->argument);

	if (test)
	    init[NOBTGreaterStrategyNumber - 1] = 0;
	else
	    init[NOBTGreaterEqualStrategyNumber - 1] = 0;
    }

    /* okay, reorder and count */
    j = 0;

    for (i = NOBTMaxStrategyNumber; --i >= 0; )
	if (init[i])
	    key->data[j++] = xform->data[i];

    *numberOfKeys = j;

    pfree (xform);
}

#include <stdio.h>

_nobt_dump(rel)
    Relation rel;
{
    Buffer buf;
    Page page;
    NOBTPageOpaque opaque;
    ItemId itemid;
    NOBTItem item;
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
	buf = _nobt_getbuf(rel, i, NOBT_READ);
	page = BufferGetPage(buf, 0);
	opaque = (NOBTPageOpaque) PageGetSpecialPointer(page);

	printf("page %d prev %d next %d", BufferGetBlockNumber(buf),
		opaque->nobtpo_prev, opaque->nobtpo_next);

	flags = opaque->nobtpo_flags;

	if (flags & NOBTP_ROOT)
	    printf (" <root>");
	if (flags & NOBTP_LEAF)
	    printf (" <leaf>");
	if (flags & NOBTP_FREE)
	    printf (" <free>");

	printf("\n");

	if (opaque->nobtpo_next != P_NONE) {
	    printf("    high key:");
	    _nobt_dumptup(rel, itupdesc, page, 0);
	    start = 1;
	} else {
	    printf(" no high key\n");
	    start = 0;
	}

	maxoff = PageGetMaxOffsetIndex(page);
	if (!PageIsEmpty(page) &&
	     (opaque->nobtpo_next == P_NONE || maxoff != start)) {
	    for (offind = start; offind <= maxoff; offind++) {
		printf("\t{%d} ", offind + 1);
		_nobt_dumptup(rel, itupdesc, page, offind);
	    }
	}
	_nobt_relbuf(rel, buf, NOBT_READ);
    }
}

#include "tmp/datum.h"

_nobt_dumptup(rel, itupdesc, page, offind)
    Relation rel;
    TupleDescriptor itupdesc;
    Page page;
    OffsetIndex offind;
{
    ItemId itemid;
    Size itemsz;
    NOBTItem btitem;
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
    btitem = (NOBTItem) PageGetItem(page, itemid);
    itup = &(btitem->nobti_itup);
    tuplen = IndexTupleSize(itup);
    iptr = &(itup->t_tid);
    blkno = ItemPointerGetBlockNumber(iptr);
    pgno = ItemPointerGetPageNumber(iptr, 0);
    offno = ItemPointerGetOffsetNumber(iptr, 0);

    idatum = IndexTupleGetAttributeValue(itup, 1, itupdesc, &null);
    tmpkey = DatumGetInt16(idatum);

    printf("[%d/%d bytes] <%d,%d,%d> %d\n", itemsz, tuplen,
	    blkno, pgno, offno, tmpkey);
}

bool
_nobt_checkqual(scan, itup)
    IndexScanDesc scan;
    IndexTuple itup;
{
    if (scan->numberOfKeys > 0)
	return (ikeytest(itup, scan->relation,
			 scan->numberOfKeys, &(scan->keyData)));
    else
	return (true);
}

NOBTItem
_nobt_formitem(itup)
    IndexTuple itup;
{
    int nbytes_btitem;
    NOBTItem btitem;
    Size tuplen = IndexTupleSize(itup);

    /* make a copy of the index tuple with room for the sequence number */
    nbytes_btitem = tuplen +
			(sizeof(NOBTItemData) - sizeof(IndexTupleData));

    btitem = (NOBTItem) palloc(nbytes_btitem);
    bcopy((char *) itup, (char *) &(btitem->nobti_itup), tuplen);
    btitem->nobti_oldchild = P_NONE;

    return (btitem);
}
