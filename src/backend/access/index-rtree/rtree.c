/*
 *  rtree.c -- interface routines for the postgres rtree indexed access
 *	   method.
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
#include "access/rtree.h"

RcsId("$Header$");

extern InsertIndexResult    rtdoinsert();
extern InsertIndexResult    dosplit();
extern int	            nospace();

typedef struct SPLITVEC {
    OffsetNumber	*spl_left;
    int			spl_nleft;
    char		*spl_ldatum;
    OffsetNumber	*spl_right;
    int			spl_nright;
    char		*spl_rdatum;
} SPLITVEC;

void
rtbuild(heap, index, natts, attnum, istrat, pcount, params)
    Relation heap;
    Relation index;
    AttributeNumber natts;
    AttributeNumber *attnum;
    IndexStrategy istrat;
    uint16 pcount;
    Datum *params;
{
    HeapScanDesc scan;
    Buffer buffer;
    AttributeNumber i;
    HeapTuple htup;
    IndexTuple itup;
    TupleDescriptor hd, id;
    Datum *d;
    Boolean *null;
    int n;

    /* rtrees only know how to do stupid locking now */
    RelationSetLockForWrite(index);

    /*
     *  We expect to be called exactly once for any index relation.
     *  If that's not the case, big trouble's what we have.
     */

    if ((n = RelationGetNumberOfBlocks(index)) != 0)
	elog(WARN, "%.16s already contains data", index->rd_rel->relname);

    /* initialize the root page */
    buffer = ReadBuffer(index, P_NEW);
    RTInitBuffer(buffer, F_LEAF);
    WriteBuffer(buffer);

    /* init the tuple descriptors and get set for a heap scan */
    hd = RelationGetTupleDescriptor(heap);
    id = RelationGetTupleDescriptor(index);
    d = LintCast(Datum *, palloc(natts * sizeof (*d)));
    null = LintCast(Boolean *, palloc(natts * sizeof (*null)));

    scan = heap_beginscan(heap, 0, NowTimeQual, 0, (ScanKey) NULL);
    htup = heap_getnext(scan, 0, &buffer);

    /* count the tuples as we insert them */
    n = 0;

    while (HeapTupleIsValid(htup)) {

	n++;

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
	    d[attoff] = HeapTupleGetAttributeValue(htup, buffer,
			attnum[attoff], hd, &attnull);
	    null[attoff] = (attnull ? 'n' : ' ');
	}

	/* form an index tuple and point it at the heap tuple */
	itup = FormIndexTuple(natts, id, d, null);
	itup->t_tid = htup->t_ctid;

	/*
	 *  Since we already have the index relation locked, we
	 *  call rtdoinsert directly.  Normal access method calls
	 *  dispatch through rtinsert, which locks the relation
	 *  for write.  This is the right thing to do if you're
	 *  inserting single tups, but not when you're initializing
	 *  the whole index at once.  As a side effect, rtdoinsert
	 *  pfree's the tuple after insertion.
	 */

	rtdoinsert(index, itup);

	/* do the next tuple in the heap */
	htup = heap_getnext(scan, 0, &buffer);
    }

    /* okay, all heap tuples are indexed */
    heap_endscan(scan);
    RelationUnsetLockForWrite(index);

    /*
     *  Since we just counted the tuples in the heap, we update its
     *  stats in pg_relation to guarantee that the planner takes
     *  advantage of the index we just created.
     */

    UpdateStats(heap, n);

    /* be tidy */
    pfree(null);
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
    RelationUnsetLockForWrite(r);
    return (res);
}

