/*
 * ipc.h --
 *	POSTGRES inter-process communication definitions.
 *
 * Identification:
 *	$Header$
 */

#ifndef	IPCIncluded	/* Include this file only once */
#define IPCIncluded	1

#include "tmp/c.h"

#if defined(sequent) || defined(sparc) || defined(m68k) || defined(mc68020)
#define HAS_TEST_AND_SET
#endif

/*
 * for these, use the mc68020 tas instruction
 */

#if defined(mc68030) || defined(mc68040) || defined(amiga)
#define HAS_TEST_AND_SET
#endif

#if defined(HAS_TEST_AND_SET) && !defined(sequent)
typedef unsigned char   slock_t;
#endif

#if defined(sequent) || defined(mips)
#if !sprite
union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};
#endif /* sprite */
#endif

typedef uint16	SystemPortAddress;


/* semaphore definitions */

#define IPCProtection	(0600)		/* access/modify by user only */

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

/*
 * XXX improperly styled declarations
 */
extern IpcSemaphoreId IpcSemaphoreCreate();
extern void IpcSemaphoreKill();
extern void IpcSemaphoreLock();
extern void IpcSemaphoreUnlock();
extern int IpcSemaphoreGetCount();
extern int IpcSemaphoreGetValue();
extern IpcMemoryId IpcMemoryCreate();
extern IpcMemoryId IpcMemoryIdGet();
extern char *IpcMemoryAttach();
extern void IpcMemoryKill();

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
    SINVALLOCKID,
    OIDGENLOCKID,
    SHMEMLOCKID,
    BINDINGLOCKID,
    LOCKMGRLOCKID,

#ifdef SONY_JUKEBOX
    SJCACHELOCKID,
#endif /* SONY_JUKEBOX */

#ifdef MAIN_MEMORY
    MMCACHELOCKID,
#endif /* MAIN_MEMORY */

    PROCSTRUCTLOCKID,
    FIRSTFREELOCKID
} _LockId_;

typedef struct slock {
    slock_t		locklock;
    unsigned char	flag;
    short		nshlocks;
    slock_t		shlock;
    slock_t		exlock;
    slock_t		comlock;
    struct slock	*next;
} SLock;
extern void CreateAndInitSLockMemory();
extern void AttachSLockMemory();
extern int CreateLock();
extern void RelinquishLock();
extern void SharedLock();
extern void SharedUnlock();
extern void ExclusiveLock();
extern void ExclusiveUnlock();
extern bool LockIsFree();
#else /* HAS_TEST_AND_SET */

typedef enum _LockId_ {
    SHMEMLOCKID,
    BINDINGLOCKID,
    BUFMGRLOCKID,
    LOCKMGRLOCKID,

#ifdef SONY_JUKEBOX
    SJCACHELOCKID,
#endif /* SONY_JUKEBOX */

#ifdef MAIN_MEMORY
    MMCACHELOCKID,
#endif /* MAIN_MEMORY */

    PROCSTRUCTLOCKID,
    FIRSTFREELOCKID
} _LockId_;

#define MAX_SPINS	FIRSTFREELOCKID

#endif /* HAS_TEST_AND_SET */
#endif	/* !defined(IPCIncluded) */
