/* ----------------------------------------------------------------
 *   FILE
 *	heapam.c
 *	
 *   DESCRIPTION
 *	heap access method code
 *
 *   INTERFACE ROUTINES
 *	heapgettup 	- fetch next heap tuple from a scan
 *	heap_open 	- open a heap relation by relationId
 *	heap_openr 	- open a heap relation by name
 *	heap_close 	- close a heap relation
 *	heap_beginscan	- begin relation scan
 *	heap_rescan	- restart a relation scan
 *	heap_endscan	- end relation scan
 *	heap_getnext	- retrieve next tuple in scan
 *	heap_fetch	- retrive tuple with tid
 *	heap_insert	- insert tuple into a relation
 *	heap_delete	- delete a tuple from a relation
 *	heap_replace	- replace a tuple in a relation with another tuple
 *	heap_markpos	- mark scan position
 *	heap_restrpos	- restore position to marked location
 *	
 *   NOTES
 *	This file contains the heap_ routines which implement
 *	the POSTGRES heap access method used for all POSTGRES
 *	relations.  
 *
 *  old comments:
 *	struct relscan hints:  (struct should be made AM independent?)
 *
 *	rs_ctid is the tid of the last tuple returned by getnext.
 *	rs_ptid and rs_ntid are the tids of the previous and next tuples
 *	returned by getnext, respectively.  NULL indicates an end of
 *	scan (either direction); NON indicates an unknow value.
 *
 *	possible combinations:
 *	rs_p	rs_c	rs_n		interpretation
 *	NULL	NULL	NULL		empty scan
 *	NULL	NULL	NON		at begining of scan
 *	NULL	NULL	t1		at begining of scan (with cached tid)
 *	NON	NULL	NULL		at end of scan
 *	t1	NULL	NULL		at end of scan (with cached tid)
 *	NULL	t1	NULL		just returned only tuple
 *	NULL	t1	NON		just returned first tuple
 *	NULL	t1	t2		returned first tuple (with cached tid)
 *	NON	t1	NULL		just returned last tuple
 *	t2	t1	NULL		returned last tuple (with cached tid)
 *	t1	t2	NON		in the middle of a forward scan
 *	NON	t2	t1		in the middle of a reverse scan
 *	ti	tj	tk		in the middle of a scan (w cached tid)
 *
 *	Here NULL is ...tup == NULL && ...buf == InvalidBuffer,
 *	and NON is ...tup == NULL && ...buf == UnknownBuffer.
 *
 *	Currently, the NONTID values are not cached with their actual
 *	values by getnext.  Values may be cached by markpos since it stores
 *	all three tids.
 *
 *	NOTE:  the calls to elog() must stop.  Should decide on an interface
 *	between the general and specific AM calls.
 *
 * 	XXX probably do not need a free tuple routine for heaps.
 * 	Huh?  Free tuple is not necessary for tuples returned by scans, but
 * 	is necessary for tuples which are returned by
 *	RelationGetTupleByItemPointer. -hirohama
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */


#include <sys/file.h>
#include <strings.h>

#include "tmp/postgres.h"

RcsId("$Header$");

#include "access/att.h"
#include "access/attnum.h"
#include "access/heapam.h"
#include "access/hio.h"
#include "access/hrnd.h"
#include "access/htup.h"
#include "access/relscan.h"
#include "access/skey.h"
#include "access/tqual.h"
#include "access/valid.h"
#include "access/xcxt.h"

#include "catalog/catname.h"
#include "rules/rac.h"
#include "rules/rlock.h"

#include "storage/buf.h"
#include "storage/bufmgr.h"
#include "storage/bufpage.h"
#include "storage/itemid.h"
#include "storage/itemptr.h"
#include "storage/page.h"

#include "tmp/miscadmin.h"

#include "utils/memutils.h"
#include "utils/fmgr.h"
#include "utils/inval.h"
#include "utils/lmgr.h"
#include "utils/log.h"
#include "utils/mcxt.h"
#include "utils/rel.h"
#include "utils/relcache.h"

/* If RANDOMINSERT is defined, random pages will be examined to see if there
 * is space for insertion.  If it is not defined, the new tuple is always
 * appended to the end.
 * plai 8/7/90
 */
#undef RANDOMINSERT

static bool	ImmediateInvalidation;
extern HeapAccessStatistics heap_access_stats;

/* ----------------------------------------------------------------
 *                       heap support routines
 * ----------------------------------------------------------------
 */

/* ----------------
 *	initsdesc - sdesc code common to heap_beginscan and heap_rescan
 * ----------------
 */
void
initsdesc(sdesc, relation, atend, nkeys, key)
    HeapScanDesc sdesc;
    Relation	 relation;
    int		 atend;
    unsigned	 nkeys;
    ScanKey	 key;
{
    if (!RelationGetNumberOfBlocks(relation)) {
	/* ----------------
	 *  relation is empty
	 * ----------------
	 */
	sdesc->rs_ntup = sdesc->rs_ctup = sdesc->rs_ptup = NULL;
	sdesc->rs_nbuf = sdesc->rs_cbuf = sdesc->rs_pbuf = InvalidBuffer;
    } else if (atend) {
	/* ----------------
	 *  reverse scan
	 * ----------------
	 */
	sdesc->rs_ntup = sdesc->rs_ctup = NULL;
	sdesc->rs_nbuf = sdesc->rs_cbuf = InvalidBuffer;
	sdesc->rs_ptup = NULL;
	sdesc->rs_pbuf = UnknownBuffer;
    } else {
	/* ----------------
	 *  forward scan
	 * ----------------
	 */
	sdesc->rs_ctup = sdesc->rs_ptup = NULL;
	sdesc->rs_cbuf = sdesc->rs_pbuf = InvalidBuffer;
	sdesc->rs_ntup = NULL;
	sdesc->rs_nbuf = UnknownBuffer;
    } /* invalid too */

    /* we don't have a marked position... */
    ItemPointerSetInvalid(&(sdesc->rs_mptid));
    ItemPointerSetInvalid(&(sdesc->rs_mctid));
    ItemPointerSetInvalid(&(sdesc->rs_mntid));
    ItemPointerSetInvalid(&(sdesc->rs_mcd));

    /* ----------------
     *	copy the scan key, if appropriate
     * ----------------
     */
    if (key != NULL)
	bcopy((char *)&key->data[0],
	      (char *)&sdesc->rs_key.data[0],
	      (int)nkeys * sizeof *key);
}

