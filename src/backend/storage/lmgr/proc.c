/*
 * proc.c -- routines to manage per-process shared memory data
 *	structure
 *
 *  Each postgres backend gets one of these.  We'll use it to
 *  clean up after the process should the process suddenly die.
 *
 *
 * Interface (a):
 *	ProcSleep(), ProcWakeup(), ProcWakeupNext(),
 * 	ProcQueueAlloc() -- create a shm queue for sleeping processes
 * 	ProcQueueInit() -- create a queue without allocing memory
 *
 * Locking and waiting for buffers can cause the backend to be
 * put to sleep.  Whoever releases the lock, etc. wakes the
 * process up again (and gives it an error code so it knows
 * whether it was awoken on an error condition).
 *
 * Interface (b):
 *
 * ProcReleaseLocks -- frees the locks associated with this process,
 * ProcKill -- destroys the shared memory state (and locks)
 *	associated with the process.
 *
 * 5/15/91 -- removed the buffer pool based lock chain in favor
 *	of a shared memory lock chain.  The write-protection is
 *	more expensive if the lock chain is in the buffer pool.
 *	The only reason I kept the lock chain in the buffer pool
 *	in the first place was to allow the lock table to grow larger
 *	than available shared memory and that isn't going to work
 *	without a lot of unimplemented support anyway.
 *
 * $Header$
 */
#include <sys/time.h>
#include <signal.h>

#include "utils/hsearch.h"
#include "utils/log.h"

#include "storage/buf.h"	
#include "storage/lock.h"
#include "storage/shmem.h"
#include "storage/spin.h"
#include "storage/proc.h"
#include "storage/procq.h"

extern int MyPid;	/* for parallel backends w/same xid */

/* --------------------
 * Spin lock for manipulating the shared process data structure:
 * ProcGlobal.... Adding an extra spin lock seemed like the smallest
 * hack to get around reading and updating this structure in shared
 * memory. -mer 17 July 1991
 * --------------------
 */
SPINLOCK ProcStructLock;

/*
 * For cleanup routines.  Don't cleanup if the initialization
 * has not happened.
 */
Boolean	ProcInitialized = FALSE;

PROC_HDR *ProcGlobal = NULL;

PROC 		*MyProc = NULL;

/* ------------------------
 * InitProc -- create a per-process data structure for this process
 * used by the lock manager on semaphore queues.
 * ------------------------
 */
void
InitProcess(key)
IPCKey key;
{
  bool found = false;
  int pid;
  int semstat;
  unsigned int location, myOffset;

  /* ------------------
   * Routine called if deadlock timer goes off. See ProcSleep()
   * ------------------
   */
  signal(SIGALRM, HandleDeadLock);

  SpinAcquire(ProcStructLock);

  /* attach to the free list */
  ProcGlobal = (PROC_HDR *)
    ShmemInitStruct("Proc Header",(unsigned)sizeof(PROC_HDR),&found);

  /* --------------------
   * We're the first - initialize.
   * --------------------
   */
  if (! found)
  {
    ProcGlobal->numProcs = 0;
    ProcGlobal->freeProcs = INVALID_OFFSET;
    ProcGlobal->currKey = IPCGetProcessSemaphoreInitKey(key);
  }

  if (MyProc != NULL)
  {
    SpinRelease(ProcStructLock);
    elog(WARN,"ProcInit: you already exist");
    return;
  }

  /* try to get a proc from the free list first */

  myOffset = ProcGlobal->freeProcs;

  if (myOffset != INVALID_OFFSET)
  {
    MyProc = (PROC *) MAKE_PTR(myOffset);
    ProcGlobal->freeProcs = MyProc->links.next;
  }
  else
  {
    /* have to allocate one.  We can't use the normal binding
     * table mechanism because the proc structure is stored
     * by PID instead of by a global name (need to look it
     * up by PID when we cleanup dead processes).
     */

    MyProc = (PROC *) ShmemAlloc((unsigned)sizeof(PROC));
    if (! MyProc)
    {
      SpinRelease(ProcStructLock);
      elog (FATAL,"cannot create new proc: out of memory");
    }

    /* this cannot be initialized until after the buffer pool */
    SHMQueueInit(&(MyProc->lockQueue));
    MyProc->procId = ProcGlobal->numProcs;
    ProcGlobal->numProcs++;
  }

  /*
   * zero out the spin lock counts and set the sLocks field for ProcStructLock 
   * to * 1 as we have acquired this spinlock above but didn't record it since
   * we didn't have MyProc until now.
   */
  bzero(MyProc->sLocks, sizeof(MyProc->sLocks));
  MyProc->sLocks[ProcStructLock] = 1;

  MyProc->sem.semId = IpcSemaphoreCreate(ProcGlobal->currKey,
                                         1,
                                         IPCProtection,
                                         IpcSemaphoreDefaultStartValue,
                                         &semstat);
  IpcSemaphoreLock(MyProc->sem.semId, 0, IpcExclusiveLock);

  /* -------------
   * Bump key val for next process
   * -------------
   */
  MyProc->sem.semKey = ProcGlobal->currKey;
  ProcGlobal->currKey++;

  /* ----------------------
   * Release the lock.
   * ----------------------
   */
  SpinRelease(ProcStructLock);

  MyProc->sem.semNum = 0;
  MyProc->pid = MyPid;

  /* ----------------
   * Start keeping spin lock stats from here on.  Any botch before
   * this initialization is forever botched
   * ----------------
   */
  bzero(MyProc->sLocks, MAX_SPINS*sizeof(*MyProc->sLocks));

  /* -------------------------
   * Install ourselves in the binding table.  The name to
   * use is determined by the OS-assigned process id.  That
   * allows the cleanup process to find us after any untimely
   * exit.
   * -------------------------
   */
  pid = getpid();
  location = MAKE_OFFSET(MyProc);
  if ((! ShmemPIDLookup(pid,&location)) || (location != MAKE_OFFSET(MyProc)))
  {
    elog(FATAL,"InitProc: ShmemPID table broken");
  }

  MyProc->errType = NO_ERROR;
  SHMQueueElemInit(&(MyProc->links));

  on_exitpg(ProcKill, pid);

  ProcInitialized = TRUE;
}

