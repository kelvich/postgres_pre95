/*
 * heapam.c --
 *	POSTGRES heap access method code.
 */

#include "c.h"

RcsId("$Header$");

#include "att.h"
#include "attnum.h"
#include "htup.h"
#include "log.h"
#include "rel.h"
#include "relcache.h"
#include "relscan.h"
#include "skey.h"
#include "tqual.h"

#include "heapam.h"

/*
 * XXX Note that longer function names are preferred when called
 * from C.  Thus, the code should eventually be incorporated into
 * the functions, below.  Better yet might be to place the functions
 * into several files.
 */

bool
HeapScanIsValid(scan)
	HeapScanDesc	scan;
{
	return ((bool)PointerIsValid(scan));
}

ObjectId
RelationNameCreateHeapRelation(relationName, archiveMode, numberOfAttributes,
		tupleDescriptor)
	Name		relationName;
	ArchiveMode	archiveMode;
	AttributeNumber	numberOfAttributes;
	TupleDescriptor	tupleDescriptor;
{
	return (amcreate(relationName, archiveMode, numberOfAttributes,
		tupleDescriptor));
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
RelationGetHeapTupleByItemPointer(relation, qual, heapItem, bufferOutP)
	Relation	relation;
	TimeQual	qual;
	ItemPointer	heapItem;
	Buffer		*bufferOutP;
{
	return (amgetunique(relation, qual, heapItem, bufferOutP));
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
	if (!ItemIdHasValidHeapTupleForQualification(lp, b, NowTimeRange,
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

HeapScanDesc
RelationBeginHeapScan(relation, startScanAtEnd, qual, numberOfKeys, key)
	Relation	relation;
	Boolean		startScanAtEnd;
	TimeQual	qual;
	uint16		numberOfKeys;
	ScanKey		key;
{
	return (ambeginscan(relation, startScanAtEnd, qual, numberOfKeys, key));
}

void
HeapScanRestart(scan, restartScanAtEnd, key)
	HeapScanDesc	scan;
	bool		restartScanAtEnd;
	ScanKey		key;
{
	amrescan(scan, restartScanAtEnd, key);
}

void
HeapScanEnd(scan)
	HeapScanDesc	scan;
{
	amendscan(scan);
}

void
HeapScanMarkPosition(scan)
	HeapScanDesc	scan;
{
	ammarkpos(scan);
}

void
HeapScanRestorePosition(scan)
	HeapScanDesc	scan;
{
	amrestrpos(scan);
}

HeapTuple
HeapScanGetNextTuple(scan, backwards, bufferOutP)
	HeapScanDesc	scan;
	Boolean		backwards;
	Buffer		*bufferOutP;
{
	return (amgetnext(scan, backwards, bufferOutP));
}

/*
 * XXX probably do not need a free tuple routine for heaps.
 * Huh?  Free tuple is not necessary for tuples returned by scans, but
 * is necessary for tuples which are returned by RelationGetTupleByItemPointer.
 * -hirohama
 */
