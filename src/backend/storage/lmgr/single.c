/*
 * single.c -- set single locks in the multi-level lock 
 *	hierarchy.
 *
 * Sometimes we don't want to set all levels of the multi-level
 *	lock hierarchy at once.  This allows us to set and release
 * 	one level at a time.  It's useful in index scans when
 *	you can set an intent lock at the beginning and thereafter
 * 	only set page locks.  Tends to speed things up.
 *
 * $Header$
 */
#include "storage/lmgr.h"
#include "storage/lock.h"
#include "storage/multilev.h"
#include "utils/rel.h"

/* from multi.c */
extern LockTableId MultiTableId;

/*
 * SingleLockReln -- lock a relation
 *
 * Returns: TRUE if the lock can be set, FALSE otherwise.
 */
SingleLockReln(linfo, lockt, action)
    LockInfo 	linfo;
    LOCKT	lockt;
    int		action;
{
    LOCKTAG	tag;

    /* 
     * LOCKTAG has two bytes of padding, unfortunately.  The
     * hash function will return miss if the padding bytes aren't
     * zero'd.
     */
    bzero(&tag,sizeof(tag));
    tag.relId = linfo->lRelId.relId;
    tag.dbId = linfo->lRelId.dbId;
    BlockIdSet(ItemPointerBlockId(&tag.tupleId), InvalidBlockNumber);
    PositionIdSetInvalid( ItemPointerPositionId(&(tag.tupleId)) );

    if (action == UNLOCK)
	return(LockRelease(MultiTableId, &tag, lockt));
    else
	return(LockAcquire(MultiTableId, &tag, lockt));
}

/*
 * SingleLockPage -- use multi-level lock table, but lock
 *	only at the page level.
 *
 * Assumes that an INTENT lock has already been set in the
 * multi-level lock table.
 *
 */
SingleLockPage(linfo, tidPtr, lockt, action)
LockInfo 	linfo;
ItemPointer	tidPtr;
LOCKT		lockt;
int		action;
{
    LOCKTAG	tag;

    /* 
     * LOCKTAG has two bytes of padding, unfortunately.  The
     * hash function will return miss if the padding bytes aren't
     * zero'd.
     */
    bzero(&tag,sizeof(tag));
    tag.relId = linfo->lRelId.relId;
    tag.dbId = linfo->lRelId.dbId;
    BlockIdCopy( ItemPointerBlockId(&tag.tupleId), ItemPointerBlockId(tidPtr) );
    PositionIdSetInvalid( ItemPointerPositionId(&(tag.tupleId)) );


    if (action == UNLOCK)
	return(LockRelease(MultiTableId, &tag, lockt));
    else
	return(LockAcquire(MultiTableId, &tag, lockt));
}

/*
 * SingleLockTuple -- use the multi-level lock table,
 *	but lock only at the tuple level.
 *
 * XXX not used and not tested --mer 28 Oct 1991
 */
SingleLockTuple(linfo, tidPtr, lockt, action)
LockInfo	linfo;
ItemPointer	tidPtr;
LOCKT		lockt;
int		action;
{
    LOCKTAG	tag;

    /* LOCKTAG has two bytes of padding, unfortunately.  The
     * hash function will return miss if the padding bytes aren't
     * zero'd.
     */
    bzero(&tag,sizeof(tag));
    tag.relId = linfo->lRelId.relId;
    tag.dbId = linfo->lRelId.dbId;
    tag.tupleId = *tidPtr;

    if (action == UNLOCK)
	return(LockRelease(MultiTableId, &tag, lockt));
    else
	return(LockAcquire(MultiTableId, &tag, lockt));
}
