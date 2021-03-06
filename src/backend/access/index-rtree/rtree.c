/* ----------------------------------------------------------------
 *   FILE
 *	rtree.c
 *
 *   DESCRIPTION
 *	interface routines for the postgres rtree indexed access method.
 *
 *   NOTES
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#include "tmp/c.h"
#include "tmp/postgres.h"

#include "storage/bufmgr.h"
#include "storage/bufpage.h"
#include "storage/page.h"

#include "utils/log.h"
#include "utils/palloc.h"
#include "utils/rel.h"
#include "utils/excid.h"

#include "access/heapam.h"
#include "access/genam.h"
#include "access/ftup.h"
#include "access/rtree.h"
#include "access/funcindex.h"

#include "nodes/execnodes.h"
#include "nodes/plannodes.h"

#include "executor/x_qual.h"
#include "executor/x_tuples.h"
#include "executor/tuptable.h"

#include "lib/index.h"

extern ExprContext RMakeExprContext();

RcsId("$Header$");

typedef struct SPLITVEC {
    OffsetNumber	*spl_left;
    int			spl_nleft;
    char		*spl_ldatum;
    OffsetNumber	*spl_right;
    int			spl_nright;
    char		*spl_rdatum;
} SPLITVEC;

/* automatically generated using mkproto */
extern void rtbuild ARGS((Relation heap, Relation index, AttributeNumber natts, AttributeNumber *attnum, IndexStrategy istrat, uint16 pcount, Datum *params, FuncIndexInfo *finfo, LispValue predInfo));
extern InsertIndexResult rtinsert ARGS((Relation r, IndexTuple itup));
extern InsertIndexResult rtdoinsert ARGS((Relation r, IndexTuple itup));
extern int rttighten ARGS((Relation r, RTSTACK *stk, char *datum, int att_size));
extern InsertIndexResult dosplit ARGS((Relation r, Buffer buffer, RTSTACK *stack, IndexTuple itup));
extern int rtintinsert ARGS((Relation r, RTSTACK *stk, IndexTuple ltup, IndexTuple rtup));
extern int rtnewroot ARGS((Relation r, IndexTuple lt, IndexTuple rt));
extern int picksplit ARGS((Relation r, Page page, SPLITVEC *v, IndexTuple itup));
extern int RTInitBuffer ARGS((Buffer b, uint32 f));
extern int choose ARGS((Relation r, Page p, IndexTuple it));
extern int nospace ARGS((Page p, IndexTuple it));
extern int freestack ARGS((RTSTACK *s));
extern char *rtdelete ARGS((Relation r, ItemPointer tid));
extern int _rtdump ARGS((Relation r));

