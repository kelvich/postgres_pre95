/*
 *   FILE
 *	ipc.h
 *
 *   DESCRIPTION
 *	POSTGRES inter-process communication definitions.
 *
 *   NOTES
 *	This file is very architecture-specific.  This stuff should actually
 *	be factored into the port/ directories.
 *
 *   IDENTIFICATION
 *	$Header$
 */

#ifndef	IPCIncluded	/* Include this file only once */
#define IPCIncluded	1

#include "tmp/c.h"

/*
 * Many architectures have support for user-level spinlocks (i.e., an
 * atomic test-and-set instruction).  However, we have only written
 * spinlock code for the architectures listed.
 *
 * This should also be done for:
 *	Digital Alpha AXP
 *	Intel 80386
 *	H-P PA-RISC
 * since these also have atomic test-and-set instructions.
 */
#if defined(PORTNAME_aix) || \
    defined(PORTNAME_sparc) || \
    defined(sequent) || \
    defined(m68k) || \
    defined(mc68020) || \
    defined(mc68030) || \
    defined(mc68040)
#define HAS_TEST_AND_SET
#endif

#if defined(HAS_TEST_AND_SET)
#if defined(sequent)
/*
 * Sequent has special hardware locks (see below) and therefore does 
 * not need a special slock_t.
 */
#else
#if defined(PORTNAME_next)
/*
 * Use Mach mutex routines since these are, in effect, test-and-set
 * spinlocks.
 */
#undef NEVER	/* definition in cthreads.h conflicts with parse.h */
#include <mach/cthreads.h>

typedef struct mutex	slock_t;

#define S_LOCK(lock)		mutex_lock(lock)
#define S_CLOCK(lock)		mutex_try_lock(lock)
#define S_UNLOCK(lock)		mutex_unlock(lock)
#define S_INIT_LOCK(lock)	mutex_init(lock)
#else /* next */
#if defined(PORTNAME_aix)
/*
 * The AIX C library has the cs(3) builtin for compare-and-set that 
 * operates on ints.
 */
typedef unsigned int	slock_t;
#else /* aix */
/*
 * On all other architectures spinlocks are a single byte.
 */
typedef unsigned char   slock_t;
#endif /* aix */
#endif /* next */
#endif /* sequent */
#endif /* HAS_TEST_AND_SET */

/*
 * On architectures for which we have not implemented spinlocks (or
 * cannot do so), we use System V semaphores.  For some reason union
 * semun is never defined in the System V header files so we must
 * do it ourselves.
 */
#if defined(sequent) || \
    defined(PORTNAME_aix) || \
    defined(PORTNAME_alpha) || \
    defined(PORTNAME_hpux) || \
    defined(PORTNAME_ultrix4) || \
    defined(PORTNAME_bsd44)
union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};
#endif

typedef uint16	SystemPortAddress;

/* semaphore definitions */

#define IPCProtection	(0600)		/* access/modify by user only */

#define IPC_NMAXSEM	25		/* maximum number of semaphores */
#define IpcSemaphoreDefaultStartValue	255
#define IpcSharedLock					(-1)
#define IpcExclusiveLock			  (-255)

#define IpcUnknownStatus	(-1)
#define IpcInvalidArgument	(-2)
#define IpcSemIdExist		(-3)
#define IpcSemIdNotExist	(-4)

typedef uint32	IpcSemaphoreKey;		/* semaphore key */
typedef int	IpcSemaphoreId;


/* shared memory definitions */ 

#define IpcMemCreationFailed	(-1)
#define IpcMemIdGetFailed	(-2)
#define IpcMemAttachFailed	0


typedef uint32  IpcMemoryKey;			/* shared memory key */
typedef int	IpcMemoryId;


/* ipc.c */
extern IpcSemaphoreId IpcSemaphoreCreate ARGS((IpcSemaphoreKey semKey,
					       int semNum,
					       int permission,
					       int semStartValue,
					       int *status));
