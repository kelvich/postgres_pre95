/*
 *  rtget.c -- fetch tuples from an rtree scan.
 */
#include "tmp/c.h"
#include "tmp/postgres.h"

#include "storage/bufmgr.h"
#include "storage/bufpage.h"
#include "storage/page.h"

#include "utils/log.h"
#include "utils/rel.h"

#include "access/heapam.h"
#include "access/genam.h"
#include "access/iqual.h"
#include "access/ftup.h"
#include "access/rtree.h"
#include "access/sdir.h"

RcsId("$Header$");

extern RetrieveIndexResult	rtscancache();
extern RetrieveIndexResult	rtfirst();
extern RetrieveIndexResult	rtnext();
extern ItemPointer		rtheapptr();

RetrieveIndexResult
rtgettuple(s, dir)
    IndexScanDesc s;
    ScanDirection dir;
{
    RetrieveIndexResult res;

    /* if we have it cached in the scan desc, just return the value */
    if ((res = rtscancache(s, dir)) != (RetrieveIndexResult) NULL)
	return (res);

    /* not cached, so we'll have to do some work */
    if (ItemPointerIsValid(&(s->currentItemData))) {
	res = rtnext(s, dir);
    } else {
	res = rtfirst(s, dir);
    }
    return (res);
}

RetrieveIndexResult
rtfirst(s, dir)
    IndexScanDesc s;
    ScanDirection dir;
{
    Buffer b;
    Page p;
    int n;
    OffsetNumber maxoff;
    RetrieveIndexResult res;
    RTreePageOpaque po;
    RTreeScanOpaque so;
    RTSTACK *stk;
    BlockNumber blk;
    IndexTuple it;
    ItemPointer ip;

    b = ReadBuffer(s->relation, P_ROOT);
    p = BufferGetPage(b, 0);
    po = (RTreePageOpaque) PageGetSpecialPointer(p);
    so = (RTreeScanOpaque) s->opaque;

    for (;;) {
	maxoff = PageGetMaxOffsetIndex(p);
	if (ScanDirectionIsBackward(dir))
	    n = findnext(s, p, maxoff, dir);
	else
	    n = findnext(s, p, 0, dir);

	while (n < 0 || n > maxoff) {

	    ReleaseBuffer(b);
	    if (so->s_stack == (RTSTACK *) NULL)
		return ((RetrieveIndexResult) NULL);

	    stk = so->s_stack;
	    b = ReadBuffer(s->relation, stk->rts_blk);
	    p = BufferGetPage(b, 0);
	    po = (RTreePageOpaque) PageGetSpecialPointer(p);
	    maxoff = PageGetMaxOffsetIndex(p);

	    if (ScanDirectionIsBackward(dir))
		n = stk->rts_child - 1;
	    else
		n = stk->rts_child + 1;
	    so->s_stack = stk->rts_parent;
	    free (stk);

	    n = findnext(s, p, n, dir);
	}
	if (po->flags & F_LEAF) {
	    ItemPointerSet(&(s->currentItemData), 0, BufferGetBlockNumber(b),
				0, n + 1);

	    it = (IndexTuple) PageGetItem(p, PageGetItemId(p, n));
	    ip = (ItemPointer) palloc(sizeof(ItemPointerData));
	    bcopy((char *) &(it->t_tid), (char *) ip, sizeof(ItemPointerData));
	    ReleaseBuffer(b);

	    res = ItemPointerFormRetrieveIndexResult(&(s->currentItemData), ip);

	    return (res);
	} else {
	    stk = (RTSTACK *) palloc(sizeof(RTSTACK));
	    stk->rts_child = n;
	    stk->rts_blk = BufferGetBlockNumber(b);
	    stk->rts_parent = so->s_stack;
	    so->s_stack = stk;

	    it = (IndexTuple) PageGetItem(p, PageGetItemId(p, n));
	    blk = ItemPointerGetBlockNumber(&(it->t_tid));

	    ReleaseBuffer(b);
	    b = ReadBuffer(s->relation, blk);
	    p = BufferGetPage(b, 0);
	    po = (RTreePageOpaque) PageGetSpecialPointer(p);
	}
    }
}