/*
 * ProcReleaseLocks() -- release all locks associated with this process
 *
 */
void
ProcReleaseLocks()
{
  if (!MyProc)
    return;
  LockReleaseAll(1,&MyProc->lockQueue);
}

bool
ProcSemaphoreKill(pid)
int pid;
{
  SHMEM_OFFSET  location,ShmemPIDDestroy();
  PROC *proc;

  location = INVALID_OFFSET;

  ShmemPIDLookup(pid,&location);
  if (location == INVALID_OFFSET)
    return(FALSE);

  proc = (PROC *) MAKE_PTR(location);
  IpcSemaphoreKill(proc->sem.semKey);
  return(TRUE);
}

/*
 * ProcKill() -- Destroy the per-proc data structure for
 *	this process. Release any of its held spin locks.
 */
bool
ProcKill(exitStatus, pid)
int exitStatus;
int pid;
{
  PROC 		*proc;
  SHMEM_OFFSET	location,ShmemPIDDestroy();

  /* -------------------- 
   * If this is a FATAL exit the postmaster will have to kill all the existing
   * backends and reinitialize shared memory.  So all we don't need to do
   * anything here.
   * --------------------
   */
  if (exitStatus != 0)
    return(TRUE);

  if (! pid)
  {
    pid = getpid();
  }

  location = ShmemPIDDestroy(pid);
  if (location == INVALID_OFFSET)
    return(FALSE);

  proc = (PROC *) MAKE_PTR(location);

  if (proc != MyProc)
    Assert( pid != getpid() );
  else
    MyProc = NULL;
    
  /* ---------------
   * Assume one lock table.
   * ---------------
   */
  ProcReleaseSpins(proc);
  LockReleaseAll(1,&proc->lockQueue);

  /* ----------------
   * get off the wait queue
   * ----------------
   */
  if (proc->links.next != INVALID_OFFSET)
    SHMQueueDelete(&(proc->links));
  SHMQueueElemInit(&(proc->links));

  SpinAcquire(ProcStructLock);

  /* ----------------------
   * If ProcGlobal hasn't been initialized (i.e. in the postmaster)
   * attach to it.
   * ----------------------
   */
  if (!ProcGlobal)
  {
    bool found = false;

    ProcGlobal = (PROC_HDR *)
      ShmemInitStruct("Proc Header",(unsigned)sizeof(PROC_HDR),&found);
    if (!found)
      elog(NOTICE, "ProcKill: Couldn't find ProcGlobal in the binding table");
    ProcGlobal = (PROC_HDR *)NULL;
    return(FALSE);
  }
  proc->links.next =  ProcGlobal->freeProcs;
  ProcGlobal->freeProcs = MAKE_OFFSET(proc);

  SpinRelease(ProcStructLock);
  return(TRUE);
}

