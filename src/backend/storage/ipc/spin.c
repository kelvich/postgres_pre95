/*
 * spin.c -- routines for managing spin locks
 *
 * Identification:
 *	$Header$
 *
 * POSTGRES has two kinds of locks: semaphores (which put the
 * process to sleep) and spinlocks (which are supposed to be
 * short term locks).  Currently both are implemented as SysV
 * semaphores, but presumably this can change if we move to
 * a machine with a test-and-set (TAS) instruction.  Its probably
 * a good idea to think about (and allocate) short term and long
 * term semaphores separately anyway.
 *
 */
#include <errno.h>
#include "tmp/postgres.h"
#include "storage/ipc.h"
#include "storage/ipci.h"
#include "storage/shmem.h"
#include "storage/spin.h"
#include "utils/log.h"

/* this spin lock guards the data structures for allocating and
 * deallocating spinlocks and semaphores
 */
SPINLOCK *SpinMaster = NULL;

/* This is configured in the kernel.  maximum number of
 * spin locks can be no more than the max size of a SysV
 * semaphore array (if spinlocks are implemented as SysV 
 * semaphores.  SpinLockId is the identifier for the semaphore
 * array.
 */
#define MAX_SPINS 5
IpcSemaphoreId  SpinLockId = -1;

	/* count of allocated spinlocks */
int *	NumSpinLocksP = NULL;



/* if debugging, spinlocks are simply integers */
#ifdef SPIN_DBG

CreateSpinlocks()
{ return(TRUE); }

AttachSpinLocks()
{ return(TRUE); }

#else 
#ifdef SPIN_OK

/* SPIN_OK means spinlocks are implemented with TAS */
CreateSpinlocks()
{ return(TRUE); }

AttachSpinLocks()
{ return(TRUE); }

#else
/* Spinlocks are implemented using SysV semaphores */


/*
 * SpinAcquire -- try to grab a spinlock
 *
 * FAILS if the semaphore is corrupted.
 */
SpinAcquire(lock) 
SPINLOCK	*lock;
{
  IpcSemaphoreLock(SpinLockId, lock->tas, IpcExclusiveLock);
}

/*
 * SpinRelease -- release a spin lock
 * 
 * FAILS if the semaphore is corrupted
 */
SpinRelease(lock) 
SPINLOCK	*lock;
{
  if (SpinIsLocked(lock))
      IpcSemaphoreUnlock(SpinLockId, lock->tas, IpcExclusiveLock);
}

int
SpinIsLocked(lock)
SPINLOCK	*lock;
{
   int semval;

   semval = IpcSemaphoreGetValue(SpinLockId, lock->tas);
   return(semval < IpcSemaphoreDefaultStartValue);
}

/*
 * SpinCreate -- initialize a spin lock 
 *
 * There are a finite number of spinlocks available.  Check
 * that the number has not been exceeded.  The semaphore is
 * locked when it is created.  
 */
SpinCreate(lock)
SPINLOCK	*lock;
{
  lock->tas = *NumSpinLocksP;

  if (*NumSpinLocksP >= MAX_SPINS) {
    elog(WARN,"SpinCreate: too many spinlocks ");
  }
}

/*
 * CreateSpinlocks -- Create a sysV semaphore array for
 *	the spinlocks
 *
 */
CreateSpinlocks(key)
IPCKey key;
{

  int status;
  IpcSemaphoreId semid;
  semid = IpcSemaphoreCreate(key, MAX_SPINS, IPCProtection, 
			     IpcSemaphoreDefaultStartValue, &status);
  if (status == IpcSemIdExist) {
    IpcSemaphoreKill(key);
    elog(NOTICE,"Destroying old spinlock semaphore");
    semid = IpcSemaphoreCreate(key, MAX_SPINS, IPCProtection, 
			       0, &status);
    }

  if (semid >= 0) {
    SpinLockId = semid;
    return(TRUE);
  }
  /* cannot create spinlocks */
  elog(FATAL,"CreateSpinlocks: cannot create spin locks");
  return(FALSE);
}

/*
 * Attach to existing spinlock set
 */
AttachSpinLocks(key)
IPCKey key;
{
  IpcSemaphoreId id;

  id = semget (key, MAX_SPINS, 0);
  if (id < 0) {
    if (errno == EEXIST) {
      /* key is the name of someone else's semaphore */
      elog (FATAL,"AttachSpinlocks: SPIN_KEY belongs to someone else");
    }
    /* cannot create spinlocks */
    elog(FATAL,"AttachSpinlocks: cannot create spin locks");
    return(FALSE);
  }
  SpinLockId = id;
  return(TRUE);
}
#endif
#endif

/*
 * InitSpinLocks -- Spinlock bootstrapping
 * 
 * We need three spinlocks for bootstrapping SpinMaster (this
 * module), BindingLock (for the shmem binding table) and
 * ShmemLock (for the shmem allocator).
 *
 */
InitSpinLocks(init, freeSpaceP, key)
int init;
unsigned int *freeSpaceP;
IPCKey key;
{
  extern SPINLOCK *ShmemLock;
  extern SPINLOCK *BindingLock;

  if (!init || key != IPC_PRIVATE) {
      /* if bootstrap and key is IPC_PRIVATE, it means that we are running
       * backend by itself.  no need to attach spinlocks
       */
      if (! AttachSpinLocks(key)) {
        elog(FATAL,"InitSpinLocks: couldnt attach spin locks");
        return(FALSE);
      }
    }

  NumSpinLocksP = (int *) MAKE_PTR(*freeSpaceP);

  /* These three spinlocks have fixed location is shmem */
  ShmemLock = (SPINLOCK *) (NumSpinLocksP+1);
  BindingLock = ShmemLock+1;
  SpinMaster = BindingLock+1;
  
  *freeSpaceP = (int ) MAKE_OFFSET(SpinMaster+1);

  /* if ! init, just not the location of these.  Someone
   * else has initialized them.
   */
  if (init) {
    *NumSpinLocksP = 0;

    SpinCreate(ShmemLock);
    *NumSpinLocksP = 1;
    SpinRelease(ShmemLock);

    SpinCreate(BindingLock);
    *NumSpinLocksP = 2;
    SpinRelease(BindingLock);

    SpinCreate(SpinMaster);
    *NumSpinLocksP = 3;
    SpinRelease(SpinMaster);
  }

  return(TRUE);
}


/*
 * SpinAlloc -- allocate a new spinlock or attach to an
 *	old one.
 *
 * Before any shared memory data structure is initialized,
 * This routine should be called to initialize the spinlock
 * for that data structure.
 *
 * Side Effects: The spinlock is already locked when it
 * 	is allocated.
 */
SPINLOCK *
SpinAlloc(name)
char *name;
{
  SPINLOCK *	spinlock;
  Boolean	found = FALSE;

  SpinAcquire(SpinMaster);

  spinlock = (SPINLOCK *) 
    ShmemInitStruct(name,sizeof(SPINLOCK),&found);

  if (! found) {
    SpinCreate(spinlock);
    ++(*NumSpinLocksP);
  } 

  SpinRelease(SpinMaster);

  SpinAcquire(spinlock);

  return(spinlock);
}