RetrieveIndexResult
rtnext(s, dir)
    IndexScanDesc s;
    ScanDirection dir;
{
    Buffer b;
    Page p;
    int n;
    OffsetNumber maxoff;
    RetrieveIndexResult res;
    RTreePageOpaque po;
    RTreeScanOpaque so;
    RTSTACK *stk;
    BlockNumber blk;
    IndexTuple it;
    ItemPointer ip;

    blk = ItemPointerGetBlockNumber(&(s->currentItemData));
    n = ItemPointerSimpleGetOffsetNumber(&(s->currentItemData)) - 1;

    if (ScanDirectionIsForward(dir))
	++n;
    else
	--n;

    b = ReadBuffer(s->relation, blk);
    p = BufferGetPage(b, 0);
    po = (RTreePageOpaque) PageGetSpecialPointer(p);
    so = (RTreeScanOpaque) s->opaque;

    for (;;) {
	maxoff = PageGetMaxOffsetIndex(p);
	n = findnext(s, p, n, dir);

	while (n < 0 || n > maxoff) {

	    ReleaseBuffer(b);
	    if (so->s_stack == (RTSTACK *) NULL)
		return ((RetrieveIndexResult) NULL);

	    stk = so->s_stack;
	    b = ReadBuffer(s->relation, stk->rts_blk);
	    p = BufferGetPage(b, 0);
	    maxoff = PageGetMaxOffsetIndex(p);
	    po = (RTreePageOpaque) PageGetSpecialPointer(p);

	    if (ScanDirectionIsBackward(dir))
		n = stk->rts_child - 1;
	    else
		n = stk->rts_child + 1;
	    so->s_stack = stk->rts_parent;
	    free (stk);

	    n = findnext(s, p, n, dir);
	}
	if (po->flags & F_LEAF) {
	    ItemPointerSet(&(s->currentItemData), 0, BufferGetBlockNumber(b),
				0, n + 1);

	    it = (IndexTuple) PageGetItem(p, PageGetItemId(p, n));
	    ip = (ItemPointer) palloc(sizeof(ItemPointerData));
	    bcopy((char *) &(it->t_tid), (char *) ip, sizeof(ItemPointerData));
	    ReleaseBuffer(b);

	    res = ItemPointerFormRetrieveIndexResult(&(s->currentItemData), ip);

	    return (res);
	} else {
	    stk = (RTSTACK *) palloc(sizeof(RTSTACK));
	    stk->rts_child = n;
	    stk->rts_blk = BufferGetBlockNumber(b);
	    stk->rts_parent = so->s_stack;
	    so->s_stack = stk;

	    it = (IndexTuple) PageGetItem(p, PageGetItemId(p, n));
	    blk = ItemPointerGetBlockNumber(&(it->t_tid));

	    ReleaseBuffer(b);
	    b = ReadBuffer(s->relation, blk);
	    p = BufferGetPage(b, 0);
	    po = (RTreePageOpaque) PageGetSpecialPointer(p);

	    if (ScanDirectionIsBackward(dir))
		n = PageGetMaxOffsetIndex(p);
	    else
		n = 0;
	}
    }
}

int
findnext(s, p, n, dir)
    IndexScanDesc s;
    Page p;
    int n;
    ScanDirection dir;
{
    OffsetNumber maxoff;
    IndexTuple it;
    RTreePageOpaque po;
    RTreeScanOpaque so;

    maxoff = PageGetMaxOffsetIndex(p);
    po = (RTreePageOpaque) PageGetSpecialPointer(p);
    so = (RTreeScanOpaque) s->opaque;

    /*
     *  If we modified the index during the scan, we may have a pointer to
     *  a ghost tuple, before the scan.  If this is the case, back up one.
     */

    if (so->s_flags & RTS_CURBEFORE) {
	so->s_flags &= ~RTS_CURBEFORE;
	n--;
    }

    while (n >= 0 && n <= maxoff) {
	it = (IndexTuple) PageGetItem(p, PageGetItemId(p, n));
	if (po->flags & F_LEAF) {
	    if (ikeytest(it, s->relation, s->numberOfKeys, &s->keyData))
		break;
	} else {
	    if (ikeytest(it, s->relation, so->s_internalNKey,
			 &so->s_internalKey))
		break;
	}

	if (ScanDirectionIsBackward(dir))
	    --n;
	else
	    ++n;
    }

    return (n);
}

RetrieveIndexResult
rtscancache(s, dir)
    IndexScanDesc s;
    ScanDirection dir;
{
    RetrieveIndexResult res;
    ItemPointer ip;

    if (!(ScanDirectionIsNoMovement(dir)
		  && ItemPointerIsValid(&(s->currentItemData)))) {

	return ((RetrieveIndexResult) NULL);
    } 

    ip = rtheapptr(s->relation, &(s->currentItemData));

    if (ItemPointerIsValid(ip))
	res = ItemPointerFormRetrieveIndexResult(&(s->currentItemData), ip);
    else
	res = (RetrieveIndexResult) NULL;

    return (res);
}

/*
 *  rtheapptr returns the item pointer to the tuple in the heap relation
 *  for which itemp is the index relation item pointer.
 */

ItemPointer
rtheapptr(r, itemp)
    Relation r;
    ItemPointer itemp;
{
    Buffer b;
    Page p;
    IndexTuple it;
    ItemPointer ip;
    OffsetNumber n;

    ip = (ItemPointer) palloc(sizeof(ItemPointerData));
    if (ItemPointerIsValid(itemp)) {
	b = ReadBuffer(r, ItemPointerGetBlockNumber(itemp));
	p = BufferGetPage(b, 0);
	n = ItemPointerSimpleGetOffsetNumber(itemp);
	it = (IndexTuple) PageGetItem(p, PageGetItemId(p, n - 1));
	bcopy((char *) &(it->t_tid), (char *) ip, sizeof(ItemPointerData));
	ReleaseBuffer(b);
    } else {
	ItemPointerSetInvalid(ip);
    }

    return (ip);
}
