/*
 *  rtree.c -- interface routines for the postgres rtree indexed access
 *	       method.
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

extern InsertIndexResult	rtdoinsert();
extern PageHeader		dosplit();

typedef struct RTSTACK {
	struct RTSTACK	*rts_parent;
	OffsetNumber	rts_child;
	BlockNumber	rts_blk;
} RTSTACK;

typedef struct SPLITVEC {
	OffsetNumber	*spl_left;
	int		spl_nleft;
	char		*spl_ldatum;
	OffsetNumber	*spl_right;
	int		spl_nright;
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

	if (RelationGetNumberOfBlocks != 0)
		elog(WARN, "%.16s already contains data",
		     index->rd_rel->relname);

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
		 *  the whole index at once.
		 */

		rtdoinsert(index, itup);

		/* done with this tuple */
		pfree(itup);

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
 *	This is the public interface routine for tuple insertion in rtrees.
 *	It doesn't do any work; just locks the relation and passes the buck.
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
	RTSTACK *stack;
	OffsetNumber l;
	bool split;
	InsertIndexResult res;

	blk = P_ROOT;
	buffer = InvalidBuffer;
	stack = (RTSTACK *) NULL;
	split = false;

	do {
		/* let go of current buffer before getting next */
		if (buffer != InvalidBuffer)
			ReleaseBuffer(buffer);

		/* get next buffer */
		buffer = ReadBuffer(r, blk);
		page = (PageHeader) BufferGetPage(buffer, 0);

		if (!(page->opaque & F_LEAF)) {
			RTSTACK *n;

			n = (RTSTACK *) palloc(sizeof(RTSTACK));
			n->rts_parent = stack;
			n->rts_blk = blk;
			n->rts_child = choose(page, itup);
			stack = n;

			which = (IndexTuple) PageGetItem(page, n->rts_child);
			blk = ItemPointerGetBlock(&(which->t_tid));
		}
	} while (!(page->opaque & F_LEAF));

	if (nospace(page, itup)) {
		page = dosplit(r, buffer, stack, itup);
		ReleaseBuffer(buffer);
		freestack(stack);
		return (rtdoinsert(r, itup));
	}

	/* add the item and release the buffer */
	PageAddItem(page, itup, PSIZE(itup),
		    PageGetMaxOffsetIndex(page), LP_USED);
	ReleaseBuffer(buffer);

	pfree(itup);

	/* build and return an InsertIndexResult for this insertion */
	res = (InsertIndexResult) palloc(sizeof(InsertIndexResultData));
	ItemPointerSet(&(res->pointerData), 0, blk, 0, l);
	res->lock = (RuleLock) NULL;
	res->offset = (double) 0;

	return (res);
}

/*
 *  dosplit -- split a page in the tree.
 *
 *	This is the quadratic-cost split algorithm Guttman describes in
 *	his paper.  The reason we chose it is that you can implement this
 *	with less information about the data types on which you're operating.
 */

PageHeader
dosplit(r, buffer, stack, itup)
	Relation r;
	Buffer buffer;
	RTSTACK *stack;
	IndexTuple itup;
{
	PageHeader thispg;
	Buffer leftbuf, rightbuf;
	PageHeader left, right;
	ItemId itemid;
	IndexTuple item;
	OffsetNumber maxoff;
	OffsetIndex i;
	OffsetIndex leftoff, rightoff;
	SPLITVEC v;

	thispg = (PageHeader) BufferGetPage(buffer, 0);

	/*
	 *  The root of the tree is the first block in the relation.  If
	 *  we're about to split the root, we need to do some hocus-pocus
	 *  to enforce this guarantee.
	 */

	if (BufferGetBlock(buffer) == P_ROOT) {
		leftbuf = (Buffer) NULL;
		left = (PageHeader)
			  PageGetTempPage(thispg, sizeof(RTreePageOpaqueData));
	} else {
		leftbuf = ReadBuffer(r, P_NEW);
		left = (PageHeader) BufferGetPage(leftbuf, 0);
	}
	rightbuf = ReadBuffer(r, P_NEW);
	right = (PageHeader) BufferGetPage(rightbuf, 0);

	picksplit(r, thispg, &v);

	leftoff = rightoff = 0;
	maxoff = PageGetMaxOffsetIndex(thispg);
	for (i = 0; i < maxoff; i++) {
		itemid = PageGetItemId(thispg, i);
		item = (IndexTuple) PageGetItem(thispg, itemid);
		if (i == *(v.spl_left)) {
			(void) PageAddItem(left, item, /* XXX XXX SIZE ,*/
					   leftoff++, LP_USED);
			v.spl_left++;
		} else {
			(void) PageAddItem(right, item, /* XXX XXX SIZE ,*/
					   rightoff++, LP_USED);
			v.spl_right++;
		}
	}
	/* XXX here, we need to propogate the insertion up the tree */
}