/* ----------------
 *	unpinsdesc - code common to heap_rescan and heap_endscan
 * ----------------
 */
void
unpinsdesc(sdesc)
    HeapScanDesc	sdesc;
{
    if (BufferIsValid(sdesc->rs_pbuf)) {
	BufferPut(sdesc->rs_pbuf, L_UNPIN);
    }
    if (BufferIsValid(sdesc->rs_cbuf)) {
	BufferPut(sdesc->rs_cbuf, L_UNPIN);
	}
    if (BufferIsValid(sdesc->rs_nbuf)) {
	BufferPut(sdesc->rs_nbuf, L_UNPIN);
    }
}
/* ----------------
 *	heapgettup - fetch next heap tuple
 *
 *	routine used by heap_getnext() which does most of the
 *	real work in scanning tuples.
 * ----------------
 */
static
HeapTuple
heapgettup(relation, tid, dir, b, timeQual, nkeys, key, pageskip, initskip)
    Relation		relation;
    ItemPointer		tid;
    int			dir;
    Buffer		*b;
    TimeQual		timeQual;
    uint32		nkeys;
    ScanKey		key;
    int			pageskip;
    int			initskip;
{
    ItemId		lpp;
    PageHeader		dp;
    int			page;
    int			lineoff;
    int			pages;
    int			lines;
    HeapTuple		rtup;
    OffsetIndex		offsetIndex;

    /* ----------------
     *	increment access statistics
     * ----------------
     */
    IncrHeapAccessStat(local_heapgettup);
    IncrHeapAccessStat(global_heapgettup);
    
    /* ----------------
     *	debugging stuff 
     *
     * check validity of arguments, here and for other functions too
     * Note: no locking manipulations needed--this is a local function
     * ----------------
     */
#ifdef	HEAPDEBUGALL
	if (ItemPointerIsValid(tid)) {
		elog(DEBUG, "heapgettup(%.16s, tid=0x%x[%d,%d], dir=%d, ...)",
			RelationGetRelationName(relation), tid, tid->blockData,
			tid->positionData, dir);
	} else {
		elog(DEBUG, "heapgettup(%.16s, tid=0x%x, dir=%d, ...)",
			RelationGetRelationName(relation), tid, dir);
	}
	elog(DEBUG, "heapgettup(..., b=0x%x, timeQ=0x%x, nkeys=%d, key=0x%x",
		b, timeQual, nkeys, key);
	if (timeQual == SelfTimeQual) {
		elog(DEBUG, "heapgettup: relation(%c)=`%.16s', SelfTimeQual",
			relation->rd_rel->relkind, &relation->rd_rel->relname);
	} else {
		elog(DEBUG, "heapgettup: relation(%c)=`%.16s', timeQual=%d",
			relation->rd_rel->relkind, &relation->rd_rel->relname,
			timeQual);
	}
#endif	/* !defined(HEAPDEBUGALL) */

    if (!ItemPointerIsValid(tid)) {
	Assert(!PointerIsValid(tid));
    }

    /* ----------------
     *	return null immediately if relation is empty
     * ----------------
     */
    if (!(pages = relation->rd_nblocks))
	return (NULL);

    /* ----------------
     *	calculate next starting lineoff, given scan direction
     * ----------------
     */
    if (!dir) {
	/* ----------------
	 * ``no movement'' scan direction
	 * ----------------
	 */
	/* assume it is a valid TID XXX	*/
	if (ItemPointerIsValid(tid) == false) {
	    *b = InvalidBuffer;
	    return (NULL);
	}
	*b = RelationGetBufferWithBuffer(relation,
			                 ItemPointerGetBlockNumber(tid),
			                 *b);
	
#ifndef NO_BUFFERISVALID
	if (!BufferIsValid(*b)) {
	    elog(WARN, "heapgettup: failed RelationGetBuffer");
	}
#endif
	
	dp = (PageHeader) BufferSimpleGetPage(*b);
	offsetIndex = ItemPointerSimpleGetOffsetIndex(tid);
	lpp = PageGetItemId(dp, offsetIndex);

	Assert(!ItemIdIsLock(lpp));

	rtup = (HeapTuple)PageGetItem(dp, lpp);
	if (BufferPut(*b, L_PIN) < 0)
	    elog(WARN, "heapgettup: failed BufferPut");
	return (rtup);
	
    } else if (dir < 0) {
	/* ----------------
	 *  reverse scan direction
	 * ----------------
	 */
	if (ItemPointerIsValid(tid) == false) {
	    tid = NULL;
	}
	page = (tid == NULL) ?
	    pages - 1 - initskip : ItemPointerGetBlockNumber(tid);

	if (page < 0) {	
	    *b = InvalidBuffer;
	    return (NULL);
	}

	*b = RelationGetBufferWithBuffer(relation, page, *b);
#ifndef NO_BUFFERISVALID
	if (!BufferIsValid(*b)) {
	    elog(WARN, "heapgettup: failed RelationGetBuffer");
	}
#endif
	
	dp = (PageHeader) BufferSimpleGetPage(*b);
	lines = 1 + PageGetMaxOffsetIndex(dp);
	lineoff = (tid == NULL) ?
	    lines - 1 : ItemPointerSimpleGetOffsetIndex(tid) - 1;
	/* page and lineoff now reference the physically previous tid */

    } else {
	/* ----------------
	 *  forward scan direction
	 * ----------------
	 */
	if (ItemPointerIsValid(tid) == false) {
	    page = initskip;
	    lineoff = 0;
	} else {
	    page = ItemPointerGetBlockNumber(tid);
	    lineoff = 1 + ItemPointerSimpleGetOffsetIndex(tid);
	}

	if (page >= pages) {
	    *b = InvalidBuffer;
	    return (NULL);
	}

	/* page and lineoff now reference the physically next tid */
	*b = RelationGetBufferWithBuffer(relation, page, *b);
	
#ifndef NO_BUFFERISVALID
	if (!BufferIsValid(*b)) {
	    elog(WARN, "heapgettup: failed RelationGetBuffer");
	}
#endif
	
	dp = (PageHeader) BufferSimpleGetPage(*b);
	lines = 1 + PageGetMaxOffsetIndex(dp);
    }

    /* ----------------
     *	calculate line pointer and number of remaining items
     *  to check on this page.
     *
     *  hack hack hack:
     *  lineoff becomes the number of additional lines left to check
     * ----------------
     */
    lpp = PageGetItemId(dp, lineoff);
    lineoff = (dir < 0) ? lineoff : lines - lineoff - 1;

    /* ----------------
     *	advance the scan until we find a qualificating tuple or
     *  run out of stuff to scan
     * ----------------
     */
    for (;;) {
	while (lineoff >= 0) {
	    /* ----------------
	     *	if current tuple qualifies, return it.
	     * ----------------
	     */
	    if (heap_satisifies(lpp, *b, timeQual, nkeys, key)) {
		rtup = (HeapTuple) PageGetItem(dp, lpp);
		if (BufferPut(*b, L_PIN) < 0)
		    elog(WARN, "heap_fetch: failed BufferPut");

		return (rtup);
	    }

	    /* ----------------
	     *	otherwise move to the next item on the page
	     * ----------------
	     */
	    lineoff--;
	    (dir < 0) ? lpp-- : lpp++;
	}

	/* ----------------
	 *  if we get here, it means we've exhausted the items on
	 *  this page and it's time to move to the next..
	 * ----------------
	 */
	if (BufferPut(*b, L_UN | L_SH) < 0) {
	    elog(WARN, "heapgettup: failed BufferPut");
	}	

	if (dir < 0)
	    page -= pageskip;
	else
	    page += pageskip;

	/* ----------------
	 *  return NULL if we've exhausted all the pages..
	 * ----------------
	 */
	if (page < 0 || page >= pages) {
	    if (BufferIsValid(*b))
		ReleaseBuffer(*b);
	    *b = InvalidBuffer;
	    return (NULL);
	}

	*b = ReleaseAndReadBuffer(*b, relation, page);
	
#ifndef NO_BUFFERISVALID
	if (!BufferIsValid(*b)) {
	    elog(WARN, "heapgettup: failed RelationGetBuffer");
	}
#endif
	dp = (PageHeader) BufferSimpleGetPage(*b);
	lines = 1 + PageGetMaxOffsetIndex(dp);
	lineoff = lines - 1;
	if (dir < 0) {
	    lpp = PageGetItemId(dp, lineoff);
	} else if (dir > 0) {
	    lpp = PageGetFirstItemId(dp);
	}
    }
}

