/*
 * hio.c --
 *	POSTGRES heap access method input/output code.
 */

#include "tmp/c.h"

RcsId("$Header$");

#include "access/heapam.h"
#include "access/hio.h"
#include "access/htup.h"

#include "storage/block.h"
#include "storage/buf.h"
#include "storage/bufmgr.h"
#include "storage/bufpage.h"
#include "storage/itemid.h"
#include "storage/itemptr.h"
#include "storage/off.h"

#include "utils/memutils.h"
#include "utils/log.h"
#include "utils/rel.h"

/*
 *	amputunique	- place tuple at tid
 *
 *	Currently on errors, calls elog.  Perhaps should return -1?
 *	Possible errors include the addition of a tuple to the page
 *	between the time the linep is chosen and the page is L_UP'd.
 *
 *	This should be coordinated with the B-tree code.
 *	Probably needs to have an amdelunique to allow for
 *	internal index records to be deleted and reordered as needed.
 *	For the heap AM, this should never be needed.
 */
void
RelationPutHeapTuple(relation, blockIndex, tuple)
	Relation	relation;
	BlockNumber	blockIndex;
	HeapTuple	tuple;
{
	Buffer		buffer;
	PageHeader	pageHeader;
	BlockNumber	numberOfBlocks;
	OffsetIndex	offsetIndex;
	unsigned	len;
	ItemId		itemId;
	Item		item;

	/* ----------------
	 *	increment access statistics
	 * ----------------
	 */
	IncrHeapAccessStat(local_RelationPutHeapTuple);
	IncrHeapAccessStat(global_RelationPutHeapTuple);
			
	Assert(RelationIsValid(relation));
	Assert(HeapTupleIsValid(tuple));

	numberOfBlocks = RelationGetNumberOfBlocks(relation);
	Assert(IndexIsInBounds(blockIndex, numberOfBlocks));

	buffer = RelationGetBuffer(relation, blockIndex, L_UP);
#ifndef NO_BUFFERISVALID
	if (!BufferIsValid(buffer)) {
		elog(WARN, "RelationPutHeapTuple: no buffer for %ld in %s",
			blockIndex, &relation->rd_rel->relname);
	}
#endif

	pageHeader = LintCast(PageHeader, BufferSimpleGetPage(buffer));
	len = (unsigned)LONGALIGN(tuple->t_len);
	Assert((int)len <= PageGetFreeSpace(pageHeader));

	if (BufferPut(buffer, L_EX) < 0) {
		elog(WARN, "amputunique: failed BufferPut(L_EX)");
	}

	offsetIndex = -1 + PageAddItem((Page)pageHeader, (Item)tuple,
		tuple->t_len, InvalidOffsetNumber, LP_USED);

	itemId = PageGetItemId((Page)pageHeader, offsetIndex);
	item = PageGetItem((Page)pageHeader, itemId);

	ItemPointerSimpleSet(&LintCast(HeapTuple, item)->t_ctid, blockIndex,
		1 + offsetIndex);

	HeapTupleStoreRuleLock(LintCast(HeapTuple, item), buffer);

	if (BufferPut(buffer, L_UN | L_EX | L_WRITE) < 0) {
		elog(WARN, "amputunique: failed BufferPut(L_UN | L_WRITE)");
	}

	/* return an accurate tuple */
	ItemPointerSimpleSet(&tuple->t_ctid, blockIndex, 1 + offsetIndex);
}

/*
 *	amputnew	- adds a tuple on a new page
 *
 *	Currently allows any sized tuples to be placed on a new
 *	page.  Long tuples must be inserted via amputnew.
 *
 *	Eventually, should have ability to specify approximate location
 *	for implementation of aggregates.	XXX
 */
/* TID	*   handle this properly XXX */

