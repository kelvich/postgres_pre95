/* ----------------------------------------------------------------
 *   FILE
 *	lmgr.c
 *	
 *   DESCRIPTION
 *	POSTGRES lock manager code
 *
 *   INTERFACE ROUTINES
 *
 *   NOTES
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

/* #define LOCKDEBUGALL	1 */
/* #define LOCKDEBUG	1 */

#ifdef	LOCKDEBUGALL
#define LOCKDEBUG	1
#endif  LOCKDEBUGALL

#include "tmp/postgres.h"
 RcsId("$Header$");

#include "access/heapam.h"
#include "access/htup.h"
#include "access/relscan.h"
#include "access/skey.h"
#include "access/tqual.h"
#include "access/xact.h"

#include "storage/block.h"
#include "storage/buf.h"
#include "storage/ipci.h"	/* PageLockTableId and MultiLevelLockTableId */
#include "storage/itemptr.h"
#include "storage/bufpage.h"
#include "utils/lmgr.h"
#include "storage/pagenum.h"
#include "storage/part.h"
#include "storage/pladt.h"
#include "storage/plm.h"	/* L_DONE, etc. */

#include "utils/log.h"
#include "utils/rel.h"

#include "catalog/catname.h"
#include "catalog/pg_relation.h"

/* ----------------
 *	
 * ----------------
 */
#define MaxRetries	10	/* XXX about a minute--a hack */

#define IntentReadRelationLock	0x0100
#define ReadRelationLock	0x0200
#define IntentWriteRelationLock	0x0400
#define WriteRelationLock	0x0800
#define IntentReadPageLock	0x1000
#define ReadTupleLock		0x2000

#define TupleLevelLockCountMask	0x000f

#define TupleLevelLockLimit	10

extern bool	TransactionInitWasProcessed;	/* XXX style */
extern ObjectId	MyDatabaseId;
bool   LockingIsDisabled = false;	/* XXX should be static, fix cinit */

static LRelId	VariableRelationLRelId =
{ VariableRelationId, InvalidObjectId };

/* ----------------
 * LockInfoIsValid --
 *	True iff lock information is valid.
 * ----------------
 */
static bool
LockInfoIsValid ARGS((
		      LockInfo	info
		      ));

    
/* ----------------
 *	RelationGetLRelId
 * ----------------
 */
#ifdef	LOCKDEBUG
#define LOCKDEBUG_10 \
    elog(NOTICE, "RelationGetLRelId(%s) invalid lockInfo", \
	 RelationGetRelationName(relation));
#else
#define LOCKDEBUG_10
#endif	LOCKDEBUG
    
LRelId
RelationGetLRelId(relation)
    Relation	relation;
{
    LockInfo	linfo;
    
    /* ----------------
     *	sanity checks
     * ----------------
     */
    Assert(RelationIsValid(relation));
    linfo = (LockInfo) relation->lockInfo;

    /* ----------------
     *	initialize lock info if necessary
     * ----------------
     */
    if (! LockInfoIsValid(linfo)) {
	LOCKDEBUG_10;
	RelationInitLockInfo(relation);
	linfo = (LockInfo) relation->lockInfo;
    }
    
    /* ----------------
     * XXX hack to prevent problems during
     * VARIABLE relation initialization
     * ----------------
     */
    if (NameIsEqual(RelationGetRelationName(relation),
		    VariableRelationName)) {
	return (VariableRelationLRelId);
    }
    
    return (linfo->lRelId);
}

/* ----------------
 *	LRelIdGetDatabaseId
 *
 * Note: The argument may not be correct, if it is not used soon
 *	 after it is created.
 * ----------------
 */
ObjectId
LRelIdGetDatabaseId(lRelId)
    LRelId	lRelId;
{
    return (lRelId.dbId);
}


/* ----------------
 *	LRelIdGetRelationId
 * ----------------
 */
ObjectId
LRelIdGetRelationId(lRelId)
    LRelId	lRelId;
{
    return (lRelId.relId);
}

/* ----------------
 *	DatabaseIdIsMyDatabaseId
 * ----------------
 */
bool
DatabaseIdIsMyDatabaseId(databaseId)
    ObjectId	databaseId;
{
    return (bool)
	(!ObjectIdIsValid(databaseId) || databaseId == MyDatabaseId);
}

/* ----------------
 *	LRelIdContainsMyDatabaseId
 * ----------------
 */
