
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
 * lmgr.c --
 *	POSTGRES lock manager code.
 */

/* #define LOCKDEBUGALL	1 */
/* #define LOCKDEBUG	1 */

#ifdef	LOCKDEBUGALL
#define LOCKDEBUG	1
#endif	/* defined(LOCKDEBUGALL) */

#include "c.h"

#include "anum.h"	/* XXX for VariableRelationId */
#include "block.h"
#include "buf.h"
#include "catname.h"
#include "heapam.h"
#include "datum.h"
#include "log.h"
#include "ipci.h"	/* XXX for PageLockTableId and MultiLevelLockTableId */
#include "itemptr.h"
#include "part.h"
#include "pagenum.h"
#include "pladt.h"
#include "plm.h"	/* XXX for L_DONE, etc. */
#include "rel.h"
#include "relscan.h"
#include "rproc.h"
#include "skey.h"
#include "tqual.h"
#include "tuple.h"
#include "xid.h"
#include "xstate.h"

#include "lmgr.h"

RcsId("$Header$");

#define MaxRetries	10	/* XXX about a minute--a hack */

typedef struct LockInfoData {
	bool			initialized;
	LRelId			lRelId;
	TransactionIdData	transactionIdData;
	uint16			flags;
} LockInfoData;

#define IntentReadRelationLock	0x0100
#define ReadRelationLock	0x0200
#define IntentWriteRelationLock	0x0400
#define WriteRelationLock	0x0800
#define IntentReadPageLock	0x1000
#define ReadTupleLock		0x2000

#define TupleLevelLockCountMask	0x000f

#define TupleLevelLockLimit	10

typedef LockInfoData	*LockInfo;

extern bool	TransactionInitWasProcessed;	/* XXX style */
extern ObjectId	MyDatabaseId;
bool	LockingIsDisabled = false;	/* XXX should be static, fix cinit */

static LRelId	VariableRelationLRelId =
	{ VariableRelationId, InvalidObjectId };

/*
 * LockInfoIsValid --
 *	True iff lock information is valid.
 */
static
bool
LockInfoIsValid ARGS((
	LockInfo	info
));

LRelId
RelationGetLRelId(relation)
	Relation	relation;
{
	Assert(RelationIsValid(relation));
	/* XXX probably not wanted ... Assert(ObjectIdIsValid(MyDatabaseId)); */

	if (!LockInfoIsValid(relation->lockInfo)) {
#ifdef	LOCKDEBUG
		elog(NOTICE, "RelationGetLRelId(%s) invalid lockInfo",
			RelationGetRelationName(relation));
#endif	/* defined(LOCKDEBUG) */
		RelationInitLockInfo(relation);
	}

	if (NameIsEqual(RelationGetRelationName(relation),
			VariableRelationName)) {

		/* XXX hack to prevent problems during
		 * VARIABLE relation initialization
		 */
		return (VariableRelationLRelId);
	}

	return (((LockInfo)relation->lockInfo)->lRelId);
}

ObjectId
LRelIdGetDatabaseId(lRelId)
	LRelId	lRelId;
{
	/*
	 * Note:
	 *	The argument may not be correct, if it is not used soon
	 *	after it is created.
	 */
	return (lRelId.dbId);
}

ObjectId
LRelIdGetRelationId(lRelId)
	LRelId	lRelId;
{
	return (lRelId.relId);
}

bool
DatabaseIdIsMyDatabaseId(databaseId)
	ObjectId	databaseId;
{
	return ((bool)(!ObjectIdIsValid(databaseId) ||
		databaseId == MyDatabaseId));
}

bool
LRelIdContainsMyDatabaseId(lRelId)
	LRelId	lRelId;
{
	return ((bool)(!ObjectIdIsValid(lRelId.dbId) ||
		lRelId.dbId == MyDatabaseId));
}