void
rtbuild(heap, index, natts, attnum, istrat, pcount, params, finfo, predInfo)
    Relation heap;
    Relation index;
    AttributeNumber natts;
    AttributeNumber *attnum;
    IndexStrategy istrat;
    uint16 pcount;
    Datum *params;
    FuncIndexInfo *finfo;
    LispValue predInfo;
{
    HeapScanDesc scan;
    Buffer buffer;
    AttributeNumber i;
    HeapTuple htup;
    IndexTuple itup;
    TupleDescriptor hd, id;
    InsertIndexResult res;
    Datum *d;
    Boolean *nulls;
    int nb, nh, ni;
    ExprContext econtext;
    TupleTable tupleTable;
    TupleTableSlot slot;
    ObjectId hrelid, irelid;
    LispValue pred, oldPred;

    /* rtrees only know how to do stupid locking now */
    RelationSetLockForWrite(index);

    pred = CAR(predInfo);
    oldPred = CADR(predInfo);

    /*
     *  We expect to be called exactly once for any index relation.
     *  If that's not the case, big trouble's what we have.
     */

    if (oldPred == LispNil && (nb = RelationGetNumberOfBlocks(index)) != 0)
	elog(WARN, "%.16s already contains data", &(index->rd_rel->relname.data[0]));

    /* initialize the root page (if this is a new index) */
    if (oldPred == LispNil) {
	buffer = ReadBuffer(index, P_NEW);
	RTInitBuffer(buffer, F_LEAF);
	WriteBuffer(buffer);
    }

    /* init the tuple descriptors and get set for a heap scan */
    hd = RelationGetTupleDescriptor(heap);
    id = RelationGetTupleDescriptor(index);
    d = LintCast(Datum *, palloc(natts * sizeof (*d)));
    nulls = LintCast(Boolean *, palloc(natts * sizeof (*nulls)));

    /*
     * If this is a predicate (partial) index, we will need to evaluate the
     * predicate using ExecQual, which requires the current tuple to be in a
     * slot of a TupleTable.  In addition, ExecQual must have an ExprContext
     * referring to that slot.  Here, we initialize dummy TupleTable and
     * ExprContext objects for this purpose. --Nels, Feb '92
     */
    if (pred != LispNil || oldPred != LispNil) {
	tupleTable = ExecCreateTupleTable(1);
	slot = (TupleTableSlot)
	    ExecGetTableSlot(tupleTable, ExecAllocTableSlot(tupleTable));
	econtext = RMakeExprContext();
	FillDummyExprContext(econtext, slot, hd, buffer);
    }

    scan = heap_beginscan(heap, 0, NowTimeQual, 0, (ScanKey) NULL);
    htup = heap_getnext(scan, 0, &buffer);

    /* count the tuples as we insert them */
    nh = ni = 0;

    for (; HeapTupleIsValid(htup); htup = heap_getnext(scan, 0, &buffer)) {

	nh++;

	/*
	 * If oldPred != LispNil, this is an EXTEND INDEX command, so skip
	 * this tuple if it was already in the existing partial index
	 */
	if (oldPred != LispNil) {
	    SetSlotContents(slot, htup);
	    if (ExecQual(oldPred, econtext) == true) {
		ni++;
		continue;
	    }
	}

	/* Skip this tuple if it doesn't satisfy the partial-index predicate */
	if (pred != LispNil) {
	    SetSlotContents(slot, htup);
	    if (ExecQual(pred, econtext) == false)
		continue;
	}

	ni++;

	/*
	 *  For the current heap tuple, extract all the attributes
	 *  we use in this index, and note which are null.
	 */

	for (i = 1; i <= natts; i++) {
	    AttributeOffset attoff;
	    Boolean attnull;

	    /*
	     *  Offsets are from the start of the tuple, and are
	     *  zero-based; indices are one-based.  The next call
	     *  returns i - 1.  That's data hiding for you.
	     */

	    attoff = AttributeNumberGetAttributeOffset(i);
	    /*
	    d[attoff] = HeapTupleGetAttributeValue(htup, buffer,
	    */
	    d[attoff] = GetIndexValue(htup, 
				      hd,
				      attoff, 
				      attnum, 
				      finfo, 
				      &attnull,
				      buffer);
	    nulls[attoff] = (attnull ? 'n' : ' ');
	}

	/* form an index tuple and point it at the heap tuple */
	itup = FormIndexTuple(natts, id, &d[0], nulls);
	itup->t_tid = htup->t_ctid;

	/*
	 *  Since we already have the index relation locked, we
	 *  call rtdoinsert directly.  Normal access method calls
	 *  dispatch through rtinsert, which locks the relation
	 *  for write.  This is the right thing to do if you're
	 *  inserting single tups, but not when you're initializing
	 *  the whole index at once.
	 */

	res = rtdoinsert(index, itup);
	pfree(itup);
	pfree(res);
    }

    /* okay, all heap tuples are indexed */
    heap_endscan(scan);
    RelationUnsetLockForWrite(index);

    if (pred != LispNil || oldPred != LispNil) {
	ExecDestroyTupleTable(tupleTable, true);
	pfree(econtext);
    }

    /*
     *  Since we just counted the tuples in the heap, we update its
     *  stats in pg_relation to guarantee that the planner takes
     *  advantage of the index we just created.  UpdateStats() does a
     *  CommandCounterIncrement(), which flushes changed entries from
     *  the system relcache.  The act of constructing an index changes
     *  these heap and index tuples in the system catalogs, so they
     *  need to be flushed.  We close them to guarantee that they
     *  will be.
     */

    hrelid = heap->rd_id;
    irelid = index->rd_id;
    heap_close(heap);
    index_close(index);

    UpdateStats(hrelid, nh, true);
    UpdateStats(irelid, ni, false);

    if (oldPred != LispNil) {
	if (ni == nh) pred = LispNil;
	UpdateIndexPredicate(irelid, oldPred, pred);
    }

    /* be tidy */
    pfree(nulls);
    pfree(d);
}

