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
#include "storage/itemptr.h"
#include "storage/bufpage.h"
#include "storage/pagenum.h"
#include "storage/part.h"
#include "storage/multilev.h"
#include "storage/lmgr.h"

#include "utils/log.h"
#include "utils/rel.h"

#include "catalog/catname.h"
#include "catalog/pg_relation.h"

#include "nodes/mnodes.h"
/* ----------------
 *	
 * ----------------
 */
#define MaxRetries	4	/* XXX about 1/4 minute--a hack */

#define IntentReadRelationLock	0x0100
#define ReadRelationLock	0x0200
#define IntentWriteRelationLock	0x0400
#define WriteRelationLock	0x0800
#define IntentReadPageLock	0x1000
#define ReadTupleLock		0x2000

#define TupleLevelLockCountMask	0x000f

#define TupleLevelLockLimit	10

extern ObjectId	MyDatabaseId;

static LRelId	VariableRelationLRelId =
{ VariableRelationId, InvalidObjectId };

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
    extern GlobalMemory CacheCxt;
    
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
    processingVariable = NameIsEqual((Name) relname, VariableRelationName);
    
    /* ----------------
     *	create a new lockinfo if not already done
     * ----------------
     */
    if (! PointerIsValid(info)) 
    {
	MemoryContext oldcxt;

	oldcxt = MemoryContextSwitchTo((MemoryContext)CacheCxt);
	info = (LockInfo)palloc(sizeof(LockInfoData));
	MemoryContextSwitchTo(oldcxt);
    }
    else if (processingVariable) {
	if (IsTransactionState()) {
	    TransactionIdStore(GetCurrentTransactionId(),
			       &info->transactionIdData);
	}
	info->flags = 0x0;
	return;		/* prevent an infinite loop--still true? */
    }
    else if (info->initialized)
    {
        /* ------------
         *  If we've already initialized we're done.
         * ------------
         */
	return;
    }

    /* ----------------
     *	initialize lockinfo.dbId and .relId appropriately
     * ----------------
     */
    if (NameIsSharedSystemRelationName((Name) relname))
	LRelIdAssign(&info->lRelId, InvalidObjectId, relationid);
    else
	LRelIdAssign(&info->lRelId, MyDatabaseId, relationid);
    
    /* ----------------
     *	store the transaction id in the lockInfo field
     * ----------------
     */
    if (processingVariable)
	TransactionIdStore(AmiTransactionId,
			   &info->transactionIdData);
    else if (IsTransactionState()) 
	TransactionIdStore(GetCurrentTransactionId(),
			   &info->transactionIdData);
    else
	PointerStoreInvalidTransactionId((Pointer)&(info->transactionIdData));
	
    /* ----------------
     *	initialize rest of lockinfo
     * ----------------
     */
    info->flags = 0x0;
    info->initialized =	(bool)true;
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
    /* ----------------
     *	sanity checks
     * ----------------
     */
    Assert(RelationIsValid(relation));
    if (LockingDisabled())
	return;

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
    LockInfo	linfo;
    TransactionId curXact;

    /* ----------------
     *	sanity checks
     * ----------------
     */
    Assert(RelationIsValid(relation));
    if (LockingDisabled())
	return;
    
    LOCKDEBUG_40;

    /* ----------------
     * If we don't have lock info on the reln just go ahead and
     * lock it without trying to short circuit the lock manager.
     * ----------------
     */
    if (!LockInfoIsValid(relation->lockInfo))
    {
	RelationInitLockInfo(relation);
	linfo = (LockInfo) relation->lockInfo;
	linfo->flags |= ReadRelationLock;
        MultiLockReln(linfo, READ_LOCK);
	return;
    }
    else
        linfo = (LockInfo) relation->lockInfo;

    MultiLockReln(linfo, READ_LOCK);
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
    LockInfo	linfo;

    /* ----------------
     *	sanity check
     * ----------------
     */
    Assert(RelationIsValid(relation));
    if (LockingDisabled())
	return;
    
    linfo = (LockInfo) relation->lockInfo;

    /* ----------------
     * If we don't have lock info on the reln just go ahead and
     * release it.
     * ----------------
     */
    if (!LockInfoIsValid(linfo))
    {
        elog(WARN, 
            "Releasing a lock on %s with invalid lock information",
            RelationGetRelationName(relation));
    }

    MultiReleaseReln(linfo, READ_LOCK);
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
    LockInfo	linfo;
    TransactionId curXact;
    
    /* ----------------
     *	sanity checks
     * ----------------
     */
    Assert(RelationIsValid(relation));
    if (LockingDisabled())
	return;
    
    LOCKDEBUG_60;
    
    /* ----------------
     * If we don't have lock info on the reln just go ahead and
     * lock it without trying to short circuit the lock manager.
     * ----------------
     */
    if (!LockInfoIsValid(relation->lockInfo))
    {
	RelationInitLockInfo(relation);
        linfo = (LockInfo) relation->lockInfo;
	linfo->flags |= WriteRelationLock;
        MultiLockReln(linfo, WRITE_LOCK);
	return;
    }
    else
        linfo = (LockInfo) relation->lockInfo;

    MultiLockReln(linfo, WRITE_LOCK);
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
    LockInfo	linfo;

    /* ----------------
     *	sanity checks
     * ----------------
     */
    Assert(RelationIsValid(relation));
    if (LockingDisabled()) {
	return;
    }

    linfo = (LockInfo) relation->lockInfo;

    if (!LockInfoIsValid(linfo))
    {
        elog(WARN, 
            "Releasing a lock on %s with invalid lock information",
            RelationGetRelationName(relation));
    }

    MultiReleaseReln(linfo, WRITE_LOCK);
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
    LockInfo	linfo;
    TransactionId curXact;
    
    /* ----------------
     *	sanity checks
     * ----------------
     */
    Assert(RelationIsValid(relation));
    if (LockingDisabled())
	return;

    LOCKDEBUG_80;
    
    /* ---------------------
     * If our lock info is invalid don't bother trying to short circuit
     * the lock manager.
     * ---------------------
     */
    if (!LockInfoIsValid(relation->lockInfo))
    {
	RelationInitLockInfo(relation);
	linfo = (LockInfo) relation->lockInfo;
	linfo->flags |=
                IntentReadRelationLock |
		IntentReadPageLock |
		ReadTupleLock;
        MultiLockTuple(linfo, itemPointer, READ_LOCK);
	return;
    }
    else
        linfo = (LockInfo) relation->lockInfo;

    /* ----------------
     *	no need to set a lower granularity lock
     * ----------------
     */
    curXact = GetCurrentTransactionId();
    if ((linfo->flags & ReadRelationLock) &&
	TransactionIdEquals(curXact, linfo->transactionIdData))
    {
	return;
    }
    
    /* ----------------
     * If we don't already have a tuple lock this transaction
     * ----------------
     */
    if (!( (linfo->flags & ReadTupleLock) &&
           TransactionIdEquals(curXact, linfo->transactionIdData) )) {
	
	linfo->flags |=
	        IntentReadRelationLock |
		IntentReadPageLock |
		ReadTupleLock;
	
	/* clear count */
	linfo->flags &= ~TupleLevelLockCountMask;
	
    } else {
	if (TupleLevelLockLimit == (TupleLevelLockCountMask &
				    linfo->flags)) {
	    LOCKDEBUG_81;
	    
	    /* escalate */
	    MultiLockReln(linfo, READ_LOCK);
	    
	    /* clear count */
	    linfo->flags &= ~TupleLevelLockCountMask;
	    return;
	}
	
	/* increment count */
	linfo->flags =
	    (linfo->flags & ~TupleLevelLockCountMask) |
		(1 + (TupleLevelLockCountMask & linfo->flags));
    }

    TransactionIdStore(curXact, &linfo->transactionIdData);

    /* ----------------
     * Lock the tuple.
     * ----------------
     */
    MultiLockTuple(linfo, itemPointer, READ_LOCK);
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
    PagePartition	partition;	/* XXX not used */
    ItemPointer		itemPointer;
{
    /* ----------------
     *	sanity check
     * ----------------
     */
    Assert(RelationIsValid(relation));
    if (LockingDisabled())
	return;
    
    /* ---------------
     * Make sure linfo is initialized
     * ---------------
     */
    if (!LockInfoIsValid(relation->lockInfo))
	RelationInitLockInfo(relation);

    /* ----------------
     *	attempt to set lock
     * ----------------
     */
    MultiLockPage((LockInfo) (relation->lockInfo), itemPointer, READ_LOCK);
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
    PagePartition	partition;	/* XXX not used */
    ItemPointer		itemPointer;
{
    /* ----------------
     *	sanity checks
     * ----------------
     */
    Assert(RelationIsValid(relation));
    if (LockingDisabled())
	return;

    /* ---------------
     * Make sure linfo is initialized
     * ---------------
     */
    if (!LockInfoIsValid(relation->lockInfo))
	RelationInitLockInfo(relation);

    /* ----------------
     *	attempt to set lock
     * ----------------
     */
    MultiLockPage((LockInfo) relation->lockInfo, itemPointer, WRITE_LOCK);
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
    /* ----------------
     *	sanity checks
     * ----------------
     */
    Assert(RelationIsValid(relation));
    if (LockingDisabled())
	return;

    if (!LockInfoIsValid(relation->lockInfo))
        elog(WARN, 
            "Releasing a lock on %s with invalid lock information",
            RelationGetRelationName(relation));

    MultiReleasePage((LockInfo) relation->lockInfo, itemPointer, READ_LOCK);
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
    /* ----------------
     *	sanity checks
     * ----------------
     */
    Assert(RelationIsValid(relation));
    if (LockingDisabled())
	return;
    
    if (!LockInfoIsValid(relation->lockInfo))
        elog(WARN, 
            "Releasing a lock on %s with invalid lock information",
            RelationGetRelationName(relation));

    MultiReleasePage((LockInfo) relation->lockInfo, itemPointer, WRITE_LOCK);
}

/*
 * Set a single level write page lock.  Assumes that you already
 * have a write intent lock on the relation.
 */
RelationSetSingleWLockPage(relation, partition, itemPointer)
    Relation relation;
    PagePartition       partition;
    ItemPointer         itemPointer;
{

    /* ----------------
     *	sanity checks
     * ----------------
     */
    Assert(RelationIsValid(relation));
    if (LockingDisabled())
	return;
    
    if (!LockInfoIsValid(relation->lockInfo))
	RelationInitLockInfo(relation);

    SingleLockPage(relation->lockInfo, itemPointer, WRITE_LOCK, !UNLOCK);
}

/*
 * Unset a single level write page lock
 */
RelationUnsetSingleWLockPage(relation, partition, itemPointer)
    Relation relation;
    PagePartition       partition;
    ItemPointer         itemPointer;
{

    /* ----------------
     *	sanity checks
     * ----------------
     */
    Assert(RelationIsValid(relation));
    if (LockingDisabled())
	return;
    
    if (!LockInfoIsValid(relation->lockInfo))
        elog(WARN, 
            "Releasing a lock on %s with invalid lock information",
            RelationGetRelationName(relation));

    SingleLockPage(relation->lockInfo, itemPointer, WRITE_LOCK, UNLOCK);
}

/*
 * Set a single level read page lock.  Assumes you already have a read
 * intent lock set on the relation.
 */
RelationSetSingleRLockPage(relation, partition, itemPointer)
    Relation relation;
    PagePartition       partition;
    ItemPointer         itemPointer;
{

    /* ----------------
     *	sanity checks
     * ----------------
     */
    Assert(RelationIsValid(relation));
    if (LockingDisabled())
	return;
    
    if (!LockInfoIsValid(relation->lockInfo))
	RelationInitLockInfo(relation);

    SingleLockPage(relation->lockInfo, itemPointer, READ_LOCK, !UNLOCK);
}

/* 
 * Unset a single level read page lock.
 */
RelationUnsetSingleRLockPage(relation, partition, itemPointer)
    Relation relation;
    PagePartition       partition;
    ItemPointer         itemPointer;
{

    /* ----------------
     *	sanity checks
     * ----------------
     */
    Assert(RelationIsValid(relation));
    if (LockingDisabled())
	return;
    
    if (!LockInfoIsValid(relation->lockInfo))
        elog(WARN, 
            "Releasing a lock on %s with invalid lock information",
            RelationGetRelationName(relation));

    SingleLockPage(relation->lockInfo, itemPointer, READ_LOCK, UNLOCK);
}

/*
 * Set a read intent lock on a relation.
 *
 * Usually these are set in a multi-level table when you acquiring a
 * page level lock.  i.e. To acquire a lock on a page you first acquire
 * an intent lock on the entire relation.  Acquiring an intent lock along
 * allows one to use the single level locking routines later.  Good for
 * index scans that do a lot of page level locking.
 */
RelationSetRIntentLock(relation)
    Relation relation;
{
    /* -----------------
     * Sanity check
     * -----------------
     */
    Assert(RelationIsValid(relation));
    if (LockingDisabled())
	return;

    if (!LockInfoIsValid(relation->lockInfo))
	RelationInitLockInfo(relation);

    SingleLockReln(relation->lockInfo, READ_LOCK+INTENT, !UNLOCK);
}

/*
 * Unset a read intent lock on a relation
 */
RelationUnsetRIntentLock(relation)
    Relation relation;
{
    /* -----------------
     * Sanity check
     * -----------------
     */
    Assert(RelationIsValid(relation));
    if (LockingDisabled())
	return;

    if (!LockInfoIsValid(relation->lockInfo))
	RelationInitLockInfo(relation);

    SingleLockReln(relation->lockInfo, READ_LOCK+INTENT, UNLOCK);
}

/*
 * Set a write intent lock on a relation. For a more complete explanation
 * see RelationSetRIntentLock()
 */
RelationSetWIntentLock(relation)
    Relation relation;
{
    /* -----------------
     * Sanity check
     * -----------------
     */
    Assert(RelationIsValid(relation));
    if (LockingDisabled())
	return;

    if (!LockInfoIsValid(relation->lockInfo))
	RelationInitLockInfo(relation);

    SingleLockReln(relation->lockInfo, WRITE_LOCK+INTENT, !UNLOCK);
}

/*
 * Unset a write intent lock.
 */
RelationUnsetWIntentLock(relation)
    Relation relation;
{
    /* -----------------
     * Sanity check
     * -----------------
     */
    Assert(RelationIsValid(relation));
    if (LockingDisabled())
	return;

    if (!LockInfoIsValid(relation->lockInfo))
	RelationInitLockInfo(relation);

    SingleLockReln(relation->lockInfo, WRITE_LOCK+INTENT, UNLOCK);
}

/*
 * Extend locks are used primarily in tertiary storage devices such as
 * a WORM disk jukebox.  Sometimes need exclusive access to extend a 
 * file by a block.
 */
RelationSetLockForExtend(relation)
    Relation relation;
{
    /* -----------------
     * Sanity check
     * -----------------
     */
    Assert(RelationIsValid(relation));
    if (LockingDisabled())
	return;

    if (!LockInfoIsValid(relation->lockInfo))
	RelationInitLockInfo(relation);

    MultiLockReln((LockInfo) relation->lockInfo, EXTEND_LOCK);
}

RelationUnsetLockForExtend(relation)
    Relation relation;
{
    /* -----------------
     * Sanity check
     * -----------------
     */
    Assert(RelationIsValid(relation));
    if (LockingDisabled())
	return;

    if (!LockInfoIsValid(relation->lockInfo))
	RelationInitLockInfo(relation);

    MultiReleaseReln((LockInfo) relation->lockInfo, EXTEND_LOCK);
}

/* 
 * Create an LRelid --- Why not just pass in a pointer to the storage?
 */
void
LRelIdAssign(lRelId, dbId, relId)
     LRelId	*lRelId;
     ObjectId 	dbId;
     ObjectId 	relId;
{   
    lRelId->dbId = dbId;
    lRelId->relId = relId;
}