void
RelationPutLongHeapTuple(relation, tuple)
	Relation	relation;
	HeapTuple	tuple;
{
	Buffer		b;
	Buffer		headb;
	PageHeader	dp;
	int		off, taillen, tailtlen;
	unsigned long	len;
	ItemId		lpp;
	char		*tp;
	ItemContinuation	tcp, headtcp;
	unsigned long	blocks;
	BlockNumber	blockNumber;

	/* ----------------
	 *	increment access statistics
	 * ----------------
	 */
	IncrHeapAccessStat(local_RelationPutLongHeapTuple);
	IncrHeapAccessStat(global_RelationPutLongHeapTuple);
	
	Assert(RelationIsValid(relation));
	Assert(HeapTupleIsValid(tuple));

/* correct validity checking here and elsewhere */
	headb = RelationGetBuffer(relation, P_NEW, L_NEW);
#ifndef NO_BUFFERISVALID
	if (!BufferIsValid(headb)) {
		elog(WARN,
			"RelationPutLongHeapTuple: no new buffer for block #1");
	}
#endif
	dp = LintCast(PageHeader, BufferSimpleGetPage(headb));
	len = (unsigned long)LONGALIGN(tuple->t_len);
	ItemPointerSimpleSet(&tuple->t_ctid, BufferGetBlockNumber(headb), 1);
	if (len <= MAXTUPLEN) {
		taillen = blocks = 0;
	} else {
		tailtlen = tuple->t_len - len;
		if ((len -= MAXTUPLEN) <= MAXTCONTLEN) {
			blocks = 0;
			taillen = MAXTUPLEN;
		} else {
			taillen = MAXTUPLEN;
			blocks = --len / MAXTCONTLEN;	/* XXX overflow */
			len %= MAXTCONTLEN;
			len++;
		}
		/* keep the header as small as possible but unfragmented */
		if (len < tuple->t_hoff) {
			taillen -= tuple->t_hoff - len;
			len = tuple->t_hoff;	/* assumed long aligned */
		}
		tailtlen = taillen - tailtlen;
	}
	/*
	 *	At this point, len is the data length in the first
	 *	(header) page, taillen is the length of the last
	 *	(tail) page, and pages is the number of intervening
	 *	full data pages.
	 *
	 *	Now write the pages in order 1, N, N-1, N-2, ..., 2.
	 */
	if (!taillen) {
		off = BLCKSZ - len;
		tp = (char *)dp + off;
		len = tuple->t_len;
		bcopy((char *)tuple, tp, (int)len);
	} else {
		off = BLCKSZ - len - TCONTPAGELEN;
		headtcp = (ItemContinuation)((char *)dp + off);
		bcopy((char *)tuple, headtcp->itemData, (int)len);
		tp = (char *)tuple + len + MAXTCONTLEN * (int)blocks;
		len += TCONTPAGELEN;
		/*
		 *	At this point, tp points to the data in
		 *	the tail of the tuple and headtcp points
		 *	to the struct tcont space.
		 */	
	}
	BufferSimpleInitPage(headb);

	dp->pd_lower += sizeof *lpp;
	dp->pd_upper = off;
/*	PageGetMaxOffsetIndex(dp) += 1;
*/
	lpp = dp->pd_linp;
	(*lpp).lp_off = off;
	(*lpp).lp_len = len;
	if (!taillen)
		(*lpp).lp_flags = LP_USED;
	else {
		(*lpp).lp_flags = LP_USED | LP_DOCNT;
		b = RelationGetBuffer(relation, P_NEW, L_NEW);
		if (BufferIsInvalid(b)) {
			elog(WARN, "RelationPutLongHeapTuple: no new block #$");
		}
		dp = (PageHeader)BufferSimpleGetPage(b);
		off = BLCKSZ - taillen;

		BufferSimpleInitPage(b);
		dp->pd_lower += sizeof *lpp;
		dp->pd_upper = off;
/*		PageGetMaxOffsetIndex(dp) += 1;
*/

		lpp = dp->pd_linp;
		(*lpp).lp_off = off;
		(*lpp).lp_len = tailtlen;
		(*lpp).lp_flags = LP_USED | LP_CTUP;
		blockNumber = BufferGetBlockNumber(b);
		bcopy(tp, (char *)dp + off, tailtlen);
		if ((BufferWriteInOrder(b, headb)) < 0) {
			elog(WARN,
				"RelationPutLongHeapTuple: no WriteInOrder #$");
		}
		if (BufferPut(b, L_UN | L_EX | L_WRITE) < 0)
			elog(WARN, "RelationPutLongHeapTuple: no BufferPut #$");
		while (blocks--) {
			b = RelationGetBuffer(relation, P_NEW, L_NEW);
#ifndef NO_BUFFERISVALID
			if (!BufferIsValid(b)) {
				elog(WARN,
					"RelationPutLongHeapTuple: no new #n");
			}
#endif
			dp = (PageHeader)BufferSimpleGetPage(b);
			tp -= MAXTCONTLEN;
			tcp = (ItemContinuation)(dp + 1);
			ItemPointerSet(&tcp->itemPointerData,
				SinglePagePartition, blockNumber, (PageNumber)0,
				(OffsetNumber)1);
/*
			BlockIdSet(tcp->tc_page, blockNumber);
*/
			bcopy(tp, tcp->itemData, MAXTCONTLEN);

			BufferSimpleInitPage(b);
			dp->pd_lower += sizeof *lpp;
			dp->pd_upper = sizeof *dp;
/*			PageGetMaxOffsetIndex(dp) += 1;
*/

			lpp = dp->pd_linp;
			(*lpp).lp_off = sizeof *dp;
			(*lpp).lp_len = MAXTUPLEN;	/* TCONTLEN + PAGELEN */
			(*lpp).lp_flags = LP_USED | LP_DOCNT | LP_CTUP;
			blockNumber = BufferGetBlockNumber(b);
			if ((BufferWriteInOrder(b, headb)) < 0) {
				elog(WARN,
					"RelationPutLongHeapTuple: no WriteIn");
			}
			if (BufferPut(b, L_UN | L_EX | L_WRITE) < 0) {
				elog(WARN, "RelationPutLongHeapTuple: no Put");
			}
		}
		ItemPointerSet(&headtcp->itemPointerData, SinglePagePartition,
			blockNumber, (PageNumber)0, (OffsetNumber)1);
		tp = &headtcp->itemData[0];
/*
		BlockIdSet(headtcp->tc_page, blockNumber);
*/
	}

	HeapTupleStoreRuleLock((HeapTuple)tp, headb);

	if (BufferPut(headb, L_UN | L_EX | L_WRITE) < 0) {
		elog(WARN, "RelationPutLongHeapTuple: failed BufferPut #1");
	}

	/* return(TIDFOR(blockNumber, 0)); */
}