/* ----------------
 *	doinsert
 *
 *	routine used by heap_insert() which in turn calls
 *	RelationPutHeapTuple() or RelationPutLongHeapTuple().
 * ----------------
 */
RuleLock
doinsert(relation, tup)
    Relation	relation;
    HeapTuple	tup;
{
    Buffer		b;
    PageHeader		dp;
    int			pages;
    BlockIndexList	list;
    Index		index;
    BlockNumber		blockIndex;

    /* ----------------
     *	??? -cim
     * ----------------
     */
    ItemPointerSetInvalid(&tup->t_chain);

    /* ----------------
     *	??? -cim
     * ----------------
     */
    if (tup->t_len > MAXTUPLEN ||
	!(pages = RelationGetNumberOfBlocks(relation))) {
	RelationPutLongHeapTuple(relation, tup);	/* ??? */
	return ((RuleLock)NULL);
    }

    /* ----------------
     *	compute proper location if cluster--passed as argument?
     * ----------------
     */
    blockIndex = pages - 1;
	
#ifdef	DOCLUSTER
    blockIndex = getclusterblockindex();
#endif  DOCLUSTER

    /* ----------------
     *	??? -cim
     * ----------------
     */
    if (BlockNumberIsValid(blockIndex)) {
	if (RelationContainsUsableBlock(relation,
					blockIndex,
					tup->t_len,
					ClusteredNumberOfFailures)) {
		    
#ifdef	RANDOMDEBUG
	    elog(DEBUG, "clustered@%d", blockIndex);
#endif	RANDOMDEBUG
	} else {
	    blockIndex = InvalidBlockNumber;
	}
    }

    /* ----------------
     *	??? -cim
     * ----------------
     */
#ifdef	DOCLUSTER
    if (!BlockNumberIsValid(blockIndex)) {
	list = RelationGetRandomBlockIndexList(relation, tup->t_oid);
	for (index = 0; BlockNumberIsValid(list[index]); index += 1) {
	    if (RelationContainsUsableBlock(relation,
					    list[index],
					    tup->t_len,
					    index)) {
		blockIndex = list[index];
		
#ifdef	RANDOMDEBUG
		elog(DEBUG, "append@%d", blockIndex);
#endif	RANDOMDEBUG
		break;
	    }
	}
    }
#endif

    /* ----------------
     *	??? -cim
     * ----------------
     */
    
    if (BlockNumberIsValid(blockIndex)) {
	/* ----------------
	 *	we've finally found a block with space
	 *	for our tuple, so insert it there.
	 * ----------------
	 */
	RelationPutHeapTuple(relation, blockIndex, tup);
	setclusterblockindex(blockIndex);
    }
#ifdef	DOCLUSTER
    else if (BlockNumberIsValid(list[0])) {
	/* ----------------
	 *	??? -cim
	 * ----------------
	 */
	RelationPutLongHeapTuple(relation, tup);
	setclusterblockindex(pages);	/* XXX PutLong assumption */
    }
#endif
    else {
	/* ----------------
	 *	??? -cim
	 * ----------------
	 */
	b = RelationGetBuffer(relation, pages - 1, L_UP);
#ifndef NO_BUFFERISVALID
	if (!BufferIsValid(b)) {
	    /* XXX L_SH better ??? */
	    elog(WARN, "aminsert: failed RelationGetBuffer");
	}
#endif
	
	dp = (PageHeader)BufferSimpleGetPage(b);
	if ((int)tup->t_len > PageGetFreeSpace(dp)) {
	    if (BufferPut(b, L_UN | L_UP) < 0)
		elog(WARN, "aminsert: failed BufferPut");
	    /* XXX there is a window in which the status can change */
	    RelationPutLongHeapTuple(relation, tup);
	} else {
	    if (BufferPut(b, L_UN | L_UP) < 0)
		elog(WARN, "aminsert: failed BufferPut");
	    /* XXX there is a window in which the status can change */
	    RelationPutHeapTuple(relation, pages - 1, tup);
	}
    }
    return ((RuleLock)NULL);
}

/* 
 *	HeapScanIsValid is now a macro in relscan.h -cim 4/27/91
 */

