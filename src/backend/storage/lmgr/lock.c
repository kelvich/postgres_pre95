/*
 * lock.c -- simple lock acquisition
 *
 *  Outside modules can create a lock table and acquire/release
 *  locks.  A lock table is a shared memory hash table.  When
 *  a process tries to acquire a lock of a type that conflicts
 *  with existing locks, it is put to sleep using the routines
 *  in storage/lmgr/proc.c.
 *
 *  Interface:
 *
 *  LockAcquire(), LockRelease(), LockTabInit().
 *
 *  LockReplace() is called only within this module and by the
 *  	lkchain module.  It releases a lock without looking
 * 	the lock up in the lock table.
 *
 *  NOTE: This module is used to define new lock tables.  The
 *	multi-level lock table (multi.c) used by the heap
 *	access methods calls these routines.  See multi.c for
 *	examples showing how to use this interface.
 *
 * $Header$
 */
#include "storage/shmem.h"
#include "storage/spin.h"
#include "storage/proc.h"
#include "storage/lock.h"
#include "utils/hsearch.h"
#include "utils/log.h"
#include "access/xact.h"

void LockTypeInit ARGS((
	LOCKTAB *ltable, 
	MASK *conflictsP, 
	int *prioP, 
	int ntypes
));

extern int MyPid;		/* For parallel backends w/same xid */
SPINLOCK LockMgrLock;		/* in Shmem or created in CreateSpinlocks() */

/* This is to simplify/speed up some bit arithmetic */

static MASK	BITS_OFF[MAX_LOCKTYPES];
static MASK	BITS_ON[MAX_LOCKTYPES];

/* -----------------
 * XXX Want to move this to this file
 * -----------------
 */
static bool LockingIsDisabled;

/* ------------------
 * from storage/ipc/shmem.c
 * ------------------
 */
extern HTAB *ShmemInitHash();

/* -------------------
 * map from tableId to the lock table structure
 * -------------------
 */
LOCKTAB *AllTables[MAX_TABLES];

/* -------------------
 * no zero-th table
 * -------------------
 */
int	NumTables = 1;

/* -------------------
 * InitLocks -- Init the lock module.  Create a private data
 *	structure for constructing conflict masks.
 * -------------------
 */
void
InitLocks()
{
  int i;
  int bit;

  bit = 1;
  /* -------------------
   * remember 0th locktype is invalid
   * -------------------
   */
  for (i=0;i<MAX_LOCKTYPES;i++,bit <<= 1)
  {
    BITS_ON[i] = bit;
    BITS_OFF[i] = ~bit;
  }
}

/* -------------------
 * LockDisable -- sets LockingIsDisabled flag to TRUE or FALSE.
 * ------------------
 */
void
LockDisable(status)
int status;
{
  LockingIsDisabled = status;
}


/*
 * LockTypeInit -- initialize the lock table's lock type
 *	structures
 *
 * Notes: just copying.  Should only be called once.
 */
static
void
LockTypeInit(ltable, conflictsP, prioP, ntypes)
LOCKTAB	*ltable;
MASK	*conflictsP;
int	*prioP;
int 	ntypes;
{
  int	i;

  ltable->ctl->nLockTypes = ntypes;
  ntypes++;
  for (i=0;i<ntypes;i++,prioP++,conflictsP++)
  {
    ltable->ctl->conflictTab[i] = *conflictsP;
    ltable->ctl->prio[i] = *prioP;
  }
}

/*
 * LockTabInit -- initialize a lock table structure
 *
 * Notes:
 *	(a) a lock table has four separate entries in the binding
 *	table.  This is because every shared hash table and spinlock
 *	has its name stored in the binding table at its creation.  It
 *	is wasteful, in this case, but not much space is involved.
 *
 */