/*
 * ProcQueue package: routines for putting processes to sleep
 * 	and  waking them up
 */

/*
 * ProcQueueAlloc -- alloc/attach to a shared memory process queue
 *
 * Returns: a pointer to the queue or NULL
 * Side Effects: Initializes the queue if we allocated one
 */
PROC_QUEUE *
ProcQueueAlloc(name)
char *name;
{
  Boolean	found;
  PROC_QUEUE *queue = (PROC_QUEUE *)
    ShmemInitStruct(name,(unsigned)sizeof(PROC_QUEUE),&found);

  if (! queue)
  {
    return(NULL);
  }
  if (! found)
  {
    ProcQueueInit(queue);
  }
  return(queue);
}
/*
 * ProcQueueInit -- initialize a shared memory process queue
 */
void
ProcQueueInit(queue)
PROC_QUEUE *queue;
{
  SHMQueueInit(&(queue->links));
  queue->size = 0;
}



/*
 * ProcSleep -- put a process to sleep
 *
 * P() on the semaphore should put us to sleep.  The process
 * semaphore is cleared by default, so the first time we try
 * to acquire it, we sleep.
 *
 * ASSUME: that no one will fiddle with the queue until after
 * 	we release the spin lock.
 *
 * NOTES: The process queue is now a priority queue for locking.
 */
int
ProcSleep(queue, spinlock, token, prio, lock)
PROC_QUEUE	*queue;
SPINLOCK 	spinlock;
int		token;
int		prio;
LOCK *		lock;
{
  int 	i;
  PROC	*proc;
  struct itimerval timeval, dummy;

  proc = (PROC *) MAKE_PTR(queue->links.prev);
  for (i=0;i<queue->size;i++)
  {
    if (proc->prio < prio)
      proc = (PROC *) MAKE_PTR(proc->links.prev);
    else
      break;
  }

  MyProc->token = token;
  MyProc->waitLock = lock;

  /* -------------------
   * currently, we only need this for the ProcWakeup routines
   * -------------------
   */
  TransactionIdStore((TransactionId) GetCurrentTransactionId(), &MyProc->xid);

  /* -------------------
   * assume that these two operations are atomic (because
   * of the spinlock).
   * -------------------
   */
  SHMQueueInsertTL(&(proc->links),&(MyProc->links));
  queue->size++;

  SpinRelease(spinlock);

  /* --------------
   * Postgres does not have any deadlock detection code and for this 
   * reason we must set a timer to wake up the process in the event of
   * a deadlock.  For now the timer is set for 1 minute and we assume that
   * any process which sleeps for this amount of time is deadlocked and will 
   * receive a SIGALRM signal.  The handler should release the processes
   * semaphore and abort the current transaction.
   *
   * Need to zero out struct to set the interval and the micro seconds fields
   * to 0.
   * --------------
   */
  bzero(&timeval, sizeof(struct itimerval));
  timeval.it_value.tv_sec = 60;

  if (setitimer(ITIMER_REAL, &timeval, &dummy))
	elog(FATAL, "ProcSleep: Unable to set timer for process wakeup");

  /* --------------
   * if someone wakes us between SpinRelease and IpcSemaphoreLock,
   * IpcSemaphoreLock will not block.  The wakeup is "saved" by
   * the semaphore implementation.
   * --------------
   */
  IpcSemaphoreLock(MyProc->sem.semId, MyProc->sem.semNum, IpcExclusiveLock);

  /* ---------------
   * We were awoken before a timeout - now disable the timer
   * ---------------
   */
  timeval.it_value.tv_sec = 0;


  if (setitimer(ITIMER_REAL, &timeval, &dummy))
	elog(FATAL, "ProcSleep: Unable to diable timer for process wakeup");

  /* ----------------
   * We were assumed to be in a critical section when we went
   * to sleep.
   * ----------------
   */
  SpinAcquire(spinlock);

  return(MyProc->errType);
}


/*
 * ProcWakeup -- wake up a process by releasing its private semaphore.
 *
 *   remove the process from the wait queue and set its links invalid.
 *   RETURN: the next process in the wait queue.
 */
PROC *
ProcWakeup(proc, errType)
PROC	*proc;
int	errType;
{
  PROC *retProc;
  /* assume that spinlock has been acquired */

  retProc = (PROC *) MAKE_PTR(proc->links.prev);
  SHMQueueDelete(&(proc->links));
  SHMQueueElemInit(&(proc->links));

  proc->errType = errType;

  IpcSemaphoreUnlock(proc->sem.semId, proc->sem.semNum, IpcExclusiveLock);

  return retProc;
}