/* ----------------
 *	SetHeapAccessMethodImmediateInvalidation
 * ----------------
 */
void
SetHeapAccessMethodImmediateInvalidation(on)
    bool	on;
{
    ImmediateInvalidation = on;
}

/* ----------------------------------------------------------------
 *                   heap access method interface
 * ----------------------------------------------------------------
 */
/* ----------------
 *	heap_open - open a heap relation by relationId
 *
 *	presently the relcache routines do all the work we need
 *	to open/close heap relations.
 * ----------------
 */
Relation
heap_open(relationId)
    ObjectId	relationId;
{
    /* ----------------
     *	increment access statistics
     * ----------------
     */
    IncrHeapAccessStat(local_open);
    IncrHeapAccessStat(global_open);
    
    return (Relation)
	RelationIdGetRelation(relationId);
}

/* ----------------
 *	heap_openr - open a heap relation by name
 *
 *	presently the relcache routines do all the work we need
 *	to open/close heap relations.
 * ----------------
 */
Relation
heap_openr(relationName)
    Name relationName;
{
    /* ----------------
     *	increment access statistics
     * ----------------
     */
    IncrHeapAccessStat(local_openr);
    IncrHeapAccessStat(global_openr);
    
    return (Relation)
	RelationNameGetRelation(relationName);
}

/* ----------------
 *	heap_close - close a heap relation
 *
 *	presently the relcache routines do all the work we need
 *	to open/close heap relations.
 * ----------------
 */
void
heap_close(relation)
    Relation relation;
{
    /* ----------------
     *	increment access statistics
     * ----------------
     */
    IncrHeapAccessStat(local_close);
    IncrHeapAccessStat(global_close);
    
    (void) RelationClose(relation);
}

    
/* ----------------
 *	heap_beginscan	- begin relation scan
 * ----------------
 */

HeapScanDesc
heap_beginscan(relation, atend, timeQual, nkeys, key)
    Relation	relation;
    int		atend;
    TimeQual	timeQual;
    unsigned	nkeys;
    ScanKey		key;
{
    HeapScanDesc	sdesc;

    /* ----------------
     *	increment access statistics
     * ----------------
     */
    IncrHeapAccessStat(local_beginscan);
    IncrHeapAccessStat(global_beginscan);

    /* ----------------
     *	sanity checks
     * ----------------
     */
    if (RelationIsValid(relation) == false)
	elog(WARN, "heap_beginscan: !RelationIsValid(relation)");
	
    /* ----------------
     * set relation level read lock
     * ----------------
     */
    RelationSetLockForRead(relation);

    /* XXX someday assert SelfTimeQual if relkind == 'u' */
    if (relation->rd_rel->relkind == 'u') {
	timeQual = SelfTimeQual;
    }

    /* ----------------
     *  increment relation ref count while scanning relation
     * ----------------
     */
    RelationIncrementReferenceCount(relation);
    
    /* ----------------
     *	allocate and initialize scan descriptor
     * ----------------
     */
    sdesc = (HeapScanDesc)
	palloc(sizeof *sdesc + (nkeys - 1) * sizeof key->data); /* XXX */

    relation->rd_nblocks = FileGetNumberOfBlocks(relation->rd_fd);
    sdesc->rs_rd = relation;

    initsdesc(sdesc, relation, atend, nkeys, key);
    
    sdesc->rs_atend = atend;
    sdesc->rs_tr = timeQual;
    sdesc->rs_nkeys = (short)nkeys;

    sdesc->pageskip = 1;
    sdesc->initskip = 0;
    
    return (sdesc);
}

/* ----------------
 *	heap_rescan	- restart a relation scan
 * ----------------
 */
void
heap_rescan(sdesc, scanFromEnd, key)
    HeapScanDesc	sdesc;
    bool		scanFromEnd;
    ScanKey		key;
{
    /* ----------------
     *	increment access statistics
     * ----------------
     */
    IncrHeapAccessStat(local_rescan);
    IncrHeapAccessStat(global_rescan);
    
    /* Note: set relation level read lock is still set */

    /* ----------------
     *	unpin scan buffers
     * ----------------
     */
    unpinsdesc(sdesc);
    
    /* ----------------
     *	reinitialize scan descriptor
     * ----------------
     */
    initsdesc(sdesc, sdesc->rs_rd, scanFromEnd, sdesc->rs_nkeys, key);
    sdesc->rs_atend = (Boolean) scanFromEnd;
}

/* ----------------
 *	heap_endscan	- end relation scan
 *
 *	See how to integrate with index scans.
 *	Check handling if reldesc caching.
 * ----------------
 */
void
heap_endscan(sdesc)
    HeapScanDesc	sdesc;
{
    /* ----------------
     *	increment access statistics
     * ----------------
     */
    IncrHeapAccessStat(local_endscan);    
    IncrHeapAccessStat(global_endscan);    
	
    /* Note: no locking manipulations needed */

    /* ----------------
     *	unpin scan buffers
     * ----------------
     */
    unpinsdesc(sdesc);
    
    /* ----------------
     *	decrement relation reference count and free scan descriptor storage
     * ----------------
     */
    RelationDecrementReferenceCount(sdesc->rs_rd);
    pfree((char *)sdesc);	/* XXX */
}

/* ----------------
 *	heap_getnext	- retrieve next tuple in scan
 *
 *	Fix to work with index relations.
 * ----------------
 */

#ifdef HEAPDEBUGALL
#define HEAPDEBUG_1 \
   elog(DEBUG, "heap_getnext([%.16s,nkeys=%d],backw=%d,0x%x) called", \
	&sdesc->rs_rd->rd_rel->relname, sdesc->rs_nkeys, backw, b)
 
#define HEAPDEBUG_2 \
   elog(DEBUG, "heap_getnext called with backw (no tracing yet)")

#define HEAPDEBUG_3 \
   elog(DEBUG, "heap_getnext returns NULL at end")

#define HEAPDEBUG_4 \
   elog(DEBUG, "heap_getnext valid buffer UNPIN'd")

#define HEAPDEBUG_5 \
   elog(DEBUG, "heap_getnext next tuple was cached")

#define HEAPDEBUG_6 \
   elog(DEBUG, "heap_getnext returning EOS")

#define HEAPDEBUG_7 \
   elog(DEBUG, "heap_getnext returning tuple");