InsertIndexResult
rtdoinsert(r, itup)
    Relation r;
    IndexTuple itup;
{
    PageHeader page;
    Buffer buffer;
    BlockNumber blk;
    IndexTuple which;
    OffsetNumber l;
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
	page = (PageHeader) BufferGetPage(buffer, 0);

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
	return (res);
    }

    /* add the item and write the buffer */
    PageAddItem(page, itup, itup->t_size, PageGetMaxOffsetIndex(page), LP_USED);
    WriteBuffer(buffer);

    datum = (((char *) itup) + sizeof(IndexTupleData));

    /* now expand the page boundary in the parent to include the new child */
    rttighten(r, stack, datum, (itup->t_size - sizeof(IndexTupleData)));
    freestack(stack);

    pfree(itup);

    /* build and return an InsertIndexResult for this insertion */
    res = (InsertIndexResult) palloc(sizeof(InsertIndexResultData));
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
    Page p;
    int old_size, newd_size;
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

    old_size = (int) fmgr(size_proc, oldud);
    datum = (char *) fmgr(union_proc, oldud, datum);

    newd_size = (int) fmgr(size_proc, datum);

    /* XXX assume constant-size data items here */
    if (newd_size != old_size) {
	bcopy(datum, oldud, att_size);
	WriteBuffer(b);
	rttighten(r, stk->rts_parent, datum, att_size);
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
    PageHeader p;
    Buffer leftbuf, rightbuf;
    PageHeader left, right;
    ItemId itemid;
    IndexTuple item;
    IndexTuple ltup, rtup;
    OffsetNumber maxoff;
    OffsetIndex i;
    OffsetIndex leftoff, rightoff;
    BlockNumber lbknum, rbknum;
    RTreePageOpaque opaque;
    int blank;
    InsertIndexResult res;
    char *isnull;
    SPLITVEC v;

    isnull = (char *) palloc(r->rd_rel->relnatts);
    for (blank = 0; blank < r->rd_rel->relnatts; blank++)
	isnull[blank] = ' ';
    p = (PageHeader) BufferGetPage(buffer, 0);
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
	left = (PageHeader) BufferGetPage(leftbuf, 0);
    } else {
	leftbuf = buffer;
	lbknum = BufferGetBlockNumber(buffer);
	left = (PageHeader) PageGetTempPage(p, sizeof(RTreePageOpaqueData));
    }

    rightbuf = ReadBuffer(r, P_NEW);
    RTInitBuffer(rightbuf, opaque->flags);
    rbknum = BufferGetBlockNumber(rightbuf);
    right = (PageHeader) BufferGetPage(rightbuf, 0);

    picksplit(r, p, &v, itup);

    leftoff = rightoff = 0;
    maxoff = PageGetMaxOffsetIndex(p);
    for (i = 0; i <= maxoff; i++) {
	char *dp;

	itemid = PageGetItemId(p, i);
	item = (IndexTuple) PageGetItem(p, itemid);

	if (i == *(v.spl_left)) {
	    (void) PageAddItem(left, item, item->t_size, leftoff++, LP_USED);
	    v.spl_left++;
	} else {
	    (void) PageAddItem(right, item, item->t_size, rightoff++, LP_USED);
	    v.spl_right++;
	}
    }

    /* build an InsertIndexResult for this insertion */
    res = (InsertIndexResult) palloc(sizeof(InsertIndexResultData));
    res->lock = (RuleLock) NULL;
    res->offset = (double) 0;

    /* now insert the new index tuple */
    if (*(v.spl_left) != (OffsetNumber) 0) {
	(void) PageAddItem(left, itup, itup->t_size, leftoff++, LP_USED);
	ItemPointerSet(&(res->pointerData), 0, lbknum, 0, leftoff);
    } else {
	(void) PageAddItem(right, itup, itup->t_size, rightoff++, LP_USED);
	ItemPointerSet(&(res->pointerData), 0, rbknum, 0, rightoff);
    }

    if (BufferGetBlockNumber(buffer) != P_ROOT) {
	PageRestoreTempPage(left, p);
    }
    WriteBuffer(leftbuf);
    WriteBuffer(rightbuf);

    /*
     *  Okay, the page is split.  We have two things left to do:
     *
     *    1)  "Tighten" the bounding box of the pointer to the left
     *	      page in the parent node in the tree, if any.  Since we
     *	      moved a bunch of stuff off the left page, we expect it
     *	      to get smaller.
     *
     *    2)  Insert a pointer to the right page in the parent.  This
     *	      may cause the parent to split.  If it does, we need to
     *	      repeat steps one and two for each split node in the tree.
     */

    ltup = (IndexTuple) formituple(r->rd_rel->relnatts, &r->rd_att.data[0],
			           &(v.spl_ldatum), isnull);
    rtup = (IndexTuple) formituple(r->rd_rel->relnatts, &r->rd_att.data[0],
				   &(v.spl_rdatum), isnull);
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

    if (old->t_size != ltup->t_size)
	elog(WARN, "Variable-length rtree keys are not supported.");

    /* install pointer to left child */
    bcopy(ltup, old, ltup->t_size);

    if (nospace(p, rtup)) {
	newdatum = (((char *) ltup) + sizeof(IndexTupleData));
	rttighten(r, stk->rts_parent, newdatum,
		  (ltup->t_size - sizeof(IndexTupleData)));
	res = dosplit(r, b, stk->rts_parent, rtup);
	pfree (res);
    } else {
	PageAddItem(p, rtup, rtup->t_size, PageGetMaxOffsetIndex(p), LP_USED);
	WriteBuffer(b);
	ldatum = (((char *) ltup) + sizeof(IndexTupleData));
	rdatum = (((char *) rtup) + sizeof(IndexTupleData));
	newdatum = (char *) fmgr(union_proc, ldatum, rdatum);

	rttighten(r, stk->rts_parent, newdatum,
		  (rtup->t_size - sizeof(IndexTupleData)));

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
    PageAddItem(p, lt, lt->t_size, 0, LP_USED);
    PageAddItem(p, rt, rt->t_size, 1, LP_USED);
    WriteBuffer(b);
}