LockTableId
LockTabInit(tabName, conflictsP, prioP, ntypes)
char *tabName;
MASK	*conflictsP;
int	*prioP;
int 	ntypes;
{
  LOCKTAB *ltable;
  char *shmemName;
  HASHCTL info;
  int hash_flags;
  Boolean	found;
  int status = TRUE;

  if (ntypes > MAX_LOCKTYPES)
  {
    elog(NOTICE,"LockTabInit: too many lock types %d greater than %d",
	 ntypes,MAX_LOCKTYPES);
    return(INVALID_TABLEID);
  }

  if (NumTables > MAX_TABLES)
  {
    elog(NOTICE,
	 "LockTabInit: system limit of MAX_TABLES (%d) lock tables",
	 MAX_TABLES);
    return(INVALID_TABLEID);
  }

  /* allocate a string for the binding table lookup */
  shmemName = (char *) palloc((unsigned)(strlen(tabName)+32));
  if (! shmemName)
  {
    elog(NOTICE,"LockTabInit: couldn't malloc string %s \n",tabName);
    return(INVALID_TABLEID);
  }

  /* each lock table has a non-shared header */
  ltable = (LOCKTAB *) palloc((unsigned) sizeof(LOCKTAB));
  if (! ltable)
  {
    elog(NOTICE,"LockTabInit: couldn't malloc lock table %s\n",tabName);
    (void) pfree (shmemName);
    return(INVALID_TABLEID);
  }

  /* ------------------------
   * find/acquire the spinlock for the table
   * ------------------------
   */
   SpinAcquire(LockMgrLock);


  /* -----------------------
   * allocate a control structure from shared memory or attach to it
   * if it already exists.
   * -----------------------
   */
  sprintf(shmemName,"%s (ctl)",tabName);
  ltable->ctl = (LOCKCTL *)
    ShmemInitStruct(shmemName,(unsigned)sizeof(LOCKCTL),&found);

  if (! ltable->ctl)
  {
    elog(FATAL,"LockTabInit: couldn't initialize %s",tabName);
    status = FALSE;
  }

  /* ----------------
   * we're first - initialize
   * ----------------
   */
  if (! found)
  {
    bzero(ltable->ctl,sizeof(LOCKCTL));
    ltable->ctl->masterLock = LockMgrLock;
    ltable->ctl->tableId = NumTables;
  }

  /* --------------------
   * other modules refer to the lock table by a tableId
   * --------------------
   */
  AllTables[NumTables] = ltable;
  NumTables++;
  Assert(NumTables <= MAX_TABLES);

  /* ----------------------
   * allocate a hash table for the lock tags.  This is used
   * to find the different locks.
   * ----------------------
   */
  info.keysize =  sizeof(LOCKTAG);
  info.datasize = sizeof(LOCK);
  info.hash = tag_hash;
  hash_flags = (HASH_ELEM | HASH_FUNCTION);

  sprintf(shmemName,"%s (lock hash)",tabName);
  ltable->lockHash = (HTAB *) ShmemInitHash(shmemName,
		  INIT_TABLE_SIZE,MAX_TABLE_SIZE,
		  &info,hash_flags);

  Assert( ltable->lockHash->hash == tag_hash);
  if (! ltable->lockHash)
  {
    elog(FATAL,"LockTabInit: couldn't initialize %s",tabName);
    status = FALSE;
  }

  /* -------------------------
   * allocate an xid table.  When different transactions hold
   * the same lock, additional information must be saved (locks per tx).
   * -------------------------
   */
  info.keysize = XID_TAGSIZE;
  info.datasize = sizeof(XIDLookupEnt);
  info.hash = tag_hash;
  hash_flags = (HASH_ELEM | HASH_FUNCTION);

  sprintf(shmemName,"%s (xid hash)",tabName);
  ltable->xidHash = (HTAB *) ShmemInitHash(shmemName,
		  INIT_TABLE_SIZE,MAX_TABLE_SIZE,
		  &info,hash_flags);

  if (! ltable->xidHash)
  {
    elog(FATAL,"LockTabInit: couldn't initialize %s",tabName);
    status = FALSE;
  }

  /* init ctl data structures */
  LockTypeInit(ltable, conflictsP, prioP, ntypes);

  SpinRelease(LockMgrLock);

  (void) pfree (shmemName);

  if (status)
    return(ltable->ctl->tableId);
  else
    return(INVALID_TABLEID);
}