#else
#define HEAPDEBUG_1
#define HEAPDEBUG_2
#define HEAPDEBUG_3
#define HEAPDEBUG_4
#define HEAPDEBUG_5
#define HEAPDEBUG_6
#define HEAPDEBUG_7
#endif	/* !defined(HEAPDEBUGALL) */


HeapTuple
heap_getnext(scandesc, backw, b)
    HeapScanDesc	scandesc;
    int			backw;
    Buffer		*b;
{
    register HeapScanDesc sdesc = scandesc;
    Buffer		  localb;
    int			  pageskip;
    int			  initskip;

    pageskip = scandesc->pageskip;
    initskip = scandesc->initskip;
    
    /* ----------------
     *	increment access statistics
     * ----------------
     */
    IncrHeapAccessStat(local_getnext);    
    IncrHeapAccessStat(global_getnext);    

    /* Note: no locking manipulations needed */

    /* ----------------
     *	argument checks
     * ----------------
     */
    if (sdesc == NULL)
	elog(WARN, "heap_getnext: NULL relscan");

    /* ----------------
     *	initialize return buffer to InvalidBuffer
     * ----------------
     */
    if (! PointerIsValid(b)) b = &localb;
    (*b) = InvalidBuffer;
    
    HEAPDEBUG_1; /* heap_getnext( info ) */
    
    if (backw) {
	/* ----------------
	 *  handle reverse scan
	 * ----------------
	 */
	HEAPDEBUG_2; /* heap_getnext called with backw */
	
	if (sdesc->rs_ptup == sdesc->rs_ctup &&
	    BufferIsInvalid(sdesc->rs_pbuf))
	    {
		return (NULL);
	    }

	/*
	if (BufferIsValid(sdesc->rs_nbuf))
	    BufferPut(sdesc->rs_nbuf, L_UNPIN);
	*/

	sdesc->rs_ntup = sdesc->rs_ctup;
	sdesc->rs_nbuf = sdesc->rs_cbuf;

	if (sdesc->rs_ptup != NULL) {
	    sdesc->rs_ctup = sdesc->rs_ptup;
	    sdesc->rs_cbuf = sdesc->rs_pbuf;
	} else { /* NONTUP */
	    ItemPointer iptr;

	    iptr = (sdesc->rs_ctup != NULL) ?
		&(sdesc->rs_ctup->t_ctid) : (ItemPointer) NULL;
	    
	    sdesc->rs_ctup = (HeapTuple)
		heapgettup(sdesc->rs_rd,
			   iptr,
			   -1,
			   &(sdesc->rs_cbuf),
			   sdesc->rs_tr,
			   sdesc->rs_nkeys,
			   &(sdesc->rs_key),
			   pageskip,
			   initskip);
	}

	if (sdesc->rs_ctup == NULL &&
	    ! BufferIsValid(sdesc->rs_cbuf))
	    {
		sdesc->rs_ptup = NULL;
		sdesc->rs_pbuf = InvalidBuffer;
		return (NULL);
	    }

	sdesc->rs_ptup = NULL;
	sdesc->rs_pbuf = UnknownBuffer;

    } else {
	/* ----------------
	 *  handle forward scan
	 * ----------------
	 */
	if (sdesc->rs_ctup == sdesc->rs_ntup &&
	    BufferIsInvalid(sdesc->rs_nbuf)) {
	    HEAPDEBUG_3; /* heap_getnext returns NULL at end */
	    return (NULL);
	}

	/*
	if (BufferIsValid(sdesc->rs_pbuf)) {
	    HEAPDEBUG_4;
	    BufferPut(sdesc->rs_pbuf, L_UNPIN);
	}
	*/

	sdesc->rs_ptup = sdesc->rs_ctup;
	sdesc->rs_pbuf = sdesc->rs_cbuf;
	
	if (sdesc->rs_ntup != NULL) {
	    sdesc->rs_ctup = sdesc->rs_ntup;
	    sdesc->rs_cbuf = sdesc->rs_nbuf;
	    HEAPDEBUG_5; /* heap_getnext next tuple was cached */
	} else { /* NONTUP */
	    ItemPointer iptr;

	    iptr = (sdesc->rs_ctup != NULL) ?
		&sdesc->rs_ctup->t_ctid : (ItemPointer) NULL;
	    
	    sdesc->rs_ctup = (HeapTuple)
		heapgettup(sdesc->rs_rd,
			   iptr,
			   1,
			   &sdesc->rs_cbuf,
			   sdesc->rs_tr,
			   sdesc->rs_nkeys,
			   &sdesc->rs_key,
			   pageskip,
			   initskip);
	}

	if (sdesc->rs_ctup == NULL &&
	    !BufferIsValid(sdesc->rs_cbuf)) {
	    sdesc->rs_ntup = NULL;
	    sdesc->rs_nbuf = InvalidBuffer;
	    HEAPDEBUG_6; /* heap_getnext returning EOS */
	    return (NULL);
	}
	
	sdesc->rs_ntup = NULL;
	sdesc->rs_nbuf = UnknownBuffer;
    }

    /* ----------------
     *	if we get here it means we have a new current scan tuple, so
     *  point to the proper return buffer and return the tuple.
     * ----------------
     */
    (*b) = sdesc->rs_cbuf;
    
    HEAPDEBUG_7; /* heap_getnext returning tuple */
    
    return (sdesc->rs_ctup);
}

/* ----------------
 *	heap_fetch	- retrive tuple with tid
 *
 *	Currently ignores LP_IVALID during processing!
 * ----------------
 */