picksplit(r, page, v, itup)
    Relation r;
    PageHeader page;
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
    int waste;
    int size_alpha, size_beta, size_union, size_waste, size_inter;
    int size_l, size_r;
    int nbytes;
    OffsetNumber seed_1, seed_2;
    OffsetNumber *left, *right;

    union_proc = index_getprocid(r, 1, RT_UNION_PROC);
    size_proc = index_getprocid(r, 1, RT_SIZE_PROC);
    inter_proc = index_getprocid(r, 1, RT_INTER_PROC);
    maxoff = PageGetMaxOffsetIndex(page);

    nbytes = (maxoff + 2) * sizeof(OffsetNumber);
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
	    size_union = (int) fmgr(size_proc, union_d);
	    inter_d = (char *) fmgr(inter_proc, datum_alpha, datum_beta);
	    size_inter = (int) fmgr(size_proc, inter_d);
	    size_waste = size_union - size_inter;

	    pfree(union_d);
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
    size_l = (int) fmgr(size_proc, datum_l);
    item_2 = (IndexTuple) PageGetItem(page, PageGetItemId(page, seed_2));
    datum_beta = ((char *) item_2) + sizeof(IndexTupleData);
    datum_r = (char *) fmgr(union_proc, datum_beta, datum_beta);
    size_r = (int) fmgr(size_proc, datum_r);

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
	size_alpha = (int) fmgr(size_proc, union_dl);
	size_beta = (int) fmgr(size_proc, union_dr);

	/* pick which page to add it to */
	if (size_alpha - size_l < size_beta - size_r) {
	    pfree(datum_l);
	    pfree(union_dr);
	    datum_l = union_dl;
	    size_l = size_alpha;
	    *left++ = i;
	    v->spl_nleft++;
	} else {
	    pfree(datum_r);
	    pfree(union_dl);
	    datum_r = union_dr;
	    size_r = size_alpha;
	    *right++ = i;
	    v->spl_nright++;
	}
    }
    v->spl_ldatum = datum_l;
    v->spl_rdatum = datum_r;
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
    int isize, usize, dsize;
    int which, which_grow;
    RegProcedure union_proc, size_proc;

    union_proc = index_getprocid(r, 1, RT_UNION_PROC);
    size_proc = index_getprocid(r, 1, RT_SIZE_PROC);
    id = ((char *) it) + sizeof(IndexTupleData);
    isize = (int) fmgr(size_proc, id);
    maxoff = PageGetMaxOffsetIndex(p);
    which_grow = -1;
    which = -1;

    for (i = 0; i <= maxoff; i++) {
	datum = (char *) PageGetItem(p, PageGetItemId(p, i));
	datum += sizeof(IndexTupleData);
	dsize = (int) fmgr(size_proc, datum);
	ud = (char *) fmgr(union_proc, datum, id);
	usize = (int) fmgr(size_proc, ud);
	pfree(ud);
	if (which_grow < 0 || usize - dsize < which_grow) {
	    which = i;
	    which_grow = usize - dsize;
	    if (which_grow == 0)
		break;
	}
    }
    return (which);
}

int
nospace(p, it)
    Page p;
    IndexTuple it;
{
    return (PageGetFreeSpace(p) < it->t_size);
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

/* ================================================================ */
char *
rtdelete()
{
    return (char *) NULL;
}