bool
LRelIdContainsMyDatabaseId(lRelId)
    LRelId	lRelId;
{
    return (bool)
	(!ObjectIdIsValid(lRelId.dbId) || lRelId.dbId == MyDatabaseId);
}

/* ----------------
 *	RelationInitLockInfo
 *
 * 	XXX processingVariable is a hack to prevent problems during
 * 	VARIABLE relation initialization.
 * ----------------
 */
void
RelationInitLockInfo(relation)
    Relation	relation;
{
    LockInfo		info;
    char 		*relname;
    ObjectId		relationid;
    bool		processingVariable;
    extern ObjectId	MyDatabaseId;		/* XXX use include */
    extern LRelId	LtCreateRelId();	/* XXX ditto */
    
    /* ----------------
     *	sanity checks
     * ----------------
     */
    Assert(RelationIsValid(relation));
    Assert(ObjectIdIsValid(RelationGetRelationId(relation)));

    /* ----------------
     *	get information from relation descriptor
     * ----------------
     */
    info = (LockInfo) relation->lockInfo;
    relname = (char *) RelationGetRelationName(relation);
    relationid = RelationGetRelationId(relation);
    processingVariable = NameIsEqual(relname, VariableRelationName);
    
    /* ----------------
     *	create a new lockinfo if not already done
     * ----------------
     */
    if (! PointerIsValid(info)) 
	info = new(LockInfoData);
    else if (processingVariable) {
	if (IsTransactionState()) {
	    TransactionIdStore(GetCurrentTransactionId(),
			       (Pointer) &info->transactionIdData);
	}
	info->flags = 0x0;
	return;		/* prevent an infinite loop--still true? */
    }

    /* ----------------
     *	initialize lockinfo.dbId and .relId appropriately
     * ----------------
     */
    if (NameIsSharedSystemRelationName(relname))
	info->lRelId = LtCreateRelId(InvalidObjectId, relationid);
    else
	info->lRelId = LtCreateRelId(MyDatabaseId, relationid);
    
    /* ----------------
     *	store the transaction id in the lockInfo field
     * ----------------
     */
    if (processingVariable)
	TransactionIdStore(AmiTransactionId,
			   (Pointer)&info->transactionIdData);
    else if (IsTransactionState()) 
	TransactionIdStore(GetCurrentTransactionId(),
			   (Pointer) &info->transactionIdData);
    else
	PointerStoreInvalidTransactionId(&info->transactionIdData);
	
    /* ----------------
     *	initialize rest of lockinfo
     * ----------------
     */
    info->flags = 0x0;
    info->initialized =	(bool)
	TransactionInitWasProcessed || processingVariable;
    relation->lockInfo = (Pointer) info;
}

/* ----------------
 *	RelationDiscardLockInfo
 * ----------------
 */
#ifdef	LOCKDEBUG
#define LOCKDEBUG_20 \
    elog(DEBUG, "DiscardLockInfo: NULL relation->lockInfo")
#else
#define LOCKDEBUG_20
#endif	LOCKDEBUG
   
void
RelationDiscardLockInfo(relation)
    Relation	relation;
{
    if (! LockInfoIsValid(relation->lockInfo)) {
	LOCKDEBUG_20;
	return;
    }
    
    delete(relation->lockInfo);	/* XXX pfree((Pointer)relation->lockInfo) */
}

/* ----------------
 *	RelationSetLockForDescriptorOpen
 * ----------------
 */
#ifdef	LOCKDEBUGALL
#define LOCKDEBUGALL_30 \
    elog(DEBUG, "RelationSetLockForDescriptorOpen(%s[%d,%d]) called", \
	 RelationGetRelationName(relation), lRelId.dbId, lRelId.relId)
#else
#define LOCKDEBUGALL_30
#endif	LOCKDEBUGALL

void
RelationSetLockForDescriptorOpen(relation)
    Relation	relation;
{
    LRelId	lRelId;
    
    /* ----------------
     *	sanity checks
     * ----------------
     */
    Assert(RelationIsValid(relation));
    if (LockingIsDisabled)
	return;

    lRelId = RelationGetLRelId(relation);
    LOCKDEBUGALL_30;

    /* ----------------
     * read lock catalog tuples which compose the relation descriptor
     * XXX race condition? XXX For now, do nothing.
     * ----------------
     */
}

/* ----------------
 *	RelationSetLockForRead
 * ----------------
 */