/*
 *  rtinsert -- wrapper for rtree tuple insertion.
 *
 *    This is the public interface routine for tuple insertion in rtrees.
 *    It doesn't do any work; just locks the relation and passes the buck.
 */

InsertIndexResult
rtinsert(r, itup)
    Relation r;
    IndexTuple itup;
{
    InsertIndexResult res;

    RelationSetLockForWrite(r);
    res = rtdoinsert(r, itup);

    /* XXX two-phase locking -- don't unlock the relation until EOT */
    return (res);
}

InsertIndexResult
rtdoinsert(r, itup)
    Relation r;
    IndexTuple itup;
{
    Page page;
    Buffer buffer;
    BlockNumber blk;
    IndexTuple which;
    OffsetNumber l = 0;	/* XXX this is never set, but it's read below!? */
    RTSTACK *stack;
    InsertIndexResult res;
    RTreePageOpaque opaque;
    char *datum;

    blk = P_ROOT;
    buffer = InvalidBuffer;
    stack = (RTSTACK *) NULL;

    do {
	/* let go of current buffer before getting next */
	if (buffer != InvalidBuffer)
	    ReleaseBuffer(buffer);

	/* get next buffer */
	buffer = ReadBuffer(r, blk);
	page = (Page) BufferGetPage(buffer, 0);

	opaque = (RTreePageOpaque) PageGetSpecialPointer(page);
	if (!(opaque->flags & F_LEAF)) {
	    RTSTACK *n;
	    ItemId iid;

	    n = (RTSTACK *) palloc(sizeof(RTSTACK));
	    n->rts_parent = stack;
	    n->rts_blk = blk;
	    n->rts_child = choose(r, page, itup);
	    stack = n;

	    iid = PageGetItemId(page, n->rts_child);
	    which = (IndexTuple) PageGetItem(page, iid);
	    blk = ItemPointerGetBlockNumber(&(which->t_tid));
	}
    } while (!(opaque->flags & F_LEAF));

    if (nospace(page, itup)) {
	/* need to do a split */
	res = dosplit(r, buffer, stack, itup);
	freestack(stack);
	WriteBuffer(buffer);  /* don't forget to release buffer! */
	return (res);
    }

    /* add the item and write the buffer */
    if (PageIsEmpty(page))
	PageAddItem(page, (Item) itup, IndexTupleSize(itup), 1, LP_USED);
    else
	PageAddItem(page, (Item) itup, IndexTupleSize(itup),
		    PageGetMaxOffsetIndex(page) + 2, LP_USED);

    WriteBuffer(buffer);

    datum = (((char *) itup) + sizeof(IndexTupleData));

    /* now expand the page boundary in the parent to include the new child */
    rttighten(r, stack, datum, (IndexTupleSize(itup) - sizeof(IndexTupleData)));
    freestack(stack);

    /* build and return an InsertIndexResult for this insertion */
    res = (InsertIndexResult) palloc(sizeof(InsertIndexResultData));
    /* XXX "l" is never set! */
    ItemPointerSet(&(res->pointerData), 0, blk, 0, l);
    res->lock = (RuleLock) NULL;
    res->offset = (double) 0;

    return (res);
}