/*
 * LockTabRename -- allocate another tableId to the same
 *	lock table.
 *
 * NOTES: Both the lock module and the lock chain (lchain.c)
 *	module use table id's to distinguish between different
 *	kinds of locks.  Short term and long term locks look
 *	the same to the lock table, but are handled differently
 *	by the lock chain manager.  This function allows the
 *	client to use different tableIds when acquiring/releasing
 *	short term and long term locks.
 */
LockTableId
LockTabRename(tableId)
LockTableId	tableId;
{
  LockTableId	newTableId;

  if (NumTables >= MAX_TABLES)
  {
    return(INVALID_TABLEID);
  }
  if (AllTables[tableId] == INVALID_TABLEID)
  {
    return(INVALID_TABLEID);
  }

  /* other modules refer to the lock table by a tableId */
  newTableId = NumTables;
  NumTables++;

  AllTables[newTableId] = AllTables[tableId];
  return(newTableId);
}

/*
 * LockAcquire -- Check for lock conflicts, sleep if conflict found,
 *	set lock if/when no conflicts.
 *
 * Returns: TRUE if parameters are correct, FALSE otherwise.
 *
 * Side Effects: The lock is always acquired.  No way to abort
 *	a lock acquisition other than aborting the transaction.
 *	Lock is recorded in the lkchain.
 */
bool
LockAcquire(tableId, lockName, lockt)
LockTableId		tableId;
LOCKTAG		*lockName;
LOCKT		lockt;
{
  XIDLookupEnt	*result,item;
  HTAB		*xidTable;
  Boolean	found;
  LOCK		*lock = NULL;
  SPINLOCK 	masterLock;
  LOCKTAB 	*ltable;
  int 		status;
  TransactionId	myXid;

  Assert (tableId < NumTables);
  ltable = AllTables[tableId];
  if (!ltable)
  {
    elog(NOTICE,"LockAcquire: bad lock table %d",tableId);
    return  (FALSE);
  }

  if (LockingIsDisabled)
  {
    return(TRUE);
  }

  LOCK_PRINT("Acquire",lockName,lockt);
  masterLock = ltable->ctl->masterLock;

  SpinAcquire(masterLock);

  Assert( ltable->lockHash->hash == tag_hash);
  lock = (LOCK *)hash_search(ltable->lockHash,(Pointer)lockName,HASH_ENTER,&found);

  if (! lock)
  {
    SpinRelease(masterLock);
    elog(FATAL,"LockAcquire: lock table %d is corrupted",tableId);
    return(FALSE);
  }

  /* --------------------
   * if there was nothing else there, complete initialization
   * --------------------
   */
  if  (! found)
  {
    lock->mask = 0;
    ProcQueueInit(&(lock->waitProcs));
    bzero((char *)lock->holders,sizeof(int)*MAX_LOCKTYPES);
    bzero((char *)lock->activeHolders,sizeof(int)*MAX_LOCKTYPES);
    lock->nHolding = 0;
    lock->nActive = 0;

    Assert ( BlockIdEquals(&(lock->tag.tupleId.blockData), &(lockName->tupleId.blockData)) );

  }

  /* ------------------
   * add an element to the lock queue so that we can clear the
   * locks at end of transaction.
   * ------------------
   */
  xidTable = ltable->xidHash;
  myXid = GetCurrentTransactionId();

  /* ------------------
   * Zero out all of the tag bytes (this clears the padding bytes for long
   * word alignment and ensures hashing consistency).
   * ------------------
   */
  bzero(&item, XID_TAGSIZE);
  TransactionIdStore(myXid, (char *) &item.tag.xid);
  item.tag.lock = MAKE_OFFSET(lock);
  item.tag.pid = MyPid;

  result = (XIDLookupEnt *)hash_search(xidTable, (Pointer)&item, HASH_ENTER, &found);
  if (!result)
  {
    elog(NOTICE,"LockResolveConflicts: xid table corrupted");
    return(STATUS_ERROR);
  }
  if (!found)
  {
    ProcAddLock(&result->queue);
    result->nHolding = 0;
    bzero((char *)result->holders,sizeof(int)*MAX_LOCKTYPES);
  }

  /* ----------------
   * lock->nholding tells us how many processes have _tried_ to
   * acquire this lock,  Regardless of whether they succeeded or
   * failed in doing so.
   * ----------------
   */
  lock->nHolding++;
  lock->holders[lockt]++;

  /* --------------------
   * If I'm the only one holding a lock, then there
   * cannot be a conflict.  Need to subtract one from the
   * lock's count since we just bumped the count up by 1 
   * above.
   * --------------------
   */
  if (result->nHolding == lock->nActive)
  {
    result->holders[lockt]++;
    result->nHolding++;
    GrantLock(lock, lockt);
    SpinRelease(masterLock);
    return(TRUE);
  }

  Assert(result->nHolding <= lock->nActive);

  status = LockResolveConflicts(ltable, lock, lockt, myXid, (int) MyPid);

  if (status == STATUS_OK)
  {
    GrantLock(lock, lockt);
  }
  else if (status == STATUS_FOUND)
  {
    /* -----------------
     * nHolding tells how many have tried to acquire the lock.
     *------------------
     */
    status = WaitOnLock(ltable, tableId, lock, lockt);
  }

  SpinRelease(masterLock);

  return(status == STATUS_OK);
}

