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

    printf("--------------------------------------------------\n");
    itupdesc = RelationGetTupleDescriptor(rel);

    buf = _bt_getroot(rel, BT_READ);
    page = BufferGetPage(buf, 0);
    opaque = (BTPageOpaque) PageGetSpecialPointer(page);

    while (!(opaque->btpo_flags & BTP_LEAF)) {
	if (opaque->btpo_next == P_NONE)
	    itemid = PageGetItemId(page, 0);
	else
	    itemid = PageGetItemId(page, 1);

	item = (BTItem) PageGetItem(page, itemid);
	nxtbuf = ItemPointerGetBlockNumber(&(item->bti_itup.t_tid));
	_bt_relbuf(rel, buf, BT_READ);
	buf = _bt_getbuf(rel, nxtbuf, BT_READ);
	page = BufferGetPage(buf, 0);
	opaque = (BTPageOpaque) PageGetSpecialPointer(page);
    }

    for (;;) {

	printf("page %d prev %d next %d\n", BufferGetBlockNumber(buf),
		opaque->btpo_prev, opaque->btpo_next);

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
		printf("\t");
		_bt_dumptup(rel, itupdesc, page, offind);
	    }
	}

	nxtbuf = opaque->btpo_next;
	_bt_relbuf(rel, buf, BT_READ);

	if (nxtbuf == P_NONE)
	    break;

	buf = _bt_getbuf(rel, nxtbuf, BT_READ);
	page = BufferGetPage(buf, 0);
	opaque = (BTPageOpaque) PageGetSpecialPointer(page);
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
    tuplen = itup->t_size;
    iptr = &(itup->t_tid);
    blkno = ItemPointerGetBlockNumber(iptr, 0);
    pgno = ItemPointerGetPageNumber(iptr, 0);
    offno = ItemPointerGetOffsetNumber(iptr, 0);

    idatum = IndexTupleGetAttributeValue(itup, 1, itupdesc, &null);
    tmpkey = DatumGetInt16(idatum);

    printf("[%d/%d bytes] <%d,%d,%d> %d (seq %d)\n", itemsz, tuplen,
	    blkno, pgno, offno, tmpkey, btitem->bti_seqno);
}