rttighten(r, stk, datum, att_size)
    Relation r;
    RTSTACK *stk;
    char *datum;
    int att_size;
{
    char *oldud;
    char *tdatum;
    Page p;
    float *old_size, *newd_size;
    RegProcedure union_proc, size_proc;
    Buffer b;

    if (stk == (RTSTACK *) NULL)
	return;

    b = ReadBuffer(r, stk->rts_blk);
    p = BufferGetPage(b, 0);

    union_proc = index_getprocid(r, 1, RT_UNION_PROC);
    size_proc = index_getprocid(r, 1, RT_SIZE_PROC);

    oldud = (char *) PageGetItem(p, PageGetItemId(p, stk->rts_child));
    oldud += sizeof(IndexTupleData);

    old_size = (float *) fmgr(size_proc, oldud);
    datum = (char *) fmgr(union_proc, oldud, datum);

    newd_size = (float *) fmgr(size_proc, datum);

    if (*newd_size != *old_size) {
	TupleDescriptor td = RelationGetTupleDescriptor(r);

	if (td->data[0]->attlen < 0) {
	    /*
	     * This is an internal page, so 'oldud' had better be a
	     * union (constant-length) key, too.  (See comment below.)
	     */
	    Assert(VARSIZE(datum) == VARSIZE(oldud));
	    bcopy(datum, oldud, VARSIZE(datum));
	} else {
	    bcopy(datum, oldud, att_size);
	}
	WriteBuffer(b);

	/*
	 *  The user may be defining an index on variable-sized data (like
	 *  polygons).  If so, we need to get a constant-sized datum for
	 *  insertion on the internal page.  We do this by calling the union
	 *  proc, which is guaranteed to return a rectangle.
	 */

	tdatum = (char *) fmgr(union_proc, datum, datum);
	rttighten(r, stk->rts_parent, tdatum, att_size);
	pfree(tdatum);
    } else {
	ReleaseBuffer(b);
    }
    pfree(datum);
}

/*
 *  dosplit -- split a page in the tree.
 *
 *    This is the quadratic-cost split algorithm Guttman describes in
 *    his paper.  The reason we chose it is that you can implement this
 *    with less information about the data types on which you're operating.
 */