/* ----------------------------
 * LockResolveConflicts -- test for lock conflicts
 *
 * NOTES:
 * 	Here's what makes this complicated: one transaction's
 * locks don't conflict with one another.  When many processes
 * hold locks, each has to subtract off the other's locks when
 * determining whether or not any new lock acquired conflicts with
 * the old ones.
 *
 *  For example, if I am already holding a WRITE_INTENT lock,
 *  there will not be a conflict with my own READ_LOCK.  If I
 *  don't consider the intent lock when checking for conflicts,
 *  I find no conflict.
 * ----------------------------
 */
LockResolveConflicts(ltable,lock,lockt,xid,pid)
LOCKTAB	*ltable;
LOCK	*lock;
LOCKT	lockt;
TransactionId	xid;
int pid;
{
  XIDLookupEnt	*result,item;
  int		*myHolders;
  int		nLockTypes;
  HTAB		*xidTable;
  Boolean	found;
  int		bitmask;
  int 		i,tmpMask;

  nLockTypes = ltable->ctl->nLockTypes;
  xidTable = ltable->xidHash;

  /* ---------------------
   * read my own statistics from the xid table.  If there
   * isn't an entry, then we'll just add one.
   *
   * Zero out the tag, this clears the padding bytes for long
   * word alignment and ensures hashing consistency.
   * ------------------
   */
  bzero(&item, XID_TAGSIZE);
  TransactionIdStore(xid, (char *) &item.tag.xid);
  item.tag.lock = MAKE_OFFSET(lock);
  item.tag.pid = pid;

  if (! (result = (XIDLookupEnt *)
	 hash_search(xidTable, (Pointer)&item, HASH_ENTER, &found)))
  {
    elog(NOTICE,"LockResolveConflicts: xid table corrupted");
    return(STATUS_ERROR);
  }
  myHolders = result->holders;

  if (! found)
  {
    /* ---------------
     * we're not holding any type of lock yet.  Clear
     * the lock stats.
     * ---------------
     */
    bzero(result->holders,
	  nLockTypes*sizeof(* (lock->holders)));
    result->nHolding = 0;
  }

  /* ----------------------------
   * first check for global conflicts: If no locks conflict
   * with mine, then I get the lock.
   *
   * Checking for conflict: lock->mask represents the types of
   * currently held locks.  conflictTable[lockt] has a bit
   * set for each type of lock that conflicts with mine.  Bitwise
   * compare tells if there is a conflict.
   * ----------------------------
   */
  if (! (ltable->ctl->conflictTab[lockt] & lock->mask))
  {
    result->holders[lockt]++;
    result->nHolding++;

    return(STATUS_OK);
  }

  /* ------------------------
   * Rats.  Something conflicts. But it could still be my own
   * lock.  We have to construct a conflict mask
   * that does not reflect our own locks.
   * ------------------------
   */
  bitmask = 0;
  tmpMask = 2;
  for (i=1;i<=nLockTypes;i++, tmpMask <<= 1)
  {
    if (lock->activeHolders[i] - myHolders[i])
    {
      bitmask |= tmpMask;
    }
  }

  /* ------------------------
   * now check again for conflicts.  'bitmask' describes the types
   * of locks held by other processes.  If one of these
   * conflicts with the kind of lock that I want, there is a
   * conflict and I have to sleep.
   * ------------------------
   */
  if (! (ltable->ctl->conflictTab[lockt] & bitmask))
  {

    /* no conflict. Get the lock and go on */

    result->holders[lockt]++;
    result->nHolding++;
    return(STATUS_OK);

  }

  return(STATUS_FOUND);
}