HeapTuple
heap_fetch(relation, timeQual, tid, b)
    Relation	relation;
    TimeQual	timeQual;
    ItemPointer	tid;
    Buffer	*b;
{
    ItemId	lp;
    Buffer	buffer;
    PageHeader	dp;
    HeapTuple	tuple;
    OffsetIndex	offsetIndex;
    HeapTuple	heap_copytuple();

    /* ----------------
     *	increment access statistics
     * ----------------
     */
    IncrHeapAccessStat(local_fetch);    
    IncrHeapAccessStat(global_fetch);    

    /* ----------------
     *	sanity checks
     * ----------------
     */
    Assert(RelationIsValid(relation));
    Assert(relation->rd_rel->relkind == 'r');

    /* Note: set tuple level read lock */
    /* Pray that this cannot cause a deadlock */
    RelationSetLockForTupleRead(relation, tid);

    /* ----------------
     *	get the buffer from the relation descriptor
     * ----------------
     */
    buffer = RelationGetBuffer(relation,
			       ItemPointerGetBlockNumber(tid),
			       L_SH);
    
#ifndef NO_BUFFERISVALID
    if (!BufferIsValid(buffer)) {
	elog(WARN, "heap_fetch: %s relation: RelationGetBuffer(%lx) failed",
	     &relation->rd_rel->relname, (long)tid);
    }
#endif

    /* ----------------
     *	get the item line pointer corresponding to the requested tid
     * ----------------
     */
    dp = (PageHeader) BufferSimpleGetPage(buffer);
    offsetIndex = ItemPointerSimpleGetOffsetIndex(tid);
    lp = PageGetItemId(dp, offsetIndex);

    /* ----------------
     *	more sanity checks
     * ----------------
     */
    if (!ItemIdIsUsed(lp)) {
	elog(WARN, "heap_fetch: %s relation: unused tid (%ld,%d)",
	     &relation->rd_rel->relname,
	     ItemPointerGetBlockNumber(tid),
	     ItemPointerSimpleGetOffsetNumber(tid));
    }

    if (ItemIdIsContinuation(lp) || ItemIdIsInternal(lp)) {
	elog(WARN, "heap_fetch: %s relation--bad item pointer",
	     &relation->rd_rel->relname);
    }

    /* ----------------
     *	check time qualification of tid
     * ----------------
     */
    if (! heap_satisifies(lp, buffer, timeQual, 0,(ScanKey)NULL)) {
	if (BufferPut(buffer, L_UNLOCK) < 0)
	    elog(WARN, "heap_fetch: BufferPut failed");
	
	return (NULL);
    }

    /* ----------------
     *	all checks passed, now either return a copy of the tuple
     *  or pin the buffer page and return a pointer, depending on
     *  whether caller gave us a valid b.
     * ----------------
     */
    tuple = (HeapTuple) PageGetItem(dp, lp);
    if (BufferPut(buffer, L_PIN) < 0)
	elog(WARN, "heap_fetch: BufferPut failed");

    if (PointerIsValid(b)) {
	*b = buffer;
    } else {
	tuple = heap_copytuple(tuple, buffer, relation);
	if (BufferPut(buffer, L_UNPIN) < 0) {
	    elog(WARN, "heap_fetch: BufferPut failed");
	}
    }
    return (tuple);

    /* Note: lots of commented code was removed from here. */
}

/* ----------------
 *	heap_insert	- insert tuple
 *
 *	The assignment of t_min (and thus the others) should be
 *	removed eventually.
 *
 *	Currently places the tuple onto the last page.  If there is no room,
 *	it is placed on new pages.  (Heap relations)
 *	Note that concurrent inserts during a scan will probably have
 *	unexpected results, though this will be fixed eventually.
 *
 *	Fix to work with indexes.
 * ----------------
 */

RuleLock
heap_insert(relation, tup, off)
    Relation	relation;
    HeapTuple	tup;
    double	*off;
{
    /* ----------------
     *	increment access statistics
     * ----------------
     */
    IncrHeapAccessStat(local_insert);
    IncrHeapAccessStat(global_insert);

    /* ----------------
     *	set relation level read lock
     * ----------------
     */
    RelationSetLockForWrite(relation);

    if (off != NULL)
	*off = -1.0;			/* XXX ignore off for now */

    /* ----------------
     *  If the object id of this tuple has already been assigned, trust
     *  the caller.  There are a couple of ways this can happen.  At initial
     *  db creation, the backend program sets oids for tuples.  When we
     *  define an index, we set the oid.  Finally, in the future, we may
     *  allow users to set their own object ids in order to support a
     *  persistent object store (objects need to contain pointers to one
     *  another).
     * ----------------
     */
    if (!ObjectIdIsValid(tup->t_oid)) {
	tup->t_oid = newoid();
	LastOidProcessed = tup->t_oid;
    }

    TransactionIdStore(GetCurrentTransactionId(), (Pointer)tup->t_xmin);
    tup->t_cmin = GetCurrentCommandId();
    PointerStoreInvalidTransactionId((Pointer)tup->t_xmax);
    tup->t_tmin = InvalidTime;
    tup->t_tmax = InvalidTime;

    return(doinsert(relation, tup));
}

/* ----------------
 *	heap_delete	- delete a tuple
 *
 *	Must decide how to handle errors.
 * ----------------
 */

RuleLock
heap_delete(relation, tid)
    Relation	relation;
    ItemPointer	tid;
{
    ItemId		lp;
    HeapTuple		tp;
    PageHeader		dp;
    Buffer		b;
    long		time();
    char		*fmgr();

    /* ----------------
     *	increment access statistics
     * ----------------
     */
    IncrHeapAccessStat(local_delete);
    IncrHeapAccessStat(global_delete);

    /* ----------------
     *	sanity check
     * ----------------
     */
    Assert(ItemPointerIsValid(tid));

    /* ----------------
     *	set relation level write lock
     * ----------------
     */
    RelationSetLockForWrite(relation);

    b = RelationGetBuffer(relation, ItemPointerGetBlockNumber(tid), L_UP);
    
#ifndef NO_BUFFERISVALID
    if (!BufferIsValid(b)) { /* XXX L_SH better ??? */
	elog(WARN, "heap_delete: failed RelationGetBuffer");
    }
#endif NO_BUFFERISVALID

    dp = (PageHeader) BufferSimpleGetPage(b);
    lp = PageGetItemId(dp, ItemPointerSimpleGetOffsetIndex(tid));

    /* ----------------
     *	check that we're deleteing a valid item
     * ----------------
     */
    if (! heap_satisifies(lp, b, NowTimeQual, 0, (ScanKey) NULL)) {
	
	/* XXX call something else */
	if (BufferPut(b, L_UN | L_UP) < 0)
	    elog(WARN, "heap_delete: failed BufferPut");
	
	elog(WARN, "heap_delete: (am)invalid tid");
    }

    /* ----------------
     *	get the tuple and lock tell the buffer manager we want
     *  exclusive access to the page
     * ----------------
     */
    tp = (HeapTuple) PageGetItem(dp, lp);
    /* XXX order problems if not atomic assignment ??? */
    if (BufferPut(b, L_EX) < 0)
	elog(WARN, "heap_delete: failed BufferPut(L_EX)");

    /* ----------------
     *	store transaction information of xact deleting the tuple
     * ----------------
     */
    TransactionIdStore(GetCurrentTransactionId(), (Pointer)tp->t_xmax);
    tp->t_cmax = GetCurrentCommandId();
    /* tp->t_tmax = InvalidTime; */
    ItemPointerSetInvalid(&tp->t_chain);

    /* ----------------
     *	invalidate caches
     * ----------------
     */
    SetRefreshWhenInvalidate(ImmediateInvalidation);
    RelationInvalidateHeapTuple(relation, tp);
    SetRefreshWhenInvalidate((bool)!ImmediateInvalidation);

    if (BufferPut(b, L_UN | L_EX | L_WRITE) < 0) {
	elog(WARN, "heap_delete: failed BufferPut(L_UN | L_WRITE)");
    }
}