InsertIndexResult
dosplit(r, buffer, stack, itup)
    Relation r;
    Buffer buffer;
    RTSTACK *stack;
    IndexTuple itup;
{
    Page p;
    Buffer leftbuf, rightbuf;
    Page left, right;
    ItemId itemid;
    IndexTuple item;
    IndexTuple ltup, rtup;
    OffsetNumber maxoff;
    OffsetIndex i;
    OffsetIndex leftoff, rightoff;
    BlockNumber lbknum, rbknum;
    BlockNumber bufblock;
    RTreePageOpaque opaque;
    int blank;
    InsertIndexResult res;
    char *isnull;
    SPLITVEC v;

    isnull = (char *) palloc(r->rd_rel->relnatts);
    for (blank = 0; blank < r->rd_rel->relnatts; blank++)
	isnull[blank] = ' ';
    p = (Page) BufferGetPage(buffer, 0);
    opaque = (RTreePageOpaque) PageGetSpecialPointer(p);

    /*
     *  The root of the tree is the first block in the relation.  If
     *  we're about to split the root, we need to do some hocus-pocus
     *  to enforce this guarantee.
     */

    if (BufferGetBlockNumber(buffer) == P_ROOT) {
	leftbuf = ReadBuffer(r, P_NEW);
	RTInitBuffer(leftbuf, opaque->flags);
	lbknum = BufferGetBlockNumber(leftbuf);
	left = (Page) BufferGetPage(leftbuf, 0);
    } else {
	leftbuf = buffer;
	IncrBufferRefCount(buffer);
	lbknum = BufferGetBlockNumber(buffer);
	left = (Page) PageGetTempPage(p, sizeof(RTreePageOpaqueData));
    }

    rightbuf = ReadBuffer(r, P_NEW);
    RTInitBuffer(rightbuf, opaque->flags);
    rbknum = BufferGetBlockNumber(rightbuf);
    right = (Page) BufferGetPage(rightbuf, 0);

    picksplit(r, p, &v, itup);

    leftoff = rightoff = 0;
    maxoff = PageGetMaxOffsetIndex(p);
    for (i = 0; i <= maxoff; i++) {
	char *dp;

	itemid = PageGetItemId(p, i);
	item = (IndexTuple) PageGetItem(p, itemid);

	if (i == *(v.spl_left)) {
	    (void) PageAddItem(left, (Item) item, IndexTupleSize(item),
			       leftoff++, LP_USED);
	    v.spl_left++;
	} else {
	    (void) PageAddItem(right, (Item) item, IndexTupleSize(item),
			       rightoff++, LP_USED);
	    v.spl_right++;
	}
    }

    /* build an InsertIndexResult for this insertion */
    res = (InsertIndexResult) palloc(sizeof(InsertIndexResultData));
    res->lock = (RuleLock) NULL;
    res->offset = (double) 0;

    /* now insert the new index tuple */
    if (*(v.spl_left) != (OffsetNumber) 0) {
	(void) PageAddItem(left, (Item) itup, IndexTupleSize(itup),
			   leftoff++, LP_USED);
	ItemPointerSet(&(res->pointerData), 0, lbknum, 0, leftoff);
    } else {
	(void) PageAddItem(right, (Item) itup, IndexTupleSize(itup),
			   rightoff++, LP_USED);
	ItemPointerSet(&(res->pointerData), 0, rbknum, 0, rightoff);
    }

    if ((bufblock = BufferGetBlockNumber(buffer)) != P_ROOT) {
	PageRestoreTempPage(left, p);
    }
    WriteBuffer(leftbuf);
    WriteBuffer(rightbuf);

    /*
     *  Okay, the page is split.  We have three things left to do:
     *
     *    1)  Adjust any active scans on this index to cope with changes
     *        we introduced in its structure by splitting this page.
     *
     *    2)  "Tighten" the bounding box of the pointer to the left
     *	      page in the parent node in the tree, if any.  Since we
     *	      moved a bunch of stuff off the left page, we expect it
     *	      to get smaller.  This happens in the internal insertion
     *        routine.
     *
     *    3)  Insert a pointer to the right page in the parent.  This
     *	      may cause the parent to split.  If it does, we need to
     *	      repeat steps one and two for each split node in the tree.
     */

    /* adjust active scans */
    rtadjscans(r, RTOP_SPLIT, bufblock, 0);

    ltup = (IndexTuple) index_formtuple(r->rd_rel->relnatts,
				   (TupleDescriptor) (&r->rd_att.data[0]),
			           (Datum *) &(v.spl_ldatum), isnull);
    rtup = (IndexTuple) index_formtuple(r->rd_rel->relnatts,
				   (TupleDescriptor) &r->rd_att.data[0],
				   (Datum *) &(v.spl_rdatum), isnull);
    pfree (isnull);

    /* set pointers to new child pages in the internal index tuples */
    ItemPointerSet(&(ltup->t_tid), 0, lbknum, 0, 1);
    ItemPointerSet(&(rtup->t_tid), 0, rbknum, 0, 1);

    rtintinsert(r, stack, ltup, rtup);

    pfree(ltup);
    pfree(rtup);

    return (res);
}

rtintinsert(r, stk, ltup, rtup)
    Relation r;
    RTSTACK *stk;
    IndexTuple ltup;
    IndexTuple rtup;
{
    IndexTuple old;
    IndexTuple it;
    Buffer b;
    Page p;
    RegProcedure union_proc, size_proc;
    char *ldatum, *rdatum, *newdatum;
    InsertIndexResult res;

    if (stk == (RTSTACK *) NULL) {
	rtnewroot(r, ltup, rtup);
	return;
    }

    union_proc = index_getprocid(r, 1, RT_UNION_PROC);
    b = ReadBuffer(r, stk->rts_blk);
    p = BufferGetPage(b, 0);
    old = (IndexTuple) PageGetItem(p, PageGetItemId(p, stk->rts_child));

    /*
     *  This is a hack.  Right now, we force rtree keys to be constant size.
     *  To fix this, need delete the old key and add both left and right
     *  for the two new pages.  The insertion of left may force a split if
     *  the new left key is bigger than the old key.
     */

    if (IndexTupleSize(old) != IndexTupleSize(ltup))
	elog(WARN, "Variable-length rtree keys are not supported.");

    /* install pointer to left child */
    bcopy(ltup, old, IndexTupleSize(ltup));

    if (nospace(p, rtup)) {
	newdatum = (((char *) ltup) + sizeof(IndexTupleData));
	rttighten(r, stk->rts_parent, newdatum,
		  (IndexTupleSize(ltup) - sizeof(IndexTupleData)));
	res = dosplit(r, b, stk->rts_parent, rtup);
	WriteBuffer(b);  /* don't forget to release buffer!  - 01/31/94 */
	pfree (res);
    } else {
	PageAddItem(p, (Item) rtup, IndexTupleSize(rtup),
		    PageGetMaxOffsetIndex(p), LP_USED);
	WriteBuffer(b);
	ldatum = (((char *) ltup) + sizeof(IndexTupleData));
	rdatum = (((char *) rtup) + sizeof(IndexTupleData));
	newdatum = (char *) fmgr(union_proc, ldatum, rdatum);

	rttighten(r, stk->rts_parent, newdatum,
		  (IndexTupleSize(rtup) - sizeof(IndexTupleData)));

	pfree(newdatum);
    }
}

