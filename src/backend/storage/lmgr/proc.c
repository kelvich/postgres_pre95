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

#include "storage/shmem.h"
#include "storage/spin.h"
#include "storage/proc.h"
#include "storage/lock.h"
#include "utils/hsearch.h"
#include "storage/buf.h"	
#include "utils/log.h"

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
InitProcess(key)
IPCKey key;
{
  bool found = false;
  int pid;
  int semstat;
  unsigned int location, myOffset;

  /* attach to the free list */
  ProcGlobal = (PROC_HDR *)
    ShmemInitStruct("Proc Header",(unsigned)sizeof(PROC_HDR),&found);

  /* --------------------
   * We're the first - initialize. Postmaster will always be 1st
   * so there's no worry about locking here.
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
    elog(WARN,"ProcInit: you already exist");
    return;
  }

  /* -----------------
   * Critical section ... want to read and update ProcGlobal
   *
   * Acquiring the spin lock here bothers me as we go off and do a bunch
   * of stuff including allocating shared memory which fiddles with another
   * spin lock.  This just seemed like an invitation for deadlock, so in
   * the else clause of this if I release and re-acquire the lock to try
   * to avoid these problems, at the expense of some extra overhead.
   * mer 18 July 1991
   * -----------------
   */
  SpinAcquire(ProcStructLock);

  /* try to get a proc from the free list first */

  myOffset = ProcGlobal->freeProcs;

  if (myOffset != INVALID_OFFSET)
  {
    MyProc = (PROC *) MAKE_PTR(myOffset);
    ProcGlobal->freeProcs = MyProc->links.next;

    MyProc->sem.semId = IpcSemaphoreCreate(ProcGlobal->currKey,
                                           1,
                                           IPCProtection,
                                           IpcSemaphoreDefaultStartValue,
                                           &semstat);
    IpcSemaphoreLock(MyProc->sem.semId, 0, IpcExclusiveLock);
  }
  else
  {
    /* ----------------
     * release the lock before going and getting shared memory
     * ----------------
     */
    SpinRelease(ProcStructLock);

    /* have to allocate one.  We can't use the normal binding
     * table mechanism because the proc structure is stored
     * by PID instead of by a global name (need to look it
     * up by PID when we cleanup dead processes).
     */

    MyProc = (PROC *) ShmemAlloc((unsigned)sizeof(PROC));
    if (! MyProc)
    {
      elog (FATAL,"cannot create new proc: out of memory");
    }

    /* this cannot be initialized until after the buffer pool */
    SHMQueueInit(&(MyProc->lockQueue));

    /* ------------------
     * Reacquire the lock before modifying ProcGlobal
     * ------------------
     */
    SpinAcquire(ProcStructLock);

    MyProc->procId = ProcGlobal->numProcs;
    ProcGlobal->numProcs++;
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
    ProcGlobal->currKey++;

  }

  /* ----------------------
   * Release the lock once and for all
   * ----------------------
   */
  SpinRelease(ProcStructLock);

  MyProc->sem.semNum = 0;
  MyProc->sem.semKey = key;

  /* ------------------
   * Bump the current key up by one.  The next process then creates
   * a semaphore with a unique key.
   * ------------------
   */
  ProcGlobal->currKey++;

  /* -------------------------
   * Install ourselves in the binding table.  The name to
   * use is determined by the OS-assigned process id.  That
   * allows the cleanup process to find us after any untimely
   * exit.
   * -------------------------
   */
  pid = getpid();
  location = myOffset;
  if ((! ShmemPIDLookup(pid,&location)) || (location != myOffset))
  {
    elog(FATAL,"InitProc: ShmemPID table broken");
  }

  MyProc->errType = NO_ERROR;
  SHMQueueElemInit(&(MyProc->links));

  ProcInitialized = TRUE;
}

/*
 * ProcReleaseLocks() -- release all locks associated with this process
 *
 */
ProcReleaseLocks()
{
  LockReleaseAll(1,&MyProc->lockQueue);
}

/*
 * ProcKill() -- Destroy the per-proc data structure for
 *	this process.
 */
ProcKill(pid)
int pid;
{
  PROC 		*proc;
  SHMEM_OFFSET	location,ShmemPIDDestroy();

  if (! ProcInitialized)
    return;

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
   * assume one lock table
   * ---------------
   */
  LockReleaseAll(1,&MyProc->lockQueue);

  /* ----------------
   * toss the wait queue
   * ----------------
   */
  IpcSemaphoreKill((proc->sem.semKey));
  if (proc->links.next != INVALID_OFFSET)
    SHMQueueDelete(&(proc->links));
  proc->links.next =  ProcGlobal->freeProcs;
  ProcGlobal->freeProcs = MAKE_OFFSET(proc);

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
ProcSleep(queue,spinlock,token,prio)
PROC_QUEUE	*queue;
SPINLOCK 	spinlock;
int		token;
int		prio;
{
  int 	i;
  PROC	*proc;

  proc = (PROC *) MAKE_PTR(queue->links.prev);
  for (i=0;i<queue->size;i++)
  {
    if (proc->prio < prio)
      proc = (PROC *) MAKE_PTR(proc->links.prev);
    else
      break;
  }

  MyProc->token = token;

  /* -------------------
   * currently, we only need this for the ProcWakeup routines
   * -------------------
   */
  TransactionIdStore(GetCurrentTransactionId(), &MyProc->xid);

  /* -------------------
   * assume that these two operations are atomic (because
   * of the spinlock).
   * -------------------
   */
  SHMQueueInsertTL(&(proc->links),&(MyProc->links));
  queue->size++;

  SpinRelease(spinlock);

  /* --------------
   * if someone wakes us between SpinRelease and IpcSemaphoreLock,
   * IpcSemaphoreLock will not block.  The wakeup is "saved" by
   * the semaphore implementation.
   * --------------
   */
  IpcSemaphoreLock(MyProc->sem.semId, MyProc->sem.semNum, IpcExclusiveLock);

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
 */
ProcWakeup(proc, errType)
PROC	*proc;
int	errType;
{
  /* assume that spinlock has been acquired */

  SHMQueueDelete(&(proc->links));

  proc->errType = errType;

  IpcSemaphoreUnlock(proc->sem.semId, proc->sem.semNum, IpcExclusiveLock);
}


/*
 * ProcGetId --
 */
ProcGetId()
{
  return( MyProc->procId );
}

/*
 * ProcLockWakeup -- routine for waking up processes when a lock is
 * 	released.
 */
ProcLockWakeup( queue, ltable, lock)
PROC_QUEUE	*queue;
Address		ltable;
Address		lock;
{
  PROC	*proc;
  int	count;

  if (! queue->size)
    return(STATUS_NOT_FOUND);

  proc = (PROC *) MAKE_PTR(queue->links.prev);
  count = 0;
  while ((LockResolveConflicts ( ltable, lock, proc->token, &proc->xid )
	  == STATUS_OK))
  {
    /* there was a waiting process, grant it the lock before waking it
     * up.  This will prevent another process from seizing the lock
     * between the time we release the lock master (spinlock) and
     * the time that the awoken process begins executing again.
     */
    GrantLock(lock, proc->token);
    queue->size--;
    ProcWakeup(proc, NO_ERROR);
    /* get next proc in chain.  If a writer just dropped its lock
     * and there are several waiting readers, wake them all up.
     */
    proc = (PROC *) MAKE_PTR(proc->links.prev);

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

ProcAddLock(elem)
SHM_QUEUE	*elem;
{
  SHMQueueInsertTL(&MyProc->lockQueue,elem);
}