/* ----------------
 *	heap_replace	- replace a tuple
 *
 *	Must decide how to handle errors.
 *
 *	Fix arguments, work with indexes.
 * ----------------
 */

RuleLock
heap_replace(relation, otid, tup)
    Relation	relation;
    ItemPointer	otid;
    HeapTuple	tup;
{
    ItemId		lp;
    HeapTuple		tp;
    PageHeader		dp;
    Buffer		buffer;
    BlockIndexList	list;
    BlockNumber		blockIndex;
    Index		index;
    long		time();		/* know C */

    /* ----------------
     *	increment access statistics
     * ----------------
     */
    IncrHeapAccessStat(local_replace);
    IncrHeapAccessStat(global_replace);

    /* ----------------
     *	sanity checks
     * ----------------
     */
    Assert(ItemPointerIsValid(otid));

    /* ----------------
     *	set relation level write lock
     * ----------------
     */
    RelationSetLockForWrite(relation);

    buffer = RelationGetBuffer(relation,
			       ItemPointerGetBlockNumber(otid),
			       L_UP);
#ifndef NO_BUFFERISVALID
    if (!BufferIsValid(buffer)) {
	/* XXX L_SH better ??? */
	elog(WARN, "amreplace: failed RelationGetBuffer");
    }	
#endif NO_BUFFERISVALID
    
    dp = (PageHeader) BufferSimpleGetPage(buffer);
    lp = PageGetItemId(dp, ItemPointerSimpleGetOffsetIndex(otid));

    /* ----------------
     *	check that we're replacing a valid item
     * ----------------
     */
    if (! heap_satisifies(lp, buffer, NowTimeQual, 0, (ScanKey) NULL)) {
	
	/* XXX call something else */
	if (BufferPut(buffer, L_UN | L_UP) < 0)
	    elog(WARN, "heap_replace: failed BufferPut");
	
	elog(WARN, "heap_replace: (am)invalid otid");
    }

    /* ----------------
     *	logically delete old item
     * ----------------
     */
    tp = (HeapTuple) PageGetItem(dp, lp);
    /* -----------------
     *  the following test should be able to catch all non-functional
     *  update attempts and shut out all ghost tuples.
     *  XXX In the future, Spyros may need to update the rule lock on a tuple
     *  more than once within the same command and same transaction.
     *  He will have to introduce a new flag to override the following check.
     *  -- Wei
     * -----------------
     */
    if (TupleUpdatedByCurXactAndCmd(tp)) {
	elog(NOTICE, "Non-functional update, only first update is performed");
        return (RuleLock)NULL;
    }
    /* XXX order problems if not atomic assignment ??? */
    tup->t_oid = tp->t_oid;
    TransactionIdStore(GetCurrentTransactionId(), (Pointer)tup->t_xmin);
    tup->t_cmin = GetCurrentCommandId();
    PointerStoreInvalidTransactionId((Pointer)tup->t_xmax);
    tup->t_tmin = InvalidTime;
    tup->t_tmax = InvalidTime;
    ItemPointerSetInvalid(&tup->t_chain);

    /* ----------------
     *	insert new item
     * ----------------
     */
    if ((int)tup->t_len <= PageGetFreeSpace(dp)) {
	RelationPutHeapTuple(relation, BufferGetBlockNumber(buffer), tup);
    } else {
	/* ----------------
	 *  new item won't fit on same page as old item, have to look
	 *  for a new place to put it.
	 * ----------------
	 */
	
#ifdef RANDOMINSERT
	list = RelationGetRandomBlockIndexList(relation, tup->t_oid);

	blockIndex = InvalidBlockNumber;
	for (index = 0; BlockNumberIsValid(list[index]); index += 1) {
	    if (RelationContainsUsableBlock(relation,
					    list[index],
					    tup->t_len,
					    index)) {
		blockIndex = list[index];
#ifdef	RANDOMDEBUG
		elog(DEBUG, "replace@%d", blockIndex);
#endif	/* defined(RANDOMDEBUG) */
		break;
	    }
	}

	if (BlockNumberIsValid(blockIndex)) {
	    RelationPutHeapTuple(relation, blockIndex, tup);

	    /* should check if last block is usable   plai 8/9/90 */
	} else if (blockIndex = RelationGetNumberOfBlocks(relation) - 1,
		   RelationContainsUsableBlock(relation,
					       blockIndex,
					       tup->t_len,
					       index)) {
	    RelationPutHeapTuple(relation, blockIndex, tup);
	} else {
	    RelationPutLongHeapTuple(relation, tup);
	}
#else   /* RANDOMINSERT */

	/* should check if last block is usable   plai 8/9/90 */
	if (blockIndex = RelationGetNumberOfBlocks(relation) - 1,
	    RelationContainsUsableBlock(relation, blockIndex, tup->t_len, 0)) {
	    RelationPutHeapTuple(relation, blockIndex, tup);
	} else {
	    RelationPutLongHeapTuple(relation, tup);
	}
#endif  /* RANDOMINSERT */
    }

    /* ----------------
     *	new item in place, now record transaction information
     * ----------------
     */
    if (BufferPut(buffer, L_EX) < 0) {
	elog(WARN, "amreplace: failed BufferPut(L_EX)");
    }
    TransactionIdStore(GetCurrentTransactionId(), (Pointer)tp->t_xmax);
    tp->t_cmax = GetCurrentCommandId();
    /* tp->t_tmax = InvalidTime; */
    tp->t_chain = tup->t_ctid;

    /* ----------------
     *	invalidate caches
     * ----------------
     */
    SetRefreshWhenInvalidate(ImmediateInvalidation);
    RelationInvalidateHeapTuple(relation, tp);
    SetRefreshWhenInvalidate((bool)!ImmediateInvalidation);

    if (BufferPut(buffer, L_UN | L_EX | L_WRITE) < 0) {
	elog(WARN, "heap_replace: failed BufferPut(L_UN | L_WRITE)");
    }

    return ((RuleLock)NULL);	/* XXX */
}