#ifdef	LOCKDEBUG
#define LOCKDEBUG_40 \
    elog(DEBUG, "RelationSetLockForRead(%s[%d,%d]) called", \
	 RelationGetRelationName(relation), lRelId.dbId, lRelId.relId)
#else
#define LOCKDEBUG_40
#endif	LOCKDEBUG
   
void
RelationSetLockForRead(relation)
    Relation	relation;
{
    LRelId	lRelId;
    int		status;		/* XXX style */
    int16	numberOfFailures;
    LockInfo	linfo;

    /* ----------------
     *	sanity checks
     * ----------------
     */
    Assert(RelationIsValid(relation));
    if (LockingIsDisabled) 
	return;
    
    lRelId = RelationGetLRelId(relation);
    linfo = (LockInfo) relation->lockInfo;
    LOCKDEBUG_40;

    /* ----------------
     *	return if lock already set, otherwise set lock.
     * ----------------
     */
    if (linfo->flags & ReadRelationLock)
	return;
    linfo->flags |= ReadRelationLock;
    
    status = LMLock(MultiLevelLockTableId,
		    LockNoWait,
		    &lRelId,
		    (ObjectId *)NULL,
		    (ObjectId *)NULL,
		    GetCurrentTransactionId(),
		    MultiLevelLockRequest_ReadRelation);

    /* ----------------
     *	if we couldn't get the lock we wanted, we sleep and
     *  try again, sleep more and try again, until hopefully
     *  we get the lock.  This scheme is pretty pathetic.
     * ----------------
     */
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
	
	status = LMLock(MultiLevelLockTableId,
			LockTimeout,
			&lRelId,
			(ObjectId *)NULL,
			(ObjectId *)NULL,
			GetCurrentTransactionId(),
			MultiLevelLockRequest_ReadRelation);
    }
}

/* ----------------
 *	RelationUnsetLockForRead
 * ----------------
 */
#ifdef	LOCKDEBUG
#define LOCKDEBUG_50 \
    elog(DEBUG, "RelationUnsetLockForRead(%s[%d,%d]) called", \
	 RelationGetRelationName(relation), lRelId.dbId, lRelId.relId)
#else
#define LOCKDEBUG_50
#endif	LOCKDEBUG

void
RelationUnsetLockForRead(relation)
    Relation relation;
{
    LRelId	lRelId;
    int		status;		/* XXX style */
    
    /* ----------------
     *	sanity checks
     * ----------------
     */
    Assert(RelationIsValid(relation));
    if (LockingIsDisabled)
	return;

    /* ----------------
     *	attempt to remove our read lock
     * ----------------
     */
    lRelId = RelationGetLRelId(relation);
    LOCKDEBUG_50;

    status = LMLock(MultiLevelLockTableId,
		    LockUnlock,
		    &lRelId,
		    (ObjectId *)NULL,
		    (ObjectId *)NULL,
		    GetCurrentTransactionId(),
		    MultiLevelLockRequest_ReadRelation);
    
    /* ----------------
     *	die horribly if we failed.  This should be fixed. XXX
     * ----------------
     */
    if (status != L_DONE) {
	if (status == L_ERROR) {
	    elog(WARN, "RelationUnsetLockForRead(%s[%d,%d]: error",
		 RelationGetRelationName(relation),
		 lRelId.dbId, lRelId.relId);
	}
	elog(NOTICE, "RelationUnsetLockForRead(%s[%d,%d]: failed",
	     RelationGetRelationName(relation),
	     lRelId.dbId, lRelId.relId);
    }
}

/* ----------------
 *	RelationSetLockForWrite(relation)
 * ----------------
 */
#ifdef	LOCKDEBUG
#define LOCKDEBUG_60 \
    elog(DEBUG, "RelationSetLockForWrite(%s[%d,%d]) called", \
	 RelationGetRelationName(relation), lRelId.dbId, lRelId.relId)
#else
#define LOCKDEBUG_60
#endif	LOCKDEBUG

