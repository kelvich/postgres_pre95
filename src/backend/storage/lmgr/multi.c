/*
 * multi.c  -- multi level lock table manager
 *
 *  Standard multi-level lock manager as per the Gray paper
 * (at least, that is what it is supposed to be).  I implement
 * three levels -- RELN, PAGE, TUPLE.  Tuple is actually TID
 * a physical record pointer.  It isn't an object id.  I need
 * that in order to reserve space for a  TupleInsert() and,
 * hopefully, it is what the rest of POSTGRES needs as well.
 *
 * NOTES:
 *   (1) The lock.c module assumes that the caller here is doing
 * two phase locking.
 *
 *
 * Interface:
 *	MultiLockReln(), MultiLockTuple(), InitMultiLockm();
 *
 * MultiLockRelease() is not currently used but will be when we
 * implement short term locking.
 *
 * $Header$
 */
#include "def.h"
#include "sem.h"
#include "shmem.h"
#include "lock.h"
#include "reln.h"
#include "multilev.h"


/*
 * INTENT indicates to higher level that a lower level lock has been
 * set.  For example, a write lock on a tuple conflicts with a write 
 * lock on a relation.  This conflict is detected as a WRITE_INTENT/
 * WRITE conflict between the tuple's intent lock and the relation's
 * write lock.
 */
private
int MultiConflicts[] = {
  NULL,	
  /* everything conflicts with a write lock */
  0xF,  
  /* read locks conflict with write locks at curr and lower levels */
  (1 << WRITE_LOCK)| (1 << WRITE_INTENT),  
  /* write intent locks */
  (1 << READ_LOCK) | (1 << WRITE_LOCK),
  /* read intent locks*/
  (1 << WRITE_LOCK),
};

/*
 * write locks have higher priority than read locks.  May
 * want to treat INTENT locks differently.
 */
private
int MultiPrios[] = {
  NULL,
  2,
  1,
  2,
  1
};

/* 
 * Lock table identifier for this lock table.  The multi-level
 * lock table is ONE lock table, not three.
 */
TableId MultiTableId = NULL;
TableId ShortTermTableId = NULL;

/*
 * Create the lock table described by MultiConflicts and Multiprio.
 */
InitMultiLevelLockm()
{
  int tableId;

  tableId = LockTabInit("LockTable",MultiConflicts,MultiPrios,4);
  MultiTableId = tableId;
  if (! (MultiTableId)) {
    elog(WARN,"InitMultiLockm: couldnt initialize lock table");
  }
  ShortTermTableId = LockTabRename(tableId);
  if (! (ShortTermTableId)) {
    elog(WARN,"InitMultiLockm: couldnt rename lock table");
  }
}

/*
 * MultiLockReln -- lock a relation
 *
 * Returns: TRUE if the lock can be set, FALSE otherwise.
 */
MultiLockReln(reln, lockt)
Reln *		reln;
LOCKT		lockt;
{
  LOCKTAG	tag;

  /* LOCKTAG has two bytes of padding, unfortunately.  The
   * hash function will return miss if the padding bytes aren't
   * zero'd.
   */
  bzero(&tag,sizeof(tag));
  RelnGetOid(reln,&tag.relId);
  return(MultiAcquire(MultiTableId, &tag, lockt, RELN_LEVEL));
}

/*
 * MultiLockTuple -- Lock the TID associated with a tuple
 *
 * Returns: TRUE if lock is set, FALSE otherwise.
 *
 * Side Effects: causes intention level locks to be set
 * 	at the page and relation level.
 */
MultiLockTuple(reln, tidPtr, lockt)
Reln *		reln;
TupleId		*tidPtr;
LOCKT		lockt;		
{
  LOCKTAG	tag;

  /* LOCKTAG has two bytes of padding, unfortunately.  The
   * hash function will return miss if the padding bytes aren't
   * zero'd.
   */
  bzero(&tag,sizeof(tag));
  RelnGetOid(reln,&(tag.relId));
  /* not locking any valid Tuple, just the page */
  tag.tupleId = *tidPtr;
  return(MultiAcquire(MultiTableId, &tag, lockt, TUPLE_LEVEL));
}

/*
 * same as above at page level
 */
MultiLockPage(reln, tidPtr, lockt)
Reln *		reln;
TupleId		*tidPtr;
LOCKT		lockt;		
{
  LOCKTAG	tag;

  /* LOCKTAG has two bytes of padding, unfortunately.  The
   * hash function will return miss if the padding bytes aren't
   * zero'd.
   */
  bzero(&tag,sizeof(tag));
  RelnGetOid(reln,&(tag.relId));
  MAKE_TID(tag.tupleId,tidPtr->blockNum,INVALID_TID);
  return(MultiAcquire(MultiTableId, &tag, lockt, PAGE_LEVEL));
}

