
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
 * heapam.c --
 *	POSTGRES heap access method code.
 */

#include "c.h"

#include "att.h"
#include "attnum.h"
#include "htup.h"
#include "log.h"
#include "rel.h"
#include "relcache.h"
#include "relscan.h"
#include "skey.h"

#include "heapam.h"

RcsId("$Header$");

/*
 * XXX Note that longer function names are preferred when called
 * from C.  Thus, the code should eventually be incorporated into
 * the functions, below.  Better yet might be to place the functions
 * into several files.
 */

bool
HeapScanIsValid(scan)
	HeapScan	scan;
{
	return ((bool)PointerIsValid(scan));
}

void
RelationNameCreateHeapRelation(relationName, archiveMode, numberOfAttributes,
		tupleDescriptor)
	Name		relationName;
	ArchiveMode	archiveMode;
	AttributeNumber	numberOfAttributes;
	TupleDescriptor	tupleDescriptor;
{
	amcreate(relationName, archiveMode, numberOfAttributes,
		tupleDescriptor);
}

Relation
RelationNameCreateTemporaryRelation(relationName, numberOfAttributes,
		tupleDescriptor)
	Name		relationName;
	AttributeNumber	numberOfAttributes;
	TupleDescriptor	tupleDescriptor;
{
	return (amcreatr(relationName, numberOfAttributes, tupleDescriptor));
}

void
RelationNameCreateVersionRelation(originalRelationName, versionRelationName,
		time)
	Name	originalRelationName;
	Name	versionRelationName;
	long	time;
{
	amcreatv(originalRelationName, versionRelationName, time);
}

void
RelationNameDestroyHeapRelation(relationName)
	Name	relationName;
{
	amdestroy(relationName);
}

void
RelationNameMergeRelations(oldRelationName, newRelationName)
	Name	oldRelationName;
	Name	newRelationName;
{
	ammergev(oldRelationName, newRelationName);
}

Relation
ObjectIdOpenHeapRelation(relationId)	/* XXX should be RelationIdOpenHeapRelation */
	ObjectId	relationId;
{
	return (RelationIdGetRelation(relationId));
}

Relation
RelationNameOpenHeapRelation(relationName)
	Name	relationName;
{
	return (amopenr(relationName));
}

void
RelationCloseHeapRelation(relation)
	Relation	relation;
{
	amclose (relation);
}

HeapTuple
RelationGetHeapTupleByItemPointer(relation, range, heapItem, bufferOutP)
	Relation	relation;
	TimeRange	range;
	ItemPointer	heapItem;
	Buffer		*bufferOutP;
{
	return (amgetunique(relation, range, heapItem, bufferOutP));
}

RuleLock
RelationInsertHeapTuple(relation, heapTuple, offsetOutP)
	Relation	relation;
	HeapTuple	heapTuple;
	double		*offsetOutP;
{
	return (aminsert(relation, heapTuple, offsetOutP));
}

RuleLock
RelationDeleteHeapTuple(relation, heapItem)
	Relation	relation;
	ItemPointer	heapItem;
{
	return (amdelete(relation, heapItem));
}

void
RelationPhysicallyDeleteHeapTuple(relation, heapItem)
	Relation	relation;
	ItemPointer	heapItem;
{
	/* XXX start here */
/*
	ItemId		lp;
	struct	tuple	*tp;
	PageHeader	dp;
	Buffer		b;
	long		time();
	char		*fmgr();
	
	b = RelationGetBuffer(relation, ItemPointerGetBlockNumber(heapItem), L_UP);
	if (!BufferIsValid(b)) {
*/
		/* XXX L_SH better ??? */
/*
		elog(WARN, "heapdelete: failed RelationGetBuffer");
	}
	dp = (PageHeader)BufferSimpleGetPage(b);
	lp = PageGetItemId(dp, ItemPointerSimpleGetOffsetIndex(heapItem));
	if (!ItemIdHasValidHeapTupleForQualification(lp, b, DefaultTimeRange,
			0, (ScanKey)NULL)) {
*/
		/* XXX call something else */
/*
		if (BufferPut(b, L_UN | L_UP) < 0)
			elog(WARN, "heapdelete: failed BufferPut");
		elog(WARN, "heapdelete: (am)invalid heapItem");
	}
	tp = (struct tuple *)PageGetItem(dp, lp);
*/
	/* XXX order problems if not atomic assignment ??? */
/*
	if (BufferPut(b, L_EX) < 0)
		elog(WARN, "heapdelete: failed BufferPut(L_EX)");
	TransactionIdStore(GetCurrentTransactionId(), (Pointer)tp->t_xmax);
	tp->t_cmax = GetCurrentCommandId();
*/
	/* tp->t_tmax = InvalidTime; */
/*
	ItemPointerSetInvalid(&tp->t_chain);
	if (BufferPut(b, L_UN | L_EX | L_WRITE) < 0)
		elog(WARN, "heapdelete: failed BufferPut(L_UN | L_WRITE)");
}
	return (amdelete(relation, heapItem));
*/
}

RuleLock
RelationReplaceHeapTuple(relation, heapItem, tuple, offsetOutP)
	Relation	relation;
	ItemPointer	heapItem;
	HeapTuple	tuple;
	double		*offsetOutP;
{
	return (amreplace(relation, heapItem, tuple, offsetOutP));
}

Datum
HeapTupleGetAttributeValue(tuple, buffer, attributeNumber, tupleDescriptor,
		attributeIsNullOutP)
	HeapTuple	tuple;
	Buffer		buffer;
	AttributeNumber	attributeNumber;
	TupleDescriptor	tupleDescriptor;
	Boolean		*attributeIsNullOutP;
{
	return (PointerGetDatum(amgetattr(tuple, buffer, attributeNumber,
		tupleDescriptor, attributeIsNullOutP)));
}

HeapScan
RelationBeginHeapScan(relation, startScanAtEnd, timer, numberOfKeys, key)
	Relation	relation;
	Boolean		startScanAtEnd;
	TimeRange	timer;
	uint16		numberOfKeys;
	ScanKey		key;
{
	return (ambeginscan(relation, startScanAtEnd, timer, numberOfKeys,
		key));
}

void
HeapScanRestart(scan, restartScanAtEnd, key)
	HeapScan	scan;
	bool		restartScanAtEnd;
	ScanKey		key;
{
	amrescan(scan, restartScanAtEnd, key);
}

void
HeapScanEnd(scan)
	HeapScan	scan;
{
	amendscan(scan);
}

void
HeapScanMarkPosition(scan)
	HeapScan	scan;
{
	ammarkpos(scan);
}

void
HeapScanRestorePosition(scan)
	HeapScan	scan;
{
	amrestrpos(scan);
}

HeapTuple
HeapScanGetNextTuple(scan, backwards, bufferOutP)
	HeapScan	scan;
	Boolean		backwards;
	Buffer		*bufferOutP;
{
	return (amgetnext(scan, backwards, bufferOutP));
}

/*
 * XXX probably do not need a free tuple routine for heaps.
 */