rtnewroot(r, lt, rt)
    Relation r;
    IndexTuple lt;
    IndexTuple rt;
{
    Buffer b;
    Page p;

    b = ReadBuffer(r, P_ROOT);
    RTInitBuffer(b, 0);
    p = BufferGetPage(b, 0);
    PageAddItem(p, (Item) lt, IndexTupleSize(lt), 0, LP_USED);
    PageAddItem(p, (Item) rt, IndexTupleSize(rt), 1, LP_USED);
    WriteBuffer(b);
}

picksplit(r, page, v, itup)
    Relation r;
    Page page;
    SPLITVEC *v;
    IndexTuple itup;
{
    OffsetNumber maxoff;
    OffsetNumber i, j;
    IndexTuple item_1, item_2;
    char *datum_alpha, *datum_beta;
    char *datum_l, *datum_r;
    char *union_d, *union_dl, *union_dr;
    char *inter_d;
    RegProcedure union_proc;
    RegProcedure size_proc;
    RegProcedure inter_proc;
    bool firsttime;
    float *size_alpha, *size_beta, *size_union, *size_inter;
    float size_waste, waste;
    float *size_l, *size_r;
    int nbytes;
    OffsetNumber seed_1 = 0, seed_2 = 0;
    OffsetNumber *left, *right;

    union_proc = index_getprocid(r, 1, RT_UNION_PROC);
    size_proc = index_getprocid(r, 1, RT_SIZE_PROC);
    inter_proc = index_getprocid(r, 1, RT_INTER_PROC);
    maxoff = PageGetMaxOffsetIndex(page);

    nbytes = (maxoff + 3) * sizeof(OffsetNumber);
    v->spl_left = (OffsetNumber *) palloc(nbytes);
    v->spl_right = (OffsetNumber *) palloc(nbytes);

    firsttime = true;
    waste = 0;

    for (i = 0; i < maxoff; i++) {
	item_1 = (IndexTuple) PageGetItem(page, PageGetItemId(page, i));
	datum_alpha = ((char *) item_1) + sizeof(IndexTupleData);
	for (j = i + 1; j <= maxoff; j++) {
	    item_2 = (IndexTuple) PageGetItem(page, PageGetItemId(page, j));
	    datum_beta = ((char *) item_2) + sizeof(IndexTupleData);

	    /* compute the wasted space by unioning these guys */
	    union_d = (char *) fmgr(union_proc, datum_alpha, datum_beta);
	    size_union = (float *) fmgr(size_proc, union_d);
	    inter_d = (char *) fmgr(inter_proc, datum_alpha, datum_beta);
	    size_inter = (float *) fmgr(size_proc, inter_d);
	    size_waste = *size_union - *size_inter;

	    pfree(union_d);
	    pfree(size_union);
	    pfree(size_inter);

	    if (inter_d != (char *) NULL)
		    pfree(inter_d);

	    /*
	     *  are these a more promising split that what we've
	     *  already seen?
	     */

	    if (size_waste > waste || firsttime) {
		waste = size_waste;
		seed_1 = i;
		seed_2 = j;
		firsttime = false;
	    }
	}
    }

    left = v->spl_left;
    v->spl_nleft = 0;
    right = v->spl_right;
    v->spl_nright = 0;

    item_1 = (IndexTuple) PageGetItem(page, PageGetItemId(page, seed_1));
    datum_alpha = ((char *) item_1) + sizeof(IndexTupleData);
    datum_l = (char *) fmgr(union_proc, datum_alpha, datum_alpha);
    size_l = (float *) fmgr(size_proc, datum_l);
    item_2 = (IndexTuple) PageGetItem(page, PageGetItemId(page, seed_2));
    datum_beta = ((char *) item_2) + sizeof(IndexTupleData);
    datum_r = (char *) fmgr(union_proc, datum_beta, datum_beta);
    size_r = (float *) fmgr(size_proc, datum_r);

    /*
     *  Now split up the regions between the two seeds.  An important
     *  property of this split algorithm is that the split vector v
     *  has the indices of items to be split in order in its left and
     *  right vectors.  We exploit this property by doing a merge in
     *  the code that actually splits the page.
     *
     *  For efficiency, we also place the new index tuple in this loop.
     *  This is handled at the very end, when we have placed all the
     *  existing tuples and i == maxoff + 1.
     */

    maxoff++;
    for (i = 0; i <= maxoff; i++) {

	/*
	 *  If we've already decided where to place this item, just
	 *  put it on the right list.  Otherwise, we need to figure
	 *  out which page needs the least enlargement in order to
	 *  store the item.
	 */

	if (i == seed_1) {
	    *left++ = i;
	    v->spl_nleft++;
	    continue;
	} else if (i == seed_2) {
	    *right++ = i;
	    v->spl_nright++;
	    continue;
	}

	/* okay, which page needs least enlargement? */ 
	if (i == maxoff)
	    item_1 = itup;
	else
	    item_1 = (IndexTuple) PageGetItem(page, PageGetItemId(page, i));

	datum_alpha = ((char *) item_1) + sizeof(IndexTupleData);
	union_dl = (char *) fmgr(union_proc, datum_l, datum_alpha);
	union_dr = (char *) fmgr(union_proc, datum_r, datum_alpha);
	size_alpha = (float *) fmgr(size_proc, union_dl);
	size_beta = (float *) fmgr(size_proc, union_dr);

	/* pick which page to add it to */
	if (*size_alpha - *size_l < *size_beta - *size_r) {
	    pfree(datum_l);
	    pfree(union_dr);
	    datum_l = union_dl;
	    *size_l = *size_alpha;
	    *left++ = i;
	    v->spl_nleft++;
	} else {
	    pfree(datum_r);
	    pfree(union_dl);
	    datum_r = union_dr;
	    *size_r = *size_alpha;
	    *right++ = i;
	    v->spl_nright++;
	}

	pfree(size_alpha);
	pfree(size_beta);
    }
    *left = *right = (OffsetNumber)0;

    v->spl_ldatum = datum_l;
    v->spl_rdatum = datum_r;

    pfree(size_l);
    pfree(size_r);
}

