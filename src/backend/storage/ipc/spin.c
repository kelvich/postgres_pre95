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
 * NOTE: These routines are not supposed to be widely used in Postgres.
 *	 They are preserved solely for the purpose of porting Mark Sullivan's
 *	 buffer manager to Postgres.
 */
#include <errno.h>
#include "tmp/postgres.h"
#include "storage/ipci.h"
#include "storage/ipc.h"
#include "storage/shmem.h"
#include "storage/spin.h"
#include "storage/proc.h"
#include "utils/log.h"

/* globals used in this file */
IpcSemaphoreId	SpinLockId;

#ifdef HAS_TEST_AND_SET
/* real spin lock implementations */

CreateSpinlocks(key)
IPCKey key;
{ 
   /* the spin lock shared memory must have been created by now */
   return(TRUE); 
}

AttachSpinLocks(key)
{
    /* the spin lock shared memory must have been attached by now */
    return(TRUE);
}

InitSpinLocks(init, key)
int init;
IPCKey key;
{
  extern SPINLOCK ShmemLock;
  extern SPINLOCK BindingLock;
  extern SPINLOCK BufMgrLock;
  extern SPINLOCK LockMgrLock;
  extern SPINLOCK ProcStructLock;
  extern SPINLOCK SInvalLock;
  extern SPINLOCK OidGenLockId;

#ifdef SONY_JUKEBOX
  extern SPINLOCK SJCacheLock;
  extern SPINLOCK JBSpinLock;
#endif /* SONY_JUKEBOX */

#ifdef MAIN_MEMORY
  extern SPINLOCK MMCacheLock;
#endif /* SONY_JUKEBOX */

  /* These six spinlocks have fixed location is shmem */
  ShmemLock = (SPINLOCK) SHMEMLOCKID;
  BindingLock = (SPINLOCK) BINDINGLOCKID;
  BufMgrLock = (SPINLOCK) BUFMGRLOCKID;
  LockMgrLock = (SPINLOCK) LOCKMGRLOCKID;
  ProcStructLock = (SPINLOCK) PROCSTRUCTLOCKID;
  SInvalLock = (SPINLOCK) SINVALLOCKID;
  OidGenLockId = (SPINLOCK) OIDGENLOCKID;

#ifdef SONY_JUKEBOX
  SJCacheLock = (SPINLOCK) SJCACHELOCKID;
  JBSpinLock = (SPINLOCK) JBSPINLOCKID;
#endif /* SONY_JUKEBOX */

#ifdef MAIN_MEMORY
  MMCacheLock = (SPINLOCK) MMCACHELOCKID;
#endif /* MAIN_MEMORY */

  return(TRUE);
}

SpinAcquire(lock)
SPINLOCK lock;
{
    ExclusiveLock(lock);
    PROC_INCR_SLOCK(lock);
}

SpinRelease(lock)
SPINLOCK lock;
{
    PROC_DECR_SLOCK(lock);
    ExclusiveUnlock(lock);
}

SpinIsLocked(lock)
SPINLOCK lock;
{
    return(!LockIsFree(lock));
}
#else /* HAS_TEST_AND_SET */
/* Spinlocks are implemented using SysV semaphores */


/*
 * SpinAcquire -- try to grab a spinlock
 *
 * FAILS if the semaphore is corrupted.
 */
SpinAcquire(lock) 
SPINLOCK	lock;
{
  IpcSemaphoreLock(SpinLockId, lock, IpcExclusiveLock);
  PROC_INCR_SLOCK(lock);
}

/*
 * SpinRelease -- release a spin lock
 * 
 * FAILS if the semaphore is corrupted
 */
SpinRelease(lock) 
SPINLOCK	lock;
{
  Assert(SpinIsLocked(lock))
  PROC_DECR_SLOCK(lock);
  IpcSemaphoreUnlock(SpinLockId, lock, IpcExclusiveLock);
}

int
SpinIsLocked(lock)
SPINLOCK	lock;
{
   int semval;

   semval = IpcSemaphoreGetValue(SpinLockId, lock);
   return(semval < IpcSemaphoreDefaultStartValue);
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
			       IpcSemaphoreDefaultStartValue, &status);
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

/*
 * InitSpinLocks -- Spinlock bootstrapping
 * 
 * We need several spinlocks for bootstrapping:
 * BindingLock (for the shmem binding table) and
 * ShmemLock (for the shmem allocator), BufMgrLock (for buffer
 * pool exclusive access), LockMgrLock (for the lock table), and
 * ProcStructLock (a spin lock for the shared process structure).
 * If there's a Sony WORM drive attached, we also have a spinlock
 * (SJCacheLock) for it.  Same story for the main memory storage mgr.
 *
 */
InitSpinLocks(init, key)
int init;
IPCKey key;
{
  extern SPINLOCK ShmemLock;
  extern SPINLOCK BindingLock;
  extern SPINLOCK BufMgrLock;
  extern SPINLOCK LockMgrLock;
  extern SPINLOCK ProcStructLock;
  extern SPINLOCK SInvalLock;
  extern SPINLOCK OidGenLockId;

#ifdef SONY_JUKEBOX
  extern SPINLOCK SJCacheLock;
  extern SPINLOCK JBSpinLock;
#endif /* SONY_JUKEBOX */

#ifdef MAIN_MEMORY
  extern SPINLOCK MMCacheLock;
#endif /* MAIN_MEMORY */

  if (!init || key != IPC_PRIVATE) {
      /* if bootstrap and key is IPC_PRIVATE, it means that we are running
       * backend by itself.  no need to attach spinlocks
       */
      if (! AttachSpinLocks(key)) {
        elog(FATAL,"InitSpinLocks: couldnt attach spin locks");
        return(FALSE);
      }
    }

  /* These five (or six) spinlocks have fixed location is shmem */
  ShmemLock = (SPINLOCK) SHMEMLOCKID;
  BindingLock = (SPINLOCK) BINDINGLOCKID;
  BufMgrLock = (SPINLOCK) BUFMGRLOCKID;
  LockMgrLock = (SPINLOCK) LOCKMGRLOCKID;
  ProcStructLock = (SPINLOCK) PROCSTRUCTLOCKID;
  SInvalLock = (SPINLOCK) SINVALLOCKID;
  OidGenLockId = (SPINLOCK) OIDGENLOCKID;

#ifdef SONY_JUKEBOX
  SJCacheLock = (SPINLOCK) SJCACHELOCKID;
  JBSpinLock = (SPINLOCK) JBSPINLOCKID;
#endif /* SONY_JUKEBOX */

#ifdef MAIN_MEMORY
  MMCacheLock = (SPINLOCK) MMCACHELOCKID;
#endif /* MAIN_MEMORY */
  
  return(TRUE);
}
#endif /* HAS_TEST_AND_SET */
