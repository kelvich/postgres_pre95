
/*
 * 
 * POSTGRES Data Base Management System
 * 
 * Copyright (c) 1988 Regents of the University of California
 * 
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for educational, research, and non-profit purposes and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of the University of California not be used in advertising
 * or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Permission to incorporate this
 * software into commercial products can be obtained from the Campus
 * Software Office, 295 Evans Hall, University of California, Berkeley,
 * Ca., 94720 provided only that the the requestor give the University
 * of California a free licence to any derived software for educational
 * and research purposes.  The University of California makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 * 
 */



/*
 * hio.c --
 *	POSTGRES heap access method input/output code.
 */

#include "c.h"

#include "align.h"
#include "block.h"
#include "buf.h"
#include "bufmgr.h"
#include "bufpage.h"
#include "htup.h"
#include "itemid.h"
#include "itemptr.h"
#include "log.h"
#include "off.h"
#include "rel.h"

#include "hio.h"

RcsId("$Header$");

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

	Assert(RelationIsValid(relation));
	Assert(HeapTupleIsValid(tuple));

	numberOfBlocks = RelationGetNumberOfBlocks(relation);
	Assert(IndexIsInBounds(blockIndex, numberOfBlocks));

	buffer = RelationGetBuffer(relation, blockIndex, L_UP);
	if (!BufferIsValid(buffer)) {
		elog(WARN, "RelationPutHeapTuple: no buffer for %ld in %s",
			blockIndex, &relation->rd_rel->relname);
	}

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

	HeapTupleStoreRuleLock(LintCast(HeapTuple, item), buffer,
		tuple->t_lock.l_lock);

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

	Assert(RelationIsValid(relation));
	Assert(HeapTupleIsValid(tuple));

/* correct validity checking here and elsewhere */
	headb = RelationGetBuffer(relation, P_NEW, L_NEW);
	if (!BufferIsValid(headb)) {
		elog(WARN,
			"RelationPutLongHeapTuple: no new buffer for block #1");
	}
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
			if (!BufferIsValid(b)) {
				elog(WARN,
					"RelationPutLongHeapTuple: no new #n");
			}
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

	HeapTupleStoreRuleLock((HeapTuple)tp, headb, tuple->t_lock.l_lock);

	if (BufferPut(headb, L_UN | L_EX | L_WRITE) < 0) {
		elog(WARN, "RelationPutLongHeapTuple: failed BufferPut #1");
	}

	/* return(TIDFOR(blockNumber, 0)); */
}
