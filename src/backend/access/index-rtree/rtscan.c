/*
 *  rtscan.c -- routines to manage scans on index relations
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
#include "access/ftup.h"
#include "access/rtree.h"

/* routines defined and used here */
extern void	rtregscan();
extern void	rtdropscan();
extern void	rtadjone();
extern void	adjuststack();
extern void	adjustiptr();

/*
 *  Whenever we start an rtree scan in a backend, we register it in private
 *  space.  Then if the rtree index gets updated, we check all registered
 *  scans and adjust them if the tuple they point at got moved by the
 *  update.  We only need to do this in private space, because when we update
 *  an rtree we have a write lock on the tree, so no other process can have
 *  any locks at all on it.  A single transaction can have write and read
 *  locks on the same object, so that's why we need to handle this case.
 */

typedef struct RTScanListData {
    IndexScanDesc		rtsl_scan;
    struct RTScanListData	*rtsl_next;
} RTScanListData;

typedef RTScanListData	*RTScanList;

/* pointer to list of local scans on rtrees */
static RTScanList RTScans = (RTScanList) NULL;

RcsId("$Header$");

IndexScanDesc
rtbeginscan(r, fromEnd, nkeys, key)
    Relation r;
    Boolean fromEnd;
    uint16 nkeys;
    ScanKey key;
{
    IndexScanDesc s;

    RelationSetLockForRead(r);
    s = RelationGetIndexScan(r, fromEnd, nkeys, key);
    rtregscan(s);

    return (s);
}

void
rtrescan(s, fromEnd, key)
    IndexScanDesc s;
    Boolean fromEnd;
    ScanKey key;
{
    RTreeScanOpaque p;
    int nbytes;

    if (!IndexScanIsValid(s)) {
	elog(WARN, "rtrescan: invalid scan.");
	return;
    }

    /*
     *  Clear all the pointers.
     */

    ItemPointerSetInvalid(&s->previousItemData);
    ItemPointerSetInvalid(&s->currentItemData);
    ItemPointerSetInvalid(&s->nextItemData);
    ItemPointerSetInvalid(&s->previousMarkData);
    ItemPointerSetInvalid(&s->currentMarkData);
    ItemPointerSetInvalid(&s->nextMarkData);

    /*
     *  Set flags.
     */
    if (RelationGetNumberOfBlocks(s->relation) == 0) {
	s->flags = ScanUnmarked;
    } else if (fromEnd) {
	s->flags = ScanUnmarked | ScanUncheckedPrevious;
    } else {
	s->flags = ScanUnmarked | ScanUncheckedNext;
    }

    s->scanFromEnd = fromEnd;

    if (s->numberOfKeys > 0) {
	bcopy( (Pointer)&key->data[0],			/* from */
	       (Pointer)&s->keyData.data[0],		/* to */
	       s->numberOfKeys * sizeof (key->data[0])	/* nbytes */
	);
    }

    p = (RTreeScanOpaque) s->opaque;
    if (p != (RTreeScanOpaque) NULL) {
	freestack(p->s_stack);
	freestack(p->s_markstk);
	p->s_stack = p->s_markstk = (RTSTACK *) NULL;
    } else {
	int i;

	/* initialize opaque data */
	nbytes = sizeof(RTreeScanOpaqueData);
	if (s->numberOfKeys > 0) {
	    nbytes += (s->numberOfKeys - 1)
		      * sizeof(ScanKeyEntryData);
	}

	s->opaque = (Pointer) palloc(nbytes);
	p = (RTreeScanOpaque) s->opaque;
	p->s_stack = p->s_markstk = (RTSTACK *) NULL;
	p->s_internalNKey = s->numberOfKeys;
	if (s->numberOfKeys > 0) {
	    nbytes = s->numberOfKeys * sizeof(ScanKeyEntryData);
	    bcopy(&(s->keyData.data[0]), &(p->s_internalKey.data[0]), nbytes);

	    for (i = 0; i < s->numberOfKeys; i++) {
		p->s_internalKey.data[i].procedure =
		   RTMapOperator(s->relation,
				 s->keyData.data[i].attributeNumber,
				 s->keyData.data[i].procedure);
	    }
	}
    }
}

void
rtmarkpos(s)
    IndexScanDesc s;
{
    RTreeScanOpaque p;
    RTSTACK *o, *n, *tmp;

    s->currentMarkData = s->currentItemData;
    p = (RTreeScanOpaque) s->opaque;

    o = (RTSTACK *) NULL;
    n = p->s_stack;

    /* copy the parent stack from the current item data */
    while (n != (RTSTACK *) NULL) {
	tmp = (RTSTACK *) palloc(sizeof(RTSTACK));
	tmp->rts_child = n->rts_child;
	tmp->rts_blk = n->rts_blk;
	tmp->rts_parent = o;
	o = tmp;
	n = n->rts_parent;
    }

    freestack(p->s_markstk);
    p->s_markstk = o;
}

