
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



static char	pskip_c[] = "$Header$";

#include "c.h"
#include "clib.h"

#include "access.h"
#include "align.h"
#include "buf.h"
#include "bufmgr.h"
#include "bufpage.h"
#include "itemid.h"
#include "itempos.h"
#include "itemptr.h"
#include "log.h"
#include "page.h"

/*
 *	pskip.c		- standard buffer page traversal routines
 */

ItemSubposition
startpskip(db, relation, tid)
	Buffer		db;
	Relation	relation;
	ItemPointer	tid;
{
	ItemSubposition	objp;
	PageHeader	dp;
	char		*cp;
	unsigned	off;

	if (ItemPointerIsValid(tid) == false) {
		elog(FATAL, "startpskip: passed NULL tid");
	}
	if (BufferIsValid(db)) {
/*
		if (db->db_lock != L_PIN)
			elog(DEBUG, "startpskip: db->db_lock is 0%o",
				db->db_lock);
*/
		if (BufferPut(db, L_DUP) < 0)
			elog(WARN, "startpskip: failed BufferPut(L_DUP)");
	} else if (RelationIsValid(relation) == false) {
		elog(FATAL, "startpskip: NULL rdesc and db");
	} else {
		db = RelationGetBuffer(relation,
			ItemPointerGetBlockNumber(tid), L_PIN);
		if (db < 0) {
			elog(WARN, "startpskip: failed RelationGetBuffer()");
		}
	}
	objp = (ItemSubposition)palloc(sizeof *objp);
	if (BufferPut(db, L_SH) < 0)
		elog(WARN, "startpskip: failed BufferPut(L_SH)");
	dp = (PageHeader)BufferGetBlock(db);
	objp->op_db = db;
	objp->op_lpp = PageGetItemId(dp, ItemPointerSimpleGetOffsetIndex(tid));
	if (ItemIdIsLock(objp->op_lpp)) {
		elog(FATAL, "startpskip: cannot handle LOCK's yet!");  /* XXX */
	} else if (ItemIdIsInternal(objp->op_lpp)) {
		elog(FATAL, "startpskip: cannot handle itup's yet!");  /* XXX */
	}
	cp = (char *)dp + (*objp->op_lpp).lp_off;
	/*
	 *	Assumes < 700 attributes--else use (tup->t_hoff & I1MASK)
	 * check this and the #define MAXATTS in ../h/tuple.h
	 */
	if (ItemIdIsContinuing(objp->op_lpp)) {
		cp += TCONTPAGELEN;
	}
	off = ((struct tuple *)cp)->t_hoff;
	objp->op_cp = cp + off;
	if (ItemIdIsContinuing(objp->op_lpp)) {
		off += TCONTPAGELEN;
	}
	if (objp->op_cp != (char *)LONGALIGN(objp->op_cp))
		elog(FATAL, "startpskip: malformed heap tuple");
	objp->op_len = (*objp->op_lpp).lp_len - off;
	if (BufferPut(db, L_PIN) < 0)
		elog(WARN, "startpskip: failed BufferPut(L_PIN)");
	return (objp);
}

endpskip(objp)
ItemSubposition	objp;
{
  	Assert(BufferIsValid(objp->op_db));
	if (BufferPut(objp->op_db, L_UNPIN) < 0)
		elog(WARN, "endpskip: failed BufferPut");
	pfree(objp);
}

/*
 *	pskip		- skip bytes across a chain of pages
 *
 *	Returns -1 upon error.
 */

