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
#include "storage/shmem.h"
#include "storage/lock.h"
#include "utils/rel.h"
#include "utils/log.h"
#include "storage/multilev.h"


/*
 * INTENT indicates to higher level that a lower level lock has been
 * set.  For example, a write lock on a tuple conflicts with a write 
 * lock on a relation.  This conflict is detected as a WRITE_INTENT/
 * WRITE conflict between the tuple's intent lock and the relation's
 * write lock.
 */
static
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
  /* Will add extend locks for archive storage manager */
};

/*
 * write locks have higher priority than read locks.  May
 * want to treat INTENT locks differently.
 */
static
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

TableId
InitMultiLevelLockm()
{
  int tableId;

  /* -----------------------
   * If we're already initialized just return the table id.
   * -----------------------
   */
  if (MultiTableId)
	return MultiTableId;

  tableId = LockTabInit("LockTable", MultiConflicts, MultiPrios, 4);
  MultiTableId = tableId;
  if (! (MultiTableId)) {
    elog(WARN,"InitMultiLockm: couldnt initialize lock table");
  }
  /* -----------------------
   * No short term lock table for now.  -Jeff 15 July 1991
   * 
   * ShortTermTableId = LockTabRename(tableId);
   * if (! (ShortTermTableId)) {
   *   elog(WARN,"InitMultiLockm: couldnt rename lock table");
   * }
   * -----------------------
   */
  return MultiTableId;
}

/*
 * MultiLockReln -- lock a relation
 *
 * Returns: TRUE if the lock can be set, FALSE otherwise.
 */
MultiLockReln(reln, lockt)
Relation *	reln;
LOCKT		lockt;
{
  LOCKTAG	tag;

  /* LOCKTAG has two bytes of padding, unfortunately.  The
   * hash function will return miss if the padding bytes aren't
   * zero'd.
   */
  bzero(&tag,sizeof(tag));
  tag.relId = RelationGetRelationId(reln);
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
Relation *	reln;
ItemPointer	tidPtr;
LOCKT		lockt;		
{
  LOCKTAG	tag;

  /* LOCKTAG has two bytes of padding, unfortunately.  The
   * hash function will return miss if the padding bytes aren't
   * zero'd.
   */
  bzero(&tag,sizeof(tag));

  tag.relId = RelationGetRelationId(reln);

  /* not locking any valid Tuple, just the page */
  tag.tupleId = *tidPtr;
  return(MultiAcquire(MultiTableId, &tag, lockt, TUPLE_LEVEL));
}

/*
 * same as above at page level
 */
MultiLockPage(reln, tidPtr, lockt)
Relation	*reln;
ItemPointer	tidPtr;
LOCKT		lockt;		
{
  LOCKTAG	tag;

  /* LOCKTAG has two bytes of padding, unfortunately.  The
   * hash function will return miss if the padding bytes aren't
   * zero'd.
   */
  bzero(&tag,sizeof(tag));

  tag.relId = RelationGetRelationId(reln);

  /* ----------------------------
   * Now we want to set the page offset to be invalid 
   * and lock the block.  There is some confusion here as to what
   * a page is.  In Postgres a page is an 8k block, however this
   * block may be partitioned into many subpages which are sometimes
   * also called pages.  The term is overloaded, so don't be fooled
   * when we say lock the page we mean the 8k block. -Jeff 16 July 1991
   * ----------------------------
   */
  ItemPointerSetInvalid( &(tag.tupleId) );
  BlockIdCopy( &(tag.tupleId.blockData), &(tidPtr->blockData) );
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
   * NO_LOCK for the lower levels.  Always start from the highest
   * level and go to the lowest level. 
   */
  bzero(tmpTag,sizeof(*tmpTag));
  tmpTag->relId = tag->relId;

  for (i=0;i<N_LEVELS;i++) {
    if (locks[i] != NO_LOCK) {
      switch (i) {
      case RELN_LEVEL:
	ItemPointerSetInvalid( &(tmpTag->tupleId) );
	break;
      case PAGE_LEVEL:
	ItemPointerSetInvalid( &(tmpTag->tupleId) );
	BlockIdCopy(&(tmpTag->tupleId.blockData),&tag->tupleId.blockData);
	break;
      case TUPLE_LEVEL:
	ItemPointerCopy(&tmpTag->tupleId, &tag->tupleId);
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
  tmpTag->relId = tag->relId;
  for (i=N_LEVELS;i;i--) {
    switch (i) {
    case RELN_LEVEL:
      ItemPointerSetInvalid( &(tmpTag->tupleId) );
      break;
    case PAGE_LEVEL:
      ItemPointerSetInvalid( &(tmpTag->tupleId) );
      BlockIdCopy(&(tmpTag->tupleId.blockData),&tag->tupleId.blockData);
      break;
    case TUPLE_LEVEL:
      ItemPointerCopy(&tmpTag->tupleId, &tag->tupleId);
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