int
WaitOnLock(ltable, tableId, lock,lockt)
LOCKTAB		*ltable;
LOCK 		*lock;
LockTableId		tableId;
LOCKT		lockt;
{
  PROC_QUEUE *waitQueue = &(lock->waitProcs);

  int prio = ltable->ctl->prio[lockt];

  /* the waitqueue is ordered by priority. I insert myself
   * according to the priority of the lock I am acquiring.
   *
   * SYNC NOTE: I am assuming that the lock table spinlock
   * is sufficient synchronization for this queue.  That
   * will not be true if/when people can be deleted from
   * the queue by a SIGINT or something.
   */
  LOCK_PRINT("WaitOnLock: sleeping on lock", (&lock->tag), lockt);
  if (ProcSleep(waitQueue,
		ltable->ctl->masterLock,
                lockt,
                prio,
                lock) != NO_ERROR)
  {
    /* -------------------
     * This could have happend as a result of a deadlock, see HandleDeadLock()
     * Decrement the lock nHolding and holders fields as we are no longer 
     * waiting on this lock.
     * -------------------
     */
    lock->nHolding--;
    lock->holders[lockt]--;
    SpinRelease(ltable->ctl->masterLock);
    elog(WARN,"WaitOnLock: error on wakeup - Aborting this transaction");

    /* -------------
     * There is no return from elog(WARN, ...)
     * -------------
     */
  }

  return(STATUS_OK);
}

/*
 * LockRelease -- look up 'lockName' in lock table 'tableId' and
 *	release it.
 *
 * Side Effects: if the lock no longer conflicts with the highest
 *	priority waiting process, that process is granted the lock
 *	and awoken. (We have to grant the lock here to avoid a
 *	race between the waking process and any new process to
 *	come along and request the lock).
 */