RTInitBuffer(b, f)
    Buffer b;
    uint32 f;
{
    RTreePageOpaque opaque;
    Page page;
    PageSize pageSize;

    pageSize = BufferGetBlockSize(b) / PagePartitionGetPagesPerBlock(0);
    BufferInitPage(b, pageSize);

    page = BufferGetPage(b, FirstPageNumber);
    bzero(page, (int) pageSize);
    PageInit(page, pageSize, sizeof(RTreePageOpaqueData));

    opaque = (RTreePageOpaque) PageGetSpecialPointer(page);
    opaque->flags = f;
}

choose(r, p, it)
    Relation r;
    Page p;
    IndexTuple it;
{
    int maxoff;
    int i;
    char *ud, *id;
    char *datum;
    float *isize, *usize, *dsize;
    int which;
    float which_grow;
    RegProcedure union_proc, size_proc;

    union_proc = index_getprocid(r, 1, RT_UNION_PROC);
    size_proc = index_getprocid(r, 1, RT_SIZE_PROC);
    id = ((char *) it) + sizeof(IndexTupleData);
    isize = (float *) fmgr(size_proc, id);
    maxoff = PageGetMaxOffsetIndex(p);
    which_grow = -1;
    which = -1;

    for (i = 0; i <= maxoff; i++) {
	datum = (char *) PageGetItem(p, PageGetItemId(p, i));
	datum += sizeof(IndexTupleData);
	dsize = (float *) fmgr(size_proc, datum);
	ud = (char *) fmgr(union_proc, datum, id);
	usize = (float *) fmgr(size_proc, ud);
	pfree(ud);
	if (which_grow < 0 || *usize - *dsize < which_grow) {
	    which = i;
	    which_grow = *usize - *dsize;
	    if (which_grow == 0)
		break;
	}
	pfree(usize);
	pfree(dsize);
    }

    pfree(isize);
    return (which);
}