void
RelationInitLockInfo(relation)
	Relation	relation;
{
	LockInfo	info;
	bool		processingVariable;
	extern ObjectId	MyDatabaseId;		/* XXX use include */
	extern LRelId	LtCreateRelId();	/* XXX ditto */

	Assert(RelationIsValid);
	Assert(ObjectIdIsValid(RelationGetRelationId(relation)));
	/* XXX probably not wanted ... Assert(ObjectIdIsValid(MyDatabaseId)); */

	processingVariable = NameIsEqual(RelationGetRelationName(relation),
		VariableRelationName);
	/*
	 * XXX The LOG relation probably should also be special cased.
	 * XXX We are lucky that it works fine now.
	 */

	info = (LockInfo)relation->lockInfo;

	if (!PointerIsValid((Pointer)info)) {
		info = new(LockInfoData);
	} else if (processingVariable) {
		if (IsTransactionState()) {
			TransactionIdStore(GetCurrentTransactionId(),
				(Pointer)&info->transactionIdData);
		}
		info->flags = 0x0;

		return;		/* prevent an infinite loop--still true? */
	}

	info->lRelId = LtCreateRelId(
		(NameIsSharedSystemRelationName(
			RelationGetRelationName(relation))) ?
		InvalidObjectId : MyDatabaseId,
		RelationGetRelationId(relation));

	if (processingVariable) {

		/* XXX hack to prevent problems during
		 * VARIABLE relation initialization
		 */
		TransactionIdStore(AmiTransactionId,
			(Pointer)&info->transactionIdData);
	} else if (IsTransactionState()) {

		/* XXX check this carefully someday */
		TransactionIdStore(GetCurrentTransactionId(),
			(Pointer)&info->transactionIdData);
	} else {
		PointerStoreInvalidTransactionId(
			(Pointer)&info->transactionIdData);
	}
	info->flags = 0x0;
	info->initialized =
		(bool)(TransactionInitWasProcessed || processingVariable);

	relation->lockInfo = (Pointer)info;
}

void
RelationDiscardLockInfo(relation)
	Relation	relation;
{
	if (!LockInfoIsValid(relation->lockInfo)) {
#ifdef	LOCKDEBUG
		elog(DEBUG, "DiscardLockInfo: NULL relation->lockInfo");
#endif	/* defined(LOCKDEBUG) */
		return;
	}
	delete(relation->lockInfo);	/* XXX pfree((Pointer)relation->lockInfo) */
}

void
RelationSetLockForDescriptorOpen(relation)
	Relation	relation;
{
	LRelId	lRelId;

	Assert(RelationIsValid(relation));

	if (LockingIsDisabled) {
		return;
	}

	lRelId = RelationGetLRelId(relation);

#ifdef	LOCKDEBUGALL
	elog(DEBUG, "RelationSetLockForDescriptorOpen(%s[%d,%d]) called",
		RelationGetRelationName(relation), lRelId.dbId, lRelId.relId);
#endif	/* defined(LOCKDEBUGALL) */

	/* read lock catalog tuples which compose the relation descriptor
	 * XXX race condition?
	 */

	/*
	 * XXX For now, do nothing.
	 */
}

void
RelationSetLockForRead(relation)
	Relation	relation;
{
	LRelId	lRelId;
	int	status;		/* XXX style */
	int16	numberOfFailures;

	Assert(RelationIsValid(relation));

	if (LockingIsDisabled) {
		return;
	}

	lRelId = RelationGetLRelId(relation);

#ifdef	LOCKDEBUG
	elog(DEBUG, "RelationSetLockForRead(%s[%d,%d]) called",
		RelationGetRelationName(relation), lRelId.dbId, lRelId.relId);
#endif	/* defined(LOCKDEBUG) */

	if (((LockInfo)relation->lockInfo)->flags & ReadRelationLock) {
		return;
	}
	((LockInfo)relation->lockInfo)->flags |= ReadRelationLock;

	status = LMLock(MultiLevelLockTableId, LockNoWait, &lRelId,
		(ObjectId *)NULL, (ObjectId *)NULL, GetCurrentTransactionId(),
		MultiLevelLockRequest_ReadRelation);

	numberOfFailures = 0;
	while (status != L_DONE) {
		if (status == L_ERROR) {
			elog(WARN, "RelationSetLockForRead: locking error");
		}

		if (numberOfFailures > MaxRetries) {
			elog(WARN, "RelationSetLockForRead: deadlock");
		}
		numberOfFailures += 1;
		sleep(numberOfFailures);

		elog(NOTICE, "RelationSetLockForRead: retrying");

		status = LMLock(MultiLevelLockTableId, LockTimeout, &lRelId,
			(ObjectId *)NULL, (ObjectId *)NULL,
			GetCurrentTransactionId(),
			MultiLevelLockRequest_ReadRelation);
	}
}