/*
 * ProcGetId --
 */
int
ProcGetId()
{
  return( MyProc->procId );
}

/*
 * ProcLockWakeup -- routine for waking up processes when a lock is
 * 	released.
 */
int
ProcLockWakeup( queue, ltable, lock)
PROC_QUEUE	*queue;
char *		ltable;
char *		lock;
{
  PROC	*proc;
  int	count;

  if (! queue->size)
    return(STATUS_NOT_FOUND);

  proc = (PROC *) MAKE_PTR(queue->links.prev);
  count = 0;
  while ((LockResolveConflicts ((LOCKTAB *) ltable,
				(LOCK *) lock,
				proc->token,
				proc->xid,
				proc->pid) == STATUS_OK))
  {
    /* there was a waiting process, grant it the lock before waking it
     * up.  This will prevent another process from seizing the lock
     * between the time we release the lock master (spinlock) and
     * the time that the awoken process begins executing again.
     */
    GrantLock((LOCK *) lock, proc->token);
    queue->size--;
    /*
     * ProcWakeup removes proc from the lock waiting process queue and
     * returns the next proc in chain.  If a writer just dropped
     * its lock and there are several waiting readers, wake them all up.
     */
    proc = ProcWakeup(proc, NO_ERROR);

    count++;
    if (queue->size == 0)
      break;
  }

  if (count)
    return(STATUS_OK);
  else
    /* Something is still blocking us.  May have deadlocked. */
    return(STATUS_NOT_FOUND);
}

void
ProcAddLock(elem)
SHM_QUEUE	*elem;
{
  SHMQueueInsertTL(&MyProc->lockQueue,elem);
}

/* --------------------
 * We only get to this routine I sleep for a minute 
 * waiting for a lock to be released by some other process.  After
 * the one minute deadline we assume we have a deadlock and must abort
 * this transaction.  We must also indicate that I'm no longer waiting
 * on a lock so that other processes don't try to wake me up and screw 
 * up my semaphore.
 * --------------------
 */
int
HandleDeadLock()
{
  LOCK *lock;
  LOCKT lockt;

  LockLockTable();

  /* ---------------------
   * Check to see if we've been awoken by anyone in the interim.
   *
   * If we have we can return and resume our transaction -- happy day.
   * Before we are awoken the process releasing the lock grants it to
   * us so we know that we don't have to wait anymore.
   * 
   * Damn these names are LONG! -mer
   * ---------------------
   */
  if (IpcSemaphoreGetCount(MyProc->sem.semId, MyProc->sem.semNum) == 
                                                IpcSemaphoreDefaultStartValue)
  {
	UnlockLockTable();
	return 1;
  }

  lock = MyProc->waitLock;
  lockt = (LOCKT) MyProc->token;

  /* ----------------------
   * Decrement the holders fields since we can wait no longer.
   * the name hodlers is misleading, it represents the sum of the number
   * of active holders (those who have acquired the lock), and the number
   * of waiting processes trying to acquire the lock.
   * ----------------------
   */
  LockDecrWaitHolders(lock, lockt);

  /* ------------------------
   * Get this process off the lock's wait queue
   * ------------------------
   */
  SHMQueueDelete(&MyProc->links);
  SHMQueueElemInit(&(MyProc->links));
  lock->waitProcs.size--;

  UnlockLockTable();

  /* ------------------
   * Unlock my semaphore, so that the count is right for next time.
   * I was awoken by a signal not by someone unlocking my semaphore.
   * ------------------
   */
  IpcSemaphoreUnlock(MyProc->sem.semId, MyProc->sem.semNum, IpcExclusiveLock);

  elog(NOTICE, "Timeout -- possible deadlock");
  /* -------------
   * Set MyProc->errType to STATUS_ERROR so that we abort after
   * returning from this handler.
   * -------------
   */
  MyProc->errType = STATUS_ERROR;
}

void
ProcReleaseSpins(proc)
    PROC *proc;
{
    int i;

    if (!proc)
	proc = MyProc;

    if (!proc)
	return;
    for (i=0; i < (int)MAX_SPINS; i++)
    {
	if (proc->sLocks[i])
	{
	    Assert(proc->sLocks[i] == 1);
	    SpinRelease(i);
	}
    }
}