bool
LockRelease(tableId, lockName, lockt)
LockTableId	tableId;
LOCKTAG	*lockName;
LOCKT	lockt;
{
  LOCK		*lock = NULL;
  SPINLOCK 	masterLock;
  Boolean	found;
  LOCKTAB 	*ltable;
  int		status;
  XIDLookupEnt	*result,item;
  HTAB 		*xidTable;
  bool		wakeupNeeded = true;

  Assert (tableId < NumTables);
  ltable = AllTables[tableId];
  if (!ltable) {
    elog(NOTICE, "ltable is null in LockRelease");
    return (FALSE);
  }

  if (LockingIsDisabled)
  {
    return(TRUE);
  }

  LOCK_PRINT("Release",lockName,lockt);

  masterLock = ltable->ctl->masterLock;
  xidTable = ltable->xidHash;

  SpinAcquire(masterLock);

  Assert( ltable->lockHash->hash == tag_hash);
  lock = (LOCK *)
    hash_search(ltable->lockHash,(Pointer)lockName,HASH_FIND_SAVE,&found);

  /* let the caller print its own error message, too.
   * Do not elog(WARN).
   */
  if (! lock)
  {
    SpinRelease(masterLock);
    elog(NOTICE,"LockRelease: locktable corrupted");
    return(FALSE);
  }

  if (! found)
  {
    SpinRelease(masterLock);
    elog(NOTICE,"LockRelease: locktable lookup failed, no lock");
    return(FALSE);
  }

  Assert(lock->nHolding > 0);

  /*
   * fix the general lock stats
   */
  lock->nHolding--;
  lock->holders[lockt]--;
  lock->nActive--;
  lock->activeHolders[lockt]--;

  Assert(lock->nActive >= 0);

  if (! lock->nHolding)
  {
    /* ------------------
     * if there's no one waiting in the queue,
     * we just released the last lock.
     * Delete it from the lock table.
     * ------------------
     */
    Assert( ltable->lockHash->hash == tag_hash);
    lock = (LOCK *) hash_search(ltable->lockHash,
				(Pointer) &(lock->tag),
				HASH_REMOVE_SAVED,
				&found);
    Assert(lock && found);
    wakeupNeeded = false;
  }

  /* ------------------
   * Zero out all of the tag bytes (this clears the padding bytes for long
   * word alignment and ensures hashing consistency).
   * ------------------
   */
  bzero(&item, XID_TAGSIZE);

  TransactionIdStore(GetCurrentTransactionId(), (char *) &item.tag.xid);
  item.tag.lock = MAKE_OFFSET(lock);
  item.tag.pid = MyPid;

  if (! ( result = (XIDLookupEnt *) hash_search(xidTable,
						(Pointer)&item,
						HASH_FIND_SAVE,
						&found) )
	 || !found)
  {
    SpinRelease(masterLock);
    elog(NOTICE,"LockReplace: xid table corrupted");
    return(FALSE);
  }
  /*
   * now check to see if I have any private locks.  If I do,
   * decrement the counts associated with them.
   */
  result->holders[lockt]--;
  result->nHolding--;

  /*
   * If this was my last hold on this lock, delete my entry
   * in the XID table.
   */
  if (! result->nHolding)
  {
    SHMQueueDelete(&result->queue);
    if (! (result = (XIDLookupEnt *)
	   hash_search(xidTable, (Pointer)&item, HASH_REMOVE_SAVED, &found)) ||
	! found)
    {
      SpinRelease(masterLock);
      elog(NOTICE,"LockReplace: xid table corrupted");
      return(FALSE);
    }
  }

  /* --------------------------
   * If there are still active locks of the type I just released, no one
   * should be woken up.  Whoever is asleep will still conflict
   * with the remaining locks.
   * --------------------------
   */
  if (! (lock->activeHolders[lockt]))
  {
    /* change the conflict mask.  No more of this lock type. */
    lock->mask &= BITS_OFF[lockt];
  }

  if (wakeupNeeded)
  {
    /* --------------------------
     * Wake the first waiting process and grant him the lock if it
     * doesn't conflict.  The woken process must record the lock
     * himself.
     * --------------------------
     */
    (void) ProcLockWakeup(&(lock->waitProcs), (char *) ltable, (char *) lock);
  }

  SpinRelease(masterLock);
  return(TRUE);
}

/*
 * GrantLock -- udpate the lock data structure to show
 *	the new lock holder.
 */
void
GrantLock(lock, lockt)
LOCK 	*lock;
LOCKT	lockt;
{
  lock->nActive++;
  lock->activeHolders[lockt]++;
  lock->mask |= BITS_ON[lockt];
}