void
RelationSetLockForWrite(relation)
	Relation	relation;
{
	LRelId	lRelId;
	int	status;		/* XXX style */
	int16	numberOfFailures;

	Assert(RelationIsValid(relation));

	if (LockingIsDisabled) {
		return;
	}

	lRelId = RelationGetLRelId(relation);

#ifdef	LOCKDEBUG
	elog(DEBUG, "RelationSetLockForWrite(%s[%d,%d]) called",
		RelationGetRelationName(relation), lRelId.dbId, lRelId.relId);
#endif	/* defined(LOCKDEBUG) */

	if (((LockInfo)relation->lockInfo)->flags & WriteRelationLock) {
		return;
	}
	((LockInfo)relation->lockInfo)->flags |= WriteRelationLock;

	status = LMLock(MultiLevelLockTableId, LockNoWait, &lRelId,
		(ObjectId *)NULL, (ObjectId *)NULL, GetCurrentTransactionId(),
		MultiLevelLockRequest_WriteRelation);

	numberOfFailures = 0;
	while (status != L_DONE) {
		if (status == L_ERROR) {
			elog(WARN, "RelationSetLockForWrite: locking error");
		}

		if (numberOfFailures > MaxRetries) {
			elog(WARN, "RelationSetLockForWrite: deadlock");
		}
		numberOfFailures += 1;
		sleep(numberOfFailures);

		elog(NOTICE, "RelationSetLockForWrite: retrying");

		status = LMLock(MultiLevelLockTableId, LockTimeout, &lRelId,
			(ObjectId *)NULL, (ObjectId *)NULL,
			GetCurrentTransactionId(),
			MultiLevelLockRequest_WriteRelation);
	}
}

void
RelationSetLockForTupleRead(relation, itemPointer)
	Relation	relation;
	ItemPointer	itemPointer;
{
	LRelId		lRelId;
	int		status;		/* XXX style */
	ObjectId	page;		/* XXX should be 6 byte object */
	ObjectId	line;		/* XXX should be 2 byte object */
	LockRequest	lRS[3];
	int	 	requestProcessStatus[3];	/* XXX not used */
	int16	numberOfFailures;

	Assert(RelationIsValid(relation));

	if (LockingIsDisabled) {
		return;
	}

	lRelId = RelationGetLRelId(relation);

#ifdef	LOCKDEBUG
	elog(DEBUG, "RelationSetLockForTupleRead(%s[%d,%d], 0x%x) called",
		RelationGetRelationName(relation), lRelId.dbId, lRelId.relId,
		itemPointer);
#endif	/* defined(LOCKDEBUG) */

	if (((LockInfo)relation->lockInfo)->flags & ReadRelationLock) {
		return;		/* no need to set a lower granularity lock */
	}

	if (!(((LockInfo)relation->lockInfo)->flags & ReadTupleLock)) {

		((LockInfo)relation->lockInfo)->flags |=
			IntentReadRelationLock | IntentReadPageLock |
				ReadTupleLock;

		/* clear count just in case */
		((LockInfo)relation->lockInfo)->flags &=
			~TupleLevelLockCountMask;
	} else {
		if (TupleLevelLockLimit == (TupleLevelLockCountMask &
				((LockInfo)relation->lockInfo)->flags)) {
#ifdef	LOCKDEBUG
			elog(DEBUG, "RelationSetLockForTupleRead() escalating");
#endif	/* defined(LOCKDEBUG) */

			/* escalate */
			RelationSetLockForRead(relation);

			/* clear count just in case */
			((LockInfo)relation->lockInfo)->flags &=
				~TupleLevelLockCountMask;
			return;
		}

		/* increment count */
		((LockInfo)relation->lockInfo)->flags =
			(((LockInfo)relation->lockInfo)->flags &
				~TupleLevelLockCountMask) |
			(1 + (TupleLevelLockCountMask &
				((LockInfo)relation->lockInfo)->flags));
	}

	page = ItemPointerSimpleGetPageNumber(itemPointer);
	line = ItemPointerSimpleGetOffsetNumber(itemPointer);

	lRS[0].mode = LockNoWait | LockUndo;
	lRS[0].rel = &lRelId;
	lRS[0].page = (ObjectId *)NULL;
	lRS[0].line = (ObjectId *)NULL;
	lRS[0].xid = GetCurrentTransactionId();
	lRS[0].lock = MultiLevelLockRequest_IntentReadRelation;

	lRS[1].mode = LockNoWait | LockUndo;
	lRS[1].rel = &lRelId;
	lRS[1].page = &page;
	lRS[1].line = (ObjectId *)NULL;
	lRS[1].xid = lRS[0].xid;
	lRS[1].lock = MultiLevelLockRequest_IntentReadPage;

	lRS[2].mode = LockNoWait;
	lRS[2].rel = &lRelId;
	lRS[2].page = &page;
	lRS[2].line = &line;
	lRS[2].xid = lRS[0].xid;
	lRS[2].lock = MultiLevelLockRequest_ReadLine;

	status = LMLockSet(MultiLevelLockTableId, 3, lRS, requestProcessStatus);

	numberOfFailures = 0;
	while (status != L_DONE) {
		if (status == L_ERROR) {
			elog(WARN,
				"RelationSetLockForTupleRead: locking error");
		}

		if (numberOfFailures > MaxRetries) {
			elog(WARN, "RelationSetLockForTupleRead: deadlock");
		}
		numberOfFailures += 1;
		sleep(numberOfFailures);

		elog(NOTICE, "RelationSetLockForTupleRead: retrying");

		lRS[0].mode = LockTimeout | LockUndo;

		status = LMLockSet(MultiLevelLockTableId, 3, lRS,
			requestProcessStatus);
	}
}