void
RelationSetLockForWrite(relation)
    Relation	relation;
{
    LRelId	lRelId;
    int		status;
    int16	numberOfFailures;
    LockInfo	linfo;
    
    /* ----------------
     *	sanity checks
     * ----------------
     */
    Assert(RelationIsValid(relation));
    if (LockingIsDisabled)
	return;
    
    lRelId = RelationGetLRelId(relation);
    linfo = (LockInfo) relation->lockInfo;
    LOCKDEBUG_60;
    
    /* ----------------
     *	return if lock already set, otherwise set lock.
     * ----------------
     */
    if (linfo->flags & WriteRelationLock)
	return;
    linfo->flags |= WriteRelationLock;
    
    status = LMLock(MultiLevelLockTableId,
		    LockNoWait,
		    &lRelId,
		    (ObjectId *)NULL,
		    (ObjectId *)NULL,
		    GetCurrentTransactionId(),
		    MultiLevelLockRequest_WriteRelation);
    
    /* ----------------
     *	if we couldn't get the lock we wanted, we sleep and
     *  try again, sleep more and try again, until hopefully
     *  we get the lock.  This scheme is pretty pathetic.
     * ----------------
     */
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
	
	status = LMLock(MultiLevelLockTableId,
			LockTimeout,
			&lRelId,
			(ObjectId *)NULL,
			(ObjectId *)NULL,
			GetCurrentTransactionId(),
			MultiLevelLockRequest_WriteRelation);
    }
}

/* ----------------
 *	RelationUnsetLockForWrite
 * ----------------
 */
#ifdef	LOCKDEBUG
#define LOCKDEBUG_70 \
    elog(DEBUG, "RelationUnsetLockForWrite(%s[%d,%d]) called", \
	 RelationGetRelationName(relation), lRelId.dbId, lRelId.relId);
#else
#define LOCKDEBUG_70
#endif	LOCKDEBUG

void
RelationUnsetLockForWrite(relation)
    Relation relation;
{
    LRelId	lRelId;
    int		status;		/* XXX style */
    
    /* ----------------
     *	sanity checks
     * ----------------
     */
    Assert(RelationIsValid(relation));
    if (LockingIsDisabled)
	return;
    
    /* ----------------
     *	attempt to remove our write lock
     * ----------------
     */
    lRelId = RelationGetLRelId(relation);
    LOCKDEBUG_70;
    
    status = LMLock(MultiLevelLockTableId,
		    LockUnlock,
		    &lRelId,
		    (ObjectId *)NULL,
		    (ObjectId *)NULL,
		    GetCurrentTransactionId(),
		    MultiLevelLockRequest_WriteRelation);
    
    /* ----------------
     *	die horribly if we failed.  This should be fixed. XXX
     * ----------------
     */
    if (status != L_DONE) {
	if (status == L_ERROR) {
	    elog(WARN, "RelationUnsetLockForWrite(%s[%d,%d]: error",
		 RelationGetRelationName(relation),
		 lRelId.dbId, lRelId.relId);
	}
	elog(FATAL, "RelationUnsetLockForWrite(%s[%d,%d]: failed",
	     RelationGetRelationName(relation),
	     lRelId.dbId, lRelId.relId);
    }
}

/* ----------------
 *	RelationSetLockForTupleRead
 * ----------------
 */
#ifdef	LOCKDEBUG
#define LOCKDEBUG_80 \
    elog(DEBUG, "RelationSetLockForTupleRead(%s[%d,%d], 0x%x) called", \
	 RelationGetRelationName(relation), lRelId.dbId, lRelId.relId, \
	 itemPointer)
#define LOCKDEBUG_81 \
    elog(DEBUG, "RelationSetLockForTupleRead() escalating");
#else
#define LOCKDEBUG_80
#define LOCKDEBUG_81
#endif	LOCKDEBUG