picksplit(r, page, v)
	Relation r;
	PageHeader page;
	SPLITVEC *v;
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
	OffsetNumber seed_1, seed_2;
	OffsetNumber *left, *right;

	union_proc = index_getprocid(r, 1, RT_UNION_PROC);
	size_proc = index_getprocid(r, 1, RT_SIZE_PROC);
	inter_proc = index_getprocid(r, 1, RT_INTER_PROC);
	maxoff = PageGetMaxOffsetIndex(page);

	v->spl_left = (OffsetNumber *) palloc(maxoff * sizeof(OffsetNumber));
	v->spl_right = (OffsetNumber *) palloc(maxoff * sizeof(OffsetNumber));

	firsttime = true;
	waste = 0;

	for (i = 0; i < maxoff; i++) {
		item_1 = (IndexTuple) PageGetItem(page, PageGetItemId(page, i));
		datum_alpha = ((char *) item_1) + sizeof(IndexTuple);
		for (j = i + 1; j <= maxoff; j++) {
			item_2 = (IndexTuple)
				PageGetItem(page, PageGetItemId(page, j));
			datum_beta = ((char *) item_2) + sizeof(IndexTuple);

			/* compute the wasted space by unioning these guys */
			union_d = (char *) fmgr(union_proc,
						datum_alpha, datum_beta);
			size_union = (int) fmgr(size_proc, union_d);
			inter_d = (char *) fmgr(inter_proc,
						datum_alpha, datum_beta);
			size_inter = (int) fmgr(size_proc, inter_d);
			size_waste = size_union - size_inter;

			pfree(union_d);
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
	datum_alpha = ((char *) item_1) + sizeof(IndexTuple);
	datum_l = (char *) fmgr(union_proc, datum_alpha, datum_alpha);
	size_l = (int) fmgr(size_proc, datum_l);
	item_2 = (IndexTuple) PageGetItem(page, PageGetItemId(page, seed_2));
	datum_beta = ((char *) item_2) + sizeof(IndexTuple);
	datum_r = (char *) fmgr(union_proc, datum_beta, datum_beta);
	size_r = (int) fmgr(size_proc, datum_r);

	/*
	 *  Now split up the regions between the two seeds.  An important
	 *  property of this split algorithm is that the split vector v
	 *  has the indices of items to be split in order in its left and
	 *  right vectors.  We exploit this property by doing a merge in
	 *  the code that actually splits the page.
	 */

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
		item_1 = (IndexTuple) PageGetItem(page, PageGetItemId(page, i));
		datum_alpha = ((char *) item_1) + sizeof(IndexTuple);
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

/* ================================================================ */
char *
rtdelete()
{
	return (char *) NULL;
}

char *
rtgettuple()
{
	return (char *) NULL;
}

char *
rtbeginscan()
{
	return (char *) NULL;
}

void
rtendscan()
{
	;
}

void
rtmarkpos()
{
	;
}

void
rtrestrpos()
{
	;
}

void
rtrescan()
{
	;
}

RTInitBuffer() { ; }
choose() { ; }
ItemPointerGetBlock() { ; }
nospace() { ; }
freestack() { ; }