void
RelationSetLockForReadPage(relation, partition, itemPointer)
	Relation	relation;
	PagePartition	partition;
	ItemPointer	itemPointer;
{
	LRelId		lRelId;
	ObjectId	page;		/* XXX should be something else */
	int		status;		/* XXX style */
	BlockNumber	blockIndex;
	PageNumber	pageNumber;
	int16	numberOfFailures;

	Assert(RelationIsValid(relation));

	if (LockingIsDisabled) {
		return;
	}

	lRelId = RelationGetLRelId(relation);

	page = ItemPointerGetLogicalPageNumber(itemPointer, partition);

#ifdef	LOCKDEBUG
	elog(DEBUG, "RelationSetLockForReadPage(%s[%d,%d], %d, @%d) called",
		RelationGetRelationName(relation), lRelId.dbId, lRelId.relId,
		partition, page);
#endif	/* defined(LOCKDEBUG) */

	status = LMLock(PageLockTableId, LockNoWait, &lRelId, &page,
		(ObjectId *)NULL, GetCurrentTransactionId(),
		PageLockRequest_Share);

	numberOfFailures = 0;
	while (status != L_DONE) {
		if (status == L_ERROR) {
			elog(WARN, "RelationSetLockForReadPage: locking error");
		}

		if (numberOfFailures > MaxRetries) {
			elog(WARN, "RelationSetLockForReadPage: deadlock");
		}
		numberOfFailures += 1;
		sleep(numberOfFailures);

		elog(NOTICE, "RelationSetLockForReadPage: retrying");

		status = LMLock(PageLockTableId, LockTimeout, &lRelId, &page,
			(ObjectId *)NULL, GetCurrentTransactionId(),
			PageLockRequest_Share);
	}
}