bool
LockReleaseAll(tableId,lockQueue)
LockTableId		tableId;
SHM_QUEUE	*lockQueue;
{
  PROC_QUEUE 	*waitQueue;
  int		done;
  XIDLookupEnt	*xidLook = NULL;
  XIDLookupEnt	*tmp = NULL;
  SHMEM_OFFSET 	end = MAKE_OFFSET(lockQueue);
  SPINLOCK 	masterLock;
  LOCKTAB 	*ltable;
  int		i,nLockTypes;
  LOCK		*lock;
  Boolean	found;

  Assert (tableId < NumTables);
  ltable = AllTables[tableId];
  if (!ltable)
    return (FALSE);

  nLockTypes = ltable->ctl->nLockTypes;
  masterLock = ltable->ctl->masterLock;

  if (SHMQueueEmpty(lockQueue))
    return TRUE;

  SHMQueueFirst(lockQueue,&xidLook,&xidLook->queue);

  SpinAcquire(masterLock);
  for (;;)
  {
    /* ---------------------------
     * XXX Here we assume the shared memory queue is circular and
     * that we know its internal structure.  Should have some sort of
     * macros to allow one to walk it.  mer 20 July 1991
     * ---------------------------
     */
    done = (xidLook->queue.next == end);
    lock = (LOCK *) MAKE_PTR(xidLook->tag.lock);

    LOCK_PRINT("ReleaseAll",(&lock->tag),0);

    /* ------------------
     * fix the general lock stats
     * ------------------
     */
    if (lock->nHolding != xidLook->nHolding)
    {
      lock->nHolding -= xidLook->nHolding;
      lock->nActive -= xidLook->nHolding;
      Assert(lock->nActive >= 0);
      for (i=1; i<=nLockTypes; i++)
      {
	lock->holders[i] -= xidLook->holders[i];
	lock->activeHolders[i] -= xidLook->holders[i];
	if (! lock->activeHolders[i])
	  lock->mask &= BITS_OFF[i];
      }
    }
    else
    {
      /* --------------
       * set nHolding to zero so that we can garbage collect the lock
       * down below...
       * --------------
       */
      lock->nHolding = 0;
    }
    /* ----------------
     * always remove the xidLookup entry, we're done with it now
     * ----------------
     */
    if ((! hash_search(ltable->xidHash, (Pointer)xidLook, HASH_REMOVE, &found))
        || !found)
    {
      SpinRelease(masterLock);
      elog(NOTICE,"LockReplace: xid table corrupted");
      return(FALSE);
    }

    if (! lock->nHolding)
    {
      /* --------------------
       * if there's no one waiting in the queue, we've just released
       * the last lock.
       * --------------------
       */

      Assert( ltable->lockHash->hash == tag_hash);
      lock = (LOCK *)
	hash_search(ltable->lockHash,(Pointer)&(lock->tag),HASH_REMOVE, &found);
      if ((! lock) || (!found))
      {
	SpinRelease(masterLock);
	elog(NOTICE,"LockReplace: cannot remove lock from HTAB");
	return(FALSE);
      }
    }
    else
    {
      /* --------------------
       * Wake the first waiting process and grant him the lock if it
       * doesn't conflict.  The woken process must record the lock
       * him/herself.
       * --------------------
       */
      waitQueue = &(lock->waitProcs);
      (void) ProcLockWakeup(waitQueue, (char *) ltable, (char *) lock);
    }

    if (done)
      break;
    SHMQueueFirst(&xidLook->queue,&tmp,&tmp->queue);
    xidLook = tmp;
  }
  SpinRelease(masterLock);
  SHMQueueInit(lockQueue);
  return TRUE;
}

int
LockShmemSize()
{
	int size;
	int nLockBuckets, nLockSegs;
	int nXidBuckets, nXidSegs;

	nLockBuckets = 1 << (int)my_log2((NLOCKENTS - 1) / DEF_FFACTOR + 1);
	nLockSegs = 1 << (int)my_log2((nLockBuckets - 1) / DEF_SEGSIZE + 1);

	nXidBuckets = 1 << (int)my_log2((NLOCKS_PER_XACT-1) / DEF_FFACTOR + 1);
	nXidSegs = 1 << (int)my_log2((nLockBuckets - 1) / DEF_SEGSIZE + 1);

	size =	NLOCKENTS*sizeof(LOCK) +
		NBACKENDS*sizeof(XIDLookupEnt) +
		NBACKENDS*sizeof(PROC) +
		sizeof(LOCKCTL) +
		sizeof(PROC_HDR) +
		nLockSegs*DEF_SEGSIZE*sizeof(SEGMENT) +
		my_log2(NLOCKENTS) + sizeof(HHDR) +
		nXidSegs*DEF_SEGSIZE*sizeof(SEGMENT) +
		my_log2(NBACKENDS) + sizeof(HHDR);

	return size;
}

/* -----------------
 * Boolean function to determine current locking status
 * -----------------
 */
bool
LockingDisabled()
{
	return LockingIsDisabled;
}