int
pskip(objp, skip)
ItemSubposition	objp;
unsigned long	skip;
{
	PageHeader	dpp;
	Relation	relation;
	unsigned long	pagenum;
	int		lock;

	lock = objp->op_len <= skip;
	Assert(BufferIsValid(objp->op_db));
	if (lock && BufferPut(objp->op_db, L_SH) < 0)
		elog(WARN, "pskip: failed BufferPut(L_SH)");
	while (objp->op_len <= skip) {
		if (!ItemIdIsContinuing(objp->op_lpp)) {
			if (objp->op_len != skip)
				elog(WARN, "pskip: %lu past end of object",
					skip - objp->op_len);	/* XXX elog */
			break;
		}
		skip -= objp->op_len;
		dpp = (PageHeader)BufferGetBlock(objp->op_db);

		pagenum = ItemPointerGetBlockNumber((ItemPointer)
			(uint16 *)((char *)dpp + (*objp->op_lpp).lp_off));
		/* XXX ignoring the other information */
/*
		pagenum = BlockIdGetBlockNumber((uint16 *)((char *)dpp +
			(*objp->op_lpp).lp_off));
*/
		relation = BufferGetRelation(objp->op_db);
		if (BufferPut(objp->op_db, L_UN | L_SH) < 0)
			elog(WARN, "pskip: failed BufferPut");
		objp->op_db = RelationGetBuffer(relation, pagenum, L_SH);
		if (RelationIsValid(objp->op_db) == false) {
			elog(WARN, "pskip: failed RelationGetBuffer");
		}
		dpp = (PageHeader)BufferGetBlock(objp->op_db);
		objp->op_lpp = dpp->pd_linp;	/* always first line number */
		if (ItemIdIsContinuing(objp->op_lpp)) {
			objp->op_cp = (char *)dpp + (*objp->op_lpp).lp_off +
				TCONTPAGELEN;
			objp->op_len = (*objp->op_lpp).lp_len - TCONTPAGELEN;
		} else {
			objp->op_cp = (char *)dpp + (*objp->op_lpp).lp_off;
			objp->op_len = (*objp->op_lpp).lp_len;
		}
	}
	if (lock && BufferPut(objp->op_db, L_PIN) < 0)
		elog(WARN, "pskip: failed BufferPut(L_PIN)");
	objp->op_len -= (unsigned)skip;
	objp->op_cp += skip;
	return (0);
}

/*
 *	pfill		- fill bytes across a chain of pages
 *
 *	Returns -1 upon error.  Otherwise, *fillp contains the
 *	next PSIZE(fillp) bytes of the object.
 */

int
pfill(objp, fillp)
ItemSubposition	objp;
char		*fillp;
{
	unsigned long	skip;
	char		*cp;
	PageHeader	dpp;
	Relation	relation;
	unsigned long	pagenum;

	cp = fillp;
	skip = PSIZE(fillp);
	if (BufferPut(objp->op_db, L_SH) < 0)
		elog(WARN, "pfill: failed BufferPut(L_SH)");
	while (objp->op_len <= skip) {
		if (!ItemIdIsContinuing(objp->op_lpp)) {
			if (objp->op_len != skip)
				elog(WARN, "pfill: %lu past end of object",
					skip - objp->op_len);	/* XXX elog */
			break;
		}
		bcopy(objp->op_cp, cp, (int)objp->op_len);
		skip -= objp->op_len;
		cp += objp->op_len;
		dpp = (PageHeader)BufferGetBlock(objp->op_db);

		pagenum = ItemPointerGetBlockNumber((ItemPointer)
			(uint16 *)((char *)dpp + (*objp->op_lpp).lp_off));
		/* XXX ignoring the other information */
/*
		pagenum = BlockIdGetBlockNumber((uint16 *)((char *)dpp +
			(*objp->op_lpp).lp_off));
*/
		relation = BufferGetRelation(objp->op_db);
		if (BufferPut(objp->op_db, L_UN | L_SH) < 0)
			elog(WARN, "pfill: failed BufferPut");
		objp->op_db = RelationGetBuffer(relation, pagenum, L_SH);
		if (RelationIsValid(objp->op_db) == false) {
			elog(WARN, "pfill: failed RelationGetBuffer");
		}
		dpp = (PageHeader)BufferGetBlock(objp->op_db);
		objp->op_lpp = dpp->pd_linp;	/* always first line number */
		if (ItemIdIsContinuing(objp->op_lpp)) {
			objp->op_cp = (char *)dpp + (*objp->op_lpp).lp_off +
				TCONTPAGELEN;
			objp->op_len = (*objp->op_lpp).lp_len - TCONTPAGELEN;
		} else {
			objp->op_cp = (char *)dpp + (*objp->op_lpp).lp_off;
			objp->op_len = (*objp->op_lpp).lp_len;
		}
	}
	bcopy(objp->op_cp, cp, (int)skip);
	objp->op_len -= (unsigned)skip;
	objp->op_cp += skip;
	if (BufferPut(objp->op_db, L_PIN) < 0)
		elog(WARN, "pfill: failed BufferPut(L_PIN)");
	return (0);
}
