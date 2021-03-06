
/*
 *  PSKIP.C
 *
 *  related to internal_page.h, not internal.h
 *
 */

static char	pskip_c[] = "$Header$";

#include "tmp/c.h"

#include "access/skey.h"
#include "storage/buf.h"
#include "storage/bufmgr.h"
#include "storage/bufpage.h"
#include "storage/itemid.h"
#include "storage/itempos.h"
#include "storage/itemptr.h"
#include "storage/page.h"
#include "utils/memutils.h"
#include "utils/log.h"

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
		IncrBufferRefCount(db);
	} else if (RelationIsValid(relation) == false) {
		elog(FATAL, "startpskip: NULL rdesc and db");
	} else {
		db = ReadBuffer(relation, ItemPointerGetBlockNumber(tid));
		if (db < 0) {
			elog(WARN, "startpskip: failed ReadBuffer()");
		}
	}
	objp = (ItemSubposition)palloc(sizeof *objp);
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
	 * check this and the #define MAXATTS in ../h/htup.h
	 */
	if (ItemIdIsContinuing(objp->op_lpp)) {
		cp += TCONTPAGELEN;
	}
	off = ((HeapTuple)cp)->t_hoff;
	objp->op_cp = cp + off;
	if (ItemIdIsContinuing(objp->op_lpp)) {
		off += TCONTPAGELEN;
	}
	if (objp->op_cp != (char *)LONGALIGN(objp->op_cp))
		elog(FATAL, "startpskip: malformed heap tuple");
	objp->op_len = (*objp->op_lpp).lp_len - off;
	return (objp);
}

endpskip(objp)
ItemSubposition	objp;
{
  	Assert(BufferIsValid(objp->op_db));
	ReleaseBuffer(objp->op_db);
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
		ReleaseBuffer(objp->op_db);
		objp->op_db = ReadBuffer(relation, pagenum);
		if (RelationIsValid(objp->op_db) == false) {
			elog(WARN, "pskip: failed ReadBuffer");
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
		ReleaseBuffer(objp->op_db);
		objp->op_db = ReadBuffer(relation, pagenum);
		if (RelationIsValid(objp->op_db) == false) {
			elog(WARN, "pfill: failed ReadBuffer");
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
	return (0);
}