void
RelationSetLockForTupleRead(relation, itemPointer)
    Relation	relation;
    ItemPointer	itemPointer;
{
    LRelId	lRelId;
    int		status;		/* XXX style */
    ObjectId	page;		/* XXX should be 6 byte object */
    ObjectId	line;		/* XXX should be 2 byte object */
    LockRequest	lRS[3];
    int	 	requestProcessStatus[3];	/* XXX not used */
    int16	numberOfFailures;
    LockInfo	linfo;
    
    /* ----------------
     *	sanity checks
     * ----------------
     */
    Assert(RelationIsValid(relation));
    if (LockingIsDisabled) 
	return;

    lRelId = RelationGetLRelId(relation);
    linfo = (LockInfo) relation->lockInfo;
    LOCKDEBUG_80;
    
    /* ----------------
     *	no need to set a lower granularity lock
     * ----------------
     */
    if (linfo->flags & ReadRelationLock)
	return;		
    
    /* ----------------
     *	
     * ----------------
     */
    if (! (linfo->flags & ReadTupleLock)) {
	
	linfo->flags |=
	        IntentReadRelationLock |
		IntentReadPageLock |
		ReadTupleLock;
	
	/* clear count just in case */
	linfo->flags &= ~TupleLevelLockCountMask;
	
    } else {
	if (TupleLevelLockLimit == (TupleLevelLockCountMask &
				    linfo->flags)) {
	    LOCKDEBUG_81;
	    
	    /* escalate */
	    RelationSetLockForRead(relation);
	    
	    /* clear count just in case */
	    linfo->flags &= ~TupleLevelLockCountMask;
	    return;
	}
	
	/* increment count */
	linfo->flags =
	    (linfo->flags & ~TupleLevelLockCountMask) |
		(1 + (TupleLevelLockCountMask & linfo->flags));
    }

    /* ----------------
     *	
     * ----------------
     */
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
    
    status = LMLockSet(MultiLevelLockTableId,
		       3,
		       lRS,
		       requestProcessStatus);

    /* ----------------
     *	
     * ----------------
     */
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
	
	status = LMLockSet(MultiLevelLockTableId,
			   3,
			   lRS,
			   requestProcessStatus);
    }
}

/* ----------------
 *	RelationSetLockForReadPage
 * ----------------
 */
#ifdef	LOCKDEBUG
#define LOCKDEBUG_90 \
    elog(DEBUG, "RelationSetLockForReadPage(%s[%d,%d], %d, @%d) called", \
	 RelationGetRelationName(relation), lRelId.dbId, lRelId.relId, \
	 partition, page);
#else
#define LOCKDEBUG_90
#endif	LOCKDEBUG

void
RelationSetLockForReadPage(relation, partition, itemPointer)
    Relation		relation;
    PagePartition	partition;
    ItemPointer		itemPointer;
{
    LRelId		lRelId;
    ObjectId		page;		/* XXX should be something else */
    int			status;		/* XXX style */
    BlockNumber		blockIndex;
    PageNumber		pageNumber;
    int16		numberOfFailures;
    
    /* ----------------
     *	sanity check
     * ----------------
     */
    Assert(RelationIsValid(relation));
    if (LockingIsDisabled)
	return;
    
    /* ----------------
     *	attempt to set lock
     * ----------------
     */
    lRelId = RelationGetLRelId(relation);
    page = ItemPointerGetLogicalPageNumber(itemPointer, partition);
    LOCKDEBUG_90;
    
    status = LMLock(PageLockTableId,
		    LockNoWait,
		    &lRelId,
		    &page,
		    (ObjectId *)NULL,
		    GetCurrentTransactionId(),
		    PageLockRequest_Share);

    /* ----------------
     *	if we couldn't get the lock we wanted, we sleep and
     *  try again, sleep more and try again, until hopefully
     *  we get the lock.  This scheme is pretty pathetic.
     * ----------------
     */
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
	
	status = LMLock(PageLockTableId,
			LockTimeout,
			&lRelId,
			&page,
			(ObjectId *)NULL,
			GetCurrentTransactionId(),
			PageLockRequest_Share);
    }
}

/* ----------------
 *	RelationSetLockForWritePage
 * ----------------
 */
#ifdef	LOCKDEBUG
#define LOCKDEBUG_100 \
    elog(DEBUG, "RelationSetLockForWritePage(%s[%d,%d], %d, @%d) called", \
	 RelationGetRelationName(relation), lRelId.dbId, lRelId.relId, \
	 partition, page);
#else
#define LOCKDEBUG_100
#endif	LOCKDEBUG

void
RelationSetLockForWritePage(relation, partition, itemPointer)
    Relation		relation;
    PagePartition	partition;
    ItemPointer		itemPointer;
{
    LRelId		lRelId;
    ObjectId	page;		/* XXX should be something else */
    int		status;		/* XXX style */
    BlockNumber	blockIndex;
    PageNumber	pageNumber;
    int16	numberOfFailures;

    /* ----------------
     *	sanity checks
     * ----------------
     */
    Assert(RelationIsValid(relation));
    if (LockingIsDisabled)
	return;

    /* ----------------
     *	attempt to set lock
     * ----------------
     */
    lRelId = RelationGetLRelId(relation);
    page = ItemPointerGetLogicalPageNumber(itemPointer, partition);
    LOCKDEBUG_100;
    
    status = LMLock(PageLockTableId,
		    LockNoWait,
		    &lRelId,
		    &page,
		    (ObjectId *)NULL,
		    GetCurrentTransactionId(),
		    PageLockRequest_Exclusive);

    /* ----------------
     *	if we couldn't get the lock we wanted, we sleep and
     *  try again, sleep more and try again, until hopefully
     *  we get the lock.  This scheme is pretty pathetic.
     * ----------------
     */
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
	
	status = LMLock(PageLockTableId,
			LockTimeout,
			&lRelId,
			&page,
			(ObjectId *)NULL,
			GetCurrentTransactionId(),
			PageLockRequest_Exclusive);
    }
}