void
RelationSetLockForWritePage(relation, partition, itemPointer)
	Relation	relation;
	PagePartition	partition;
	ItemPointer	itemPointer;
{
	LRelId		lRelId;
	ObjectId	page;		/* XXX should be something else */
	int		status;		/* XXX style */
	BlockNumber	blockIndex;
	PageNumber	pageNumber;
	int16	numberOfFailures;

	Assert(RelationIsValid(relation));

	if (LockingIsDisabled) {
		return;
	}

	lRelId = RelationGetLRelId(relation);

	page = ItemPointerGetLogicalPageNumber(itemPointer, partition);

#ifdef	LOCKDEBUG
	elog(DEBUG, "RelationSetLockForWritePage(%s[%d,%d], %d, @%d) called",
		RelationGetRelationName(relation), lRelId.dbId, lRelId.relId,
		partition, page);
#endif	/* defined(LOCKDEBUG) */

	status = LMLock(PageLockTableId, LockNoWait, &lRelId, &page,
		(ObjectId *)NULL, GetCurrentTransactionId(),
		PageLockRequest_Exclusive);

	numberOfFailures = 0;
	while (status != L_DONE) {
		if (status == L_ERROR) {
			elog(WARN,
				"RelationSetLockForWritePage: locking error");
		}

		if (numberOfFailures > MaxRetries) {
			elog(WARN, "RelationSetLockForWritePage: deadlock");
		}
		numberOfFailures += 1;
		sleep(numberOfFailures);

		elog(NOTICE, "RelationSetLockForWritePage: retrying");

		status = LMLock(PageLockTableId, LockTimeout, &lRelId, &page,
			(ObjectId *)NULL, GetCurrentTransactionId(),
			PageLockRequest_Exclusive);
	}
}

void
RelationUnsetLockForReadPage(relation, partition, itemPointer)
	Relation	relation;
	PagePartition	partition;
	ItemPointer	itemPointer;
{
	LRelId		lRelId;
	ObjectId	page;		/* XXX should be something else */
	int		status;		/* XXX style */
	BlockNumber	blockIndex;
	PageNumber	pageNumber;

	Assert(RelationIsValid(relation));

	if (LockingIsDisabled) {
		return;
	}

	lRelId = RelationGetLRelId(relation);

	page = ItemPointerGetLogicalPageNumber(itemPointer, partition);

#ifdef	LOCKDEBUG
	elog(DEBUG, "RelationUnsetLockForReadPage(%s[%d,%d], %d, %d) called",
		RelationGetRelationName(relation), lRelId.dbId, lRelId.relId,
		partition, page);
#endif	/* defined(LOCKDEBUG) */

	status = LMLock(PageLockTableId, LockUnlock, &lRelId, &page,
		(ObjectId *)NULL, GetCurrentTransactionId(),
		PageLockRequest_Share);

	while (status != L_DONE) {
		if (status == L_ERROR) {
			elog(WARN,
				"RelationUnsetLockForReadPage: locking error");
		}
		elog(FATAL, "RelationUnsetLockForReadPage: failed");
	}
}

void
RelationUnsetLockForWritePage(relation, partition, itemPointer)
	Relation	relation;
	PagePartition	partition;
	ItemPointer	itemPointer;
{
	LRelId		lRelId;
	ObjectId	page;		/* XXX should be something else */
	int		status;		/* XXX style */
	BlockNumber	blockIndex;
	PageNumber	pageNumber;

	Assert(RelationIsValid(relation));

	if (LockingIsDisabled) {
		return;
	}

	lRelId = RelationGetLRelId(relation);

	page = ItemPointerGetLogicalPageNumber(itemPointer, partition);

#ifdef	LOCKDEBUG
	elog(DEBUG, "RelationUnsetLockForWritePage(%s[%d,%d], %d, @%d) called",
		RelationGetRelationName(relation), lRelId.dbId, lRelId.relId,
		partition, page);
#endif	/* defined(LOCKDEBUG) */

	status = LMLock(PageLockTableId, LockUnlock, &lRelId, &page,
		(ObjectId *)NULL, GetCurrentTransactionId(),
		PageLockRequest_Exclusive);

	if (status != L_DONE) {
		if (status == L_ERROR) {
			elog(WARN,
				"RelationUnsetLockForWritePage: locking error");
		}
		elog(FATAL, "RelationUnsetLockForWritePage: failed");
	}
}

static
bool
LockInfoIsValid(info)
	LockInfo	info;
{
	if (!PointerIsValid(info)) {
		return (false);
	}
	if (!TransactionInitWasProcessed) {
		return (true);
	}
	if (!info->initialized) {
		return (false);
	}

	return (TransactionIdEquals(GetCurrentTransactionId(),
		&info->transactionIdData));
}