/* ----------------
 *	heap_markpos	- mark scan position
 *
 *	Note:
 *		Should only one mark be maintained per scan at one time.
 *	Check if this can be done generally--say calls to get the
 *	next/previous tuple and NEVER pass struct scandesc to the
 *	user AM's.  Now, the mark is sent to the executor for safekeeping.
 *	Probably can store this info into a GENERAL scan structure.
 *
 *	May be best to change this call to store the marked position
 *	(up to 2?) in the scan structure itself.
 *	Fix to use the proper caching structure.
 * ----------------
 */
void
heap_markpos(sdesc)
    HeapScanDesc	sdesc;
{
    int pageskip, initskip;

    pageskip = sdesc->pageskip;
    initskip = sdesc->initskip;

    /* ----------------
     *	increment access statistics
     * ----------------
     */
    IncrHeapAccessStat(local_markpos);
    IncrHeapAccessStat(global_markpos);
	    
    /* Note: no locking manipulations needed */

    if (sdesc->rs_ptup == NULL &&
	BufferIsUnknown(sdesc->rs_pbuf)) { /* == NONTUP */
	sdesc->rs_ptup = (HeapTuple)
	    heapgettup(sdesc->rs_rd,
		       (sdesc->rs_ctup == NULL) ?
		         (ItemPointer)NULL : &sdesc->rs_ctup->t_ctid,
		       -1,
		       &sdesc->rs_pbuf,
		       sdesc->rs_tr,
		       sdesc->rs_nkeys,
		       &sdesc->rs_key,
		       pageskip,
		       initskip);
	
    } else if (sdesc->rs_ntup == NULL &&
	       BufferIsUnknown(sdesc->rs_nbuf)) { /* == NONTUP */
	sdesc->rs_ntup = (HeapTuple)
	    heapgettup(sdesc->rs_rd,
		       (sdesc->rs_ctup == NULL) ?
		         (ItemPointer)NULL : &sdesc->rs_ctup->t_ctid,
		       1,
		       &sdesc->rs_nbuf,
		       sdesc->rs_tr,
		       sdesc->rs_nkeys,
		       &sdesc->rs_key,
		       pageskip,
		       initskip);
	}

    /* ----------------
     * Should not unpin the buffer pages.  They may still be in use.
     * ----------------
     */
    if (sdesc->rs_ptup != NULL) {
	sdesc->rs_mptid = sdesc->rs_ptup->t_ctid;
    } else {
	ItemPointerSetInvalid(&sdesc->rs_mptid);
    }
    if (sdesc->rs_ctup != NULL) {
	sdesc->rs_mctid = sdesc->rs_ctup->t_ctid;
    } else {
	ItemPointerSetInvalid(&sdesc->rs_mctid);
    }
    if (sdesc->rs_ntup != NULL) {
	sdesc->rs_mntid = sdesc->rs_ntup->t_ctid;
    } else {
	ItemPointerSetInvalid(&sdesc->rs_mntid);
    }
}

/* ----------------
 *	heap_restrpos	- restore position to marked location
 *
 *	Note:  there are bad side effects here.  If we were past the end
 *	of a relation when heapmarkpos is called, then if the relation is
 *	extended via insert, then the next call to heaprestrpos will set
 *	cause the added tuples to be visible when the scan continues.
 *	Problems also arise if the TID's are rearranged!!!
 *
 * XXX	might be better to do direct access instead of
 *	using the generality of heapgettup().
 *
 * XXX It is very possible that when a scan is restored, that a tuple
 * XXX which previously qualified may fail for time range purposes, unless
 * XXX some form of locking exists (ie., portals currently can act funny.
 * ----------------
 */
void
heap_restrpos(sdesc)
    HeapScanDesc	sdesc;
{
    int pageskip, initskip;

    /* ----------------
     *	increment access statistics
     * ----------------
     */
    IncrHeapAccessStat(local_restrpos);
    IncrHeapAccessStat(global_restrpos);
	
    /* XXX no amrestrpos checking that ammarkpos called */
	
    /* Note: no locking manipulations needed */

    if (BufferIsValid(sdesc->rs_pbuf)) {
	BufferPut(sdesc->rs_pbuf, L_UNPIN);
    }
    if (BufferIsValid(sdesc->rs_cbuf)) {
	BufferPut(sdesc->rs_cbuf, L_UNPIN);
    }
    if (BufferIsValid(sdesc->rs_nbuf)) {
	BufferPut(sdesc->rs_nbuf, L_UNPIN);
    }

    if (!ItemPointerIsValid(&sdesc->rs_mptid)) {
	sdesc->rs_ptup = NULL;
	sdesc->rs_pbuf = InvalidBuffer;
    } else {
	sdesc->rs_ptup = (HeapTuple)
	    heapgettup(sdesc->rs_rd,
		       &sdesc->rs_mptid,
		       0,
		       &sdesc->rs_pbuf,
		       NowTimeQual,
		       0,
		       (ScanKey) NULL,
		       pageskip,
		       initskip);
    }

    if (!ItemPointerIsValid(&sdesc->rs_mctid)) {
	sdesc->rs_ctup = NULL;
	sdesc->rs_cbuf = InvalidBuffer;
    } else {
	sdesc->rs_ctup = (HeapTuple)
	    heapgettup(sdesc->rs_rd,
		       &sdesc->rs_mctid,
		       0,
		       &sdesc->rs_cbuf,
		       NowTimeQual,
		       0,
		       (ScanKey) NULL,
		       pageskip,
		       initskip);
    }

    if (!ItemPointerIsValid(&sdesc->rs_mntid)) {
	sdesc->rs_ntup = NULL;
	sdesc->rs_nbuf = InvalidBuffer;
    } else {
	sdesc->rs_ntup = (HeapTuple)
	    heapgettup(sdesc->rs_rd,
		       &sdesc->rs_mntid,
		       0,
		       &sdesc->rs_nbuf,
		       NowTimeQual,
		       0,
		       (ScanKey) NULL,
		       pageskip,
		       initskip);
	}
}