/* ----------------
 *	RelationUnsetLockForReadPage
 * ----------------
 */
#ifdef	LOCKDEBUG
#define LOCKDEBUG_110 \
    elog(DEBUG, "RelationUnsetLockForReadPage(%s[%d,%d], %d, %d) called", \
	 RelationGetRelationName(relation), lRelId.dbId, lRelId.relId, \
	 partition, page)
#else
#define LOCKDEBUG_110
#endif	LOCKDEBUG

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

    /* ----------------
     *	sanity checks
     * ----------------
     */
    Assert(RelationIsValid(relation));
    if (LockingIsDisabled)
	return;

    /* ----------------
     *	attempt to remove our lock
     * ----------------
     */
    lRelId = RelationGetLRelId(relation);
    page = ItemPointerGetLogicalPageNumber(itemPointer, partition);
    LOCKDEBUG_110;
    
    status = LMLock(PageLockTableId,
		    LockUnlock,
		    &lRelId,
		    &page,
		    (ObjectId *)NULL,
		    GetCurrentTransactionId(),
		    PageLockRequest_Share);

    /* ----------------
     *	die horribly if we failed.  This should be fixed. XXX
     * ----------------
     */
    if (status != L_DONE) {
	if (status == L_ERROR) {
	    elog(WARN,
		 "RelationUnsetLockForReadPage: locking error");
	}
	elog(NOTICE, "RelationUnsetLockForReadPage: failed");
    }
}

/* ----------------
 *	RelationUnsetLockForWritePage
 * ----------------
 */
#ifdef	LOCKDEBUG
#define LOCKDEBUG_120 \
    elog(DEBUG, "RelationUnsetLockForWritePage(%s[%d,%d], %d, @%d) called", \
	 RelationGetRelationName(relation), lRelId.dbId, lRelId.relId, \
	 partition, page)
#else
#define LOCKDEBUG_120
#endif	LOCKDEBUG

void
RelationUnsetLockForWritePage(relation, partition, itemPointer)
    Relation		relation;
    PagePartition	partition;
    ItemPointer		itemPointer;
{
    LRelId		lRelId;
    ObjectId	page;		/* XXX should be something else */
    int		status;		/* XXX style */
    BlockNumber	blockIndex;
    PageNumber	pageNumber;

    /* ----------------
     *	sanity checks
     * ----------------
     */
    Assert(RelationIsValid(relation));
    if (LockingIsDisabled)
	return;
    
    /* ----------------
     *	attempt to remove our lock
     * ----------------
     */
    lRelId = RelationGetLRelId(relation);
    page = ItemPointerGetLogicalPageNumber(itemPointer, partition);
    LOCKDEBUG_120;
    
    status = LMLock(PageLockTableId,
		    LockUnlock,
		    &lRelId,
		    &page,
		    (ObjectId *)NULL,
		    GetCurrentTransactionId(),
		    PageLockRequest_Exclusive);
    
    /* ----------------
     *	die horribly if we failed.  This should be fixed. XXX
     * ----------------
     */
    if (status != L_DONE) {
	if (status == L_ERROR) {
	    elog(WARN,
		 "RelationUnsetLockForWritePage: locking error");
	}
	elog(FATAL, "RelationUnsetLockForWritePage: failed");
    }
}

/* ----------------
 *	LockInfoIsValid
 * ----------------
 */
static bool
LockInfoIsValid(info)
    LockInfo	info;
{
    if (! PointerIsValid(info)) 
	return false;
    
    if (! TransactionInitWasProcessed) 
	return true;
    
    if (! info->initialized) 
	return false;
    
    return (bool)
	TransactionIdEquals(GetCurrentTransactionId(),
			    &info->transactionIdData);
}