/*
 * MultiAcquire -- acquire multi level lock at requested level
 *
 * Returns: TRUE if lock is set, FALSE if not
 * Side Effects:
 */
MultiAcquire(tableId, tag, lockt, level)
TableId		tableId;
LOCKTAG		*tag;
LOCK_LEVEL  	level;
LOCKT		lockt;
{
  LOCKT locks[N_LEVELS];
  int	i,status;
  LOCKTAG 	xxTag, *tmpTag = &xxTag;
  int	retStatus = TRUE;

  /*
   * Three levels implemented.  If we set a low level (e.g. Tuple)
   * lock, we must set INTENT locks on the higher levels.  The 
   * intent lock detects conflicts between the low level lock
   * and an existing high level lock.  For example, setting a
   * write lock on a tuple in a relation is disallowed if there
   * is an existing read lock on the entire relation.  The
   * write lock would set a WRITE + INTENT lock on the relation
   * and that lock would conflict with the read.
   */
  switch (level) {
  case RELN_LEVEL:
    locks[0] = lockt;
    locks[1] = NO_LOCK;
    locks[2] = NO_LOCK;
    break;
  case PAGE_LEVEL:
    locks[0] = lockt + INTENT;
    locks[1] = lockt;
    locks[2] = NO_LOCK;
    break;
  case TUPLE_LEVEL:
    locks[0] = lockt + INTENT;
    locks[1] = lockt + INTENT;
    locks[2] = lockt;
    break;
  default:
    elog(WARN,"MultiAcquire: bad lock level");
    return(FALSE);
  }
  
  /*
   * construct a new tag as we go. Always loop through all levels,
   * but if we arent' seting a low level lock, locks[i] is set to
   * NO_LOCK for the lowere levels.  Always start from the highest
   * level and go to the lowest level. 
   */
  bzero(tmpTag,sizeof(*tmpTag));
  OID_Assign(tmpTag->relId,tag->relId);

  for (i=0;i<N_LEVELS;i++) {
    if (locks[i] != NO_LOCK) {
      switch (i) {
      case RELN_LEVEL:
	MAKE_TID(tmpTag->tupleId,INVALID_TID,INVALID_TID);
	break;
      case PAGE_LEVEL:
	MAKE_TID(tmpTag->tupleId,tag->tupleId.blockNum,INVALID_TID);
	break;
      case TUPLE_LEVEL:
	tmpTag->tupleId = tag->tupleId;
	break;
      }

      status = LockAcquire(tableId, tmpTag, locks[i]);
      if (! status) {
	/* failed for some reason. Before returning we have
	 * to release all of the locks we just acquired.
	 * MultiRelease(xx,xx,xx, i) means release starting from
	 * the last level lock we successfully acquired
	 */
	retStatus = FALSE;
	(void) MultiRelease(tableId, tag, lockt, i);
	/* now leave the loop.  Don't try for any more locks */
	break;
      }
    }
  }
  return(retStatus);
}


/*
 * MultiRelease -- release a multi-level lock
 *
 * Returns: TRUE if successful, FALSE otherwise.
 */
MultiRelease(tableId, tag, lockt, level)
TableId		tableId;
LOCKTAG		*tag;
LOCK_LEVEL  	level;
LOCKT		lockt;
{
  LOCKT 	locks[N_LEVELS];
  int		i,status;
  LOCKTAG 	xxTag, *tmpTag = &xxTag;

  /* 
   * same level scheme as MultiAcquire().
   */
  switch (level) {
  case RELN_LEVEL:
    locks[0] = lockt;
    locks[1] = NO_LOCK;
    locks[2] = NO_LOCK;
    break;
  case PAGE_LEVEL:
    locks[0] = lockt + INTENT;
    locks[1] = lockt;
    locks[2] = NO_LOCK;
    break;
  case TUPLE_LEVEL:
    locks[0] = lockt + INTENT;
    locks[1] = lockt + INTENT;
    locks[2] = lockt;
    break;
  default:
    elog(WARN,"MultiRelease: bad lockt");
  }
  
  /*
   * again, construct the tag on the fly.  This time, however,
   * we release the locks in the REVERSE order -- from lowest
   * level to highest level.  
   */
  OID_Assign(tmpTag->relId,tag->relId);
  for (i=N_LEVELS;i;i--) {
    switch (i) {
    case RELN_LEVEL:
      MAKE_TID(tmpTag->tupleId,INVALID_TID,INVALID_TID);
      break;
    case PAGE_LEVEL:
      MAKE_TID(tmpTag->tupleId,tag->tupleId.blockNum,INVALID_TID);
      break;
    case TUPLE_LEVEL:
      tmpTag->tupleId = tag->tupleId;
      break;
    }
    if (locks[i] == NO_LOCK) {
      break;
    } else {
      status = LockRelease(tableId, tag, locks[i]);
      if (! status) {
	elog(WARN,"MultiAcquire: couldn't release after error");
      }
    }
  }
}