void
rtrestrpos(s)
    IndexScanDesc s;
{
    RTreeScanOpaque p;
    RTSTACK *o, *n, *tmp;

    s->currentItemData = s->currentMarkData;
    p = (RTreeScanOpaque) s->opaque;

    o = (RTSTACK *) NULL;
    n = p->s_markstk;

    /* copy the parent stack from the current item data */
    while (n != (RTSTACK *) NULL) {
	tmp = (RTSTACK *) palloc(sizeof(RTSTACK));
	tmp->rts_child = n->rts_child;
	tmp->rts_blk = n->rts_blk;
	tmp->rts_parent = o;
	o = tmp;
	n = n->rts_parent;
    }

    freestack(p->s_stack);
    p->s_stack = o;
}

void
rtendscan(s)
    IndexScanDesc s;
{
    RTreeScanOpaque p;

    p = (RTreeScanOpaque) s->opaque;

    if (p != (RTreeScanOpaque) NULL) {
	freestack(p->s_stack);
	freestack(p->s_markstk);
    }

    rtdropscan(s);
    /* XXX don't unset read lock -- two-phase locking */
}

void
rtregscan(s)
    IndexScanDesc s;
{
    RTScanList l;

    l = (RTScanList) palloc(sizeof(RTScanListData));
    l->rtsl_scan = s;
    l->rtsl_next = RTScans;
    RTScans = l;
}

void
rtdropscan(s)
    IndexScanDesc s;
{
    RTScanList l;
    RTScanList prev;

    prev = (RTScanList) NULL;

    for (l = RTScans;
	 l != (RTScanList) NULL && l->rtsl_scan != s;
	 l = l->rtsl_next) {
	prev = l;
    }

    if (l == (RTScanList) NULL)
	elog(WARN, "rtree scan list corrupted -- cannot find 0x%lx", s);

    if (prev == (RTScanList) NULL)
	RTScans = l->rtsl_next;
    else
	prev->rtsl_next = l->rtsl_next;

    pfree (l);
}

void
rtadjscans(r, op, blkno, offind)
    Relation r;
    int op;
    BlockNumber blkno;
    OffsetIndex offind;
{
    RTScanList l;
    ObjectId relid;

    relid = r->rd_id;
    for (l = RTScans; l != (RTScanList) NULL; l = l->rtsl_next) {
	if (l->rtsl_scan->relation->rd_id == relid)
	    rtadjone(l->rtsl_scan, op, blkno, offind);
    }
}

/*
 *  rtadjone() -- adjust one scan for update.
 *
 *	By here, the scan passed in is on a modified relation.  Op tells
 *	us what the modification is, and blkno and offind tell us what
 *	block and offset index were affected.  This routine checks the
 *	current and marked positions, and the current and marked stacks,
 *	to see if any stored location needs to be changed because of the
 *	update.  If so, we make the change here.
 */

void
rtadjone(s, op, blkno, offind)
    IndexScanDesc s;
    int op;
    BlockNumber blkno;
    OffsetIndex offind;
{
    RTreeScanOpaque so;

    adjustiptr(&(s->currentItemData), op, blkno, offind);
    adjustiptr(&(s->currentMarkData), op, blkno, offind);

    so = (RTreeScanOpaque) s->opaque;

    if (op == RTOP_SPLIT) {
	adjuststack(so->s_stack, blkno, offind);
	adjuststack(so->s_markstk, blkno, offind);
    }
}

/*
 *  adjustiptr() -- adjust current and marked item pointers in the scan
 *
 *	Depending on the type of update and the place it happened, we
 *	need to do nothing, to back up one record, or to start over on
 *	the same page.
 */

void
adjustiptr(iptr, op, blkno, offind)
    ItemPointer iptr;
    int op;
    BlockNumber blkno;
    OffsetIndex offind;
{
    if (ItemPointerIsValid(iptr)) {
	if (ItemPointerGetBlockNumber(iptr) == blkno) {
	    switch (op) {
	      case RTOP_DEL:
		/* back up one if we need to */
		if (ItemPointerGetOffsetNumber(iptr, 0) <= offind + 1)
		  ItemPointerSet(iptr, 0, blkno, 0, offind);
		break;

	      case RTOP_SPLIT:
		/* back to start of page on split */
		ItemPointerSet(iptr, 0, blkno, 0, 1);
		break;

	      default:
		elog(WARN, "Bad operation in rtree scan adjust: %d", op);
	    }
	}
    }
}

/*
 *  adjuststack() -- adjust the supplied stack for a split on a page in
 *		     the index we're scanning.
 *
 *	If a page on our parent stack has split, we need to back up to the
 *	beginning of the page and rescan it.  The reason for this is that
 *	the split algorithm for rtrees doesn't order tuples in any useful
 *	way on a single page.  This means on that a split, we may wind up
 *	looking at some heap tuples more than once.  This is handled in the
 *	access method update code for heaps; if we've modified the tuple we
 *	are looking at already in this transaction, we ignore the update
 *	request.
 */

void
adjuststack(stk, op, blkno, offind)
    RTSTACK *stk;
    BlockNumber blkno;
    OffsetIndex offind;
{
    while (stk != (RTSTACK *) NULL) {
	if (stk->rts_blk == blkno)
	    stk->rts_child = 0;

	stk = stk->rts_parent;
    }
}