int
nospace(p, it)
    Page p;
    IndexTuple it;
{
    return (PageGetFreeSpace(p) < IndexTupleSize(it));
}

freestack(s)
    RTSTACK *s;
{
    RTSTACK *p;

    while (s != (RTSTACK *) NULL) {
	p = s->rts_parent;
	pfree (s);
	s = p;
    }
}

char *
rtdelete(r, tid)
    Relation r;
    ItemPointer tid;
{
    BlockNumber blkno;
    OffsetIndex offind;
    Buffer buf;
    Page page;

    /* must write-lock on delete */
    RelationSetLockForWrite(r);

    blkno = ItemPointerGetBlockNumber(tid);
    offind = ItemPointerGetOffsetNumber(tid, 0) - 1;

    /* adjust any scans that will be affected by this deletion */
    rtadjscans(r, RTOP_DEL, blkno, offind);

    /* delete the index tuple */
    buf = ReadBuffer(r, blkno);
    page = BufferGetPage(buf, 0);

    PageIndexTupleDelete(page, offind + 1);

    WriteBuffer(buf);

    /* XXX -- two-phase locking, don't release the write lock */
    return ((char *) NULL);
}

#define RTDEBUG
#ifdef RTDEBUG
#include "utils/geo-decls.h"

extern char *box_out ARGS((BOX *box));	/* XXX builtins.h clash */

_rtdump(r)
    Relation r;
{
    Buffer buf;
    Page page;
    OffsetIndex offind, maxoff;
    BlockNumber blkno;
    BlockNumber nblocks;
    RTreePageOpaque po;
    IndexTuple itup;
    BlockNumber itblkno;
    PageNumber itpgno;
    OffsetNumber itoffno;
    char *datum;
    char *itkey;

    nblocks = RelationGetNumberOfBlocks(r);
    for (blkno = 0; blkno < nblocks; blkno++) {
	buf = ReadBuffer(r, blkno);
	page = BufferGetPage(buf, 0);
	po = (RTreePageOpaque) PageGetSpecialPointer(page);
	maxoff = PageGetMaxOffsetIndex(page);
	printf("Page %d maxoff %d <%s>\n", blkno, maxoff,
		(po->flags & F_LEAF ? "LEAF" : "INTERNAL"));

	if (PageIsEmpty(page)) {
	    ReleaseBuffer(buf);
	    continue;
	}

	for (offind = 0; offind <= maxoff; offind++) {
	    itup = (IndexTuple) PageGetItem(page, PageGetItemId(page, offind));
	    itblkno = ItemPointerGetBlockNumber(&(itup->t_tid));
	    itpgno = ItemPointerGetPageNumber(&(itup->t_tid), 0);
	    itoffno = ItemPointerGetOffsetNumber(&(itup->t_tid), 0);
	    datum = ((char *) itup);
	    datum += sizeof(IndexTupleData);
	    itkey = (char *) box_out((BOX *) datum);
	    printf("\t[%d] size %d heap <%d,%d,%d> key:%s\n",
		   offind + 1, IndexTupleSize(itup), itblkno, itpgno,
		   itoffno, itkey);
	    pfree(itkey);
	}

	ReleaseBuffer(buf);
    }
}
#endif /* defined RTDEBUG */
