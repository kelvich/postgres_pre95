/*
 * ipc.h --
 *	POSTGRES inter-process communication definitions.
 *
 * Identification:
 *	$Header$
 */

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
union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};
#endif

typedef unsigned short	SystemPortAddress;

/* semaphore definitions */
#define IPCProtection	(0600)		/* access/modify by user only */

#define IPC_NMAXSEM			25	/* max number of semaphores */
#define IpcSemaphoreDefaultStartValue	255
#define IpcSharedLock			(-1)
#define IpcExclusiveLock		(-255)

#define IpcUnknownStatus	(-1)
#define IpcInvalidArgument	(-2)
#define IpcSemIdExist		(-3)
#define IpcSemIdNotExist	(-4)

typedef unsigned long	IpcSemaphoreKey;	/* semaphore key */
typedef int		IpcSemaphoreId;
typedef unsigned long	IpcMemoryKey;		/* shared memory key */
typedef int		IpcMemoryId;

#ifdef sequent
/* ------------------
 *  use hardware locks to replace semaphores for sequent machines
 *  to avoid costs of swapping processes and to provide unlimited
 *  supply of locks.
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