extern void IpcSemaphoreKill ARGS((IpcSemaphoreKey key));
extern void IpcSemaphoreLock ARGS((IpcSemaphoreId semId,
				   int sem,
				   int lock));
extern void IpcSemaphoreUnlock ARGS((IpcSemaphoreId semId,
				     int sem,
				     int lock));
extern int IpcSemaphoreGetCount ARGS((IpcSemaphoreId semId,
				      int sem));
extern int IpcSemaphoreGetValue ARGS((IpcSemaphoreId semId,
				      int sem));
extern IpcMemoryId IpcMemoryCreate ARGS((IpcMemoryKey memKey,
					 uint32 size,
					 int permission));
extern IpcMemoryId IpcMemoryIdGet ARGS((IpcMemoryKey memKey,
					uint32 size));
extern void IpcMemoryDetach ARGS((int status, char *shmaddr));
extern char *IpcMemoryAttach ARGS((IpcMemoryId memId));
extern void IpcMemoryKill ARGS((IpcMemoryKey memKey));

/* XXX invalid prototype style */
extern int on_exitpg();

#ifdef sequent
/* ------------------
 *  use hardware locks to replace semaphores for sequent machines
 *  to avoid costs of swapping processes and to provide unlimited
 *  supply of locks.
 *
 *  The file "parallel/parallel.h" on sequent has S_LOCK defined - for
 *  other machines, it is defined in src/storage/ipc/s_lock.c
 * ------------------
 */

#include <parallel/parallel.h>
#endif

#ifdef HAS_TEST_AND_SET

#define NSLOCKS		2048
#define	NOLOCK		0
#define SHAREDLOCK	1
#define EXCLUSIVELOCK	2

typedef enum _LockId_ {
    BUFMGRLOCKID,
    LOCKLOCKID,
    OIDGENLOCKID,
    SHMEMLOCKID,
    BINDINGLOCKID,
    LOCKMGRLOCKID,
    SINVALLOCKID,

#ifdef SONY_JUKEBOX
    SJCACHELOCKID,
    JBSPINLOCKID,
#endif /* SONY_JUKEBOX */

#ifdef MAIN_MEMORY
    MMCACHELOCKID,
#endif /* MAIN_MEMORY */

    PROCSTRUCTLOCKID,
    FIRSTFREELOCKID
} _LockId_;

#define MAX_SPINS	FIRSTFREELOCKID

typedef struct slock {
    slock_t		locklock;
    unsigned char	flag;
    short		nshlocks;
    slock_t		shlock;
    slock_t		exlock;
    slock_t		comlock;
    struct slock	*next;
} SLock;

extern void CreateAndInitSLockMemory ARGS((IPCKey key));
extern void AttachSLockMemory ARGS((IPCKey key));
extern int CreateLock ARGS((void));
extern void RelinquishLock ARGS((int lockid));
extern void SharedLock ARGS((int lockid));
extern void SharedUnlock ARGS((int lockid));
extern void ExclusiveLock ARGS((int lockid));
extern void ExclusiveUnlock ARGS((int lockid));
extern bool LockIsFree ARGS((int lockid));
#else /* HAS_TEST_AND_SET */

typedef enum _LockId_ {
    SHMEMLOCKID,
    BINDINGLOCKID,
    BUFMGRLOCKID,
    LOCKMGRLOCKID,
    SINVALLOCKID,

#ifdef SONY_JUKEBOX
    SJCACHELOCKID,
    JBSPINLOCKID,
#endif /* SONY_JUKEBOX */

#ifdef MAIN_MEMORY
    MMCACHELOCKID,
#endif /* MAIN_MEMORY */

    PROCSTRUCTLOCKID,
    OIDGENLOCKID,
    FIRSTFREELOCKID
} _LockId_;

#define MAX_SPINS	FIRSTFREELOCKID

#endif /* HAS_TEST_AND_SET */

#include "storage/execipc.h"
#endif	/* !defined(IPCIncluded) */
