/* ----------------------------------------------------------------
 * ipc.c --
 *      POSTGRES inter-process communication definitions.
 *
 * Identification:
 *      $Header$
 * ----------------------------------------------------------------
 */


#include <sys/types.h>
#include <sys/file.h>
#include <stdio.h>
#include <errno.h>

/* XXX - the following  dependency should be moved into the defaults.mk file */
#ifndef	_IPC_
#define _IPC_
#ifndef sequent 

#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>

#else

#include "/usr/att/usr/include/sys/ipc.h"
#include "/usr/att/usr/include/sys/sem.h"
#include "/usr/att/usr/include/sys/shm.h"

#endif /* defined(sequent) */
#endif

#include "storage/ipci.h"		/* for PrivateIPCKey XXX */
#include "storage/ipc.h"
#include "tmp/align.h"
#include "utils/log.h"
#include "tcop/slaves.h"

int UsePrivateMemory = 0;

#ifdef PARALLELDEBUG
#include "executor/paralleldebug.h"
#endif

/* ----------------------------------------------------------------
 *			exit() handling stuff
 * ----------------------------------------------------------------
 */

#define MAX_ON_EXITS 20

static struct ONEXIT {
    void (*function)();
    caddr_t arg;
} onexit_list[ MAX_ON_EXITS ];

static int onexit_index;

typedef struct _PrivateMemStruct {
    int id;
    char *memptr;
} PrivateMem;

PrivateMem IpcPrivateMem[16];

int
PrivateMemoryCreate ( memKey , size , permission )
     IpcMemoryKey memKey;
     uint32 size;
{
    static int memid = 0;
    
    UsePrivateMemory = 1;

    IpcPrivateMem[memid].id = memid;
    IpcPrivateMem[memid].memptr = malloc(size);
    if (IpcPrivateMem[memid].memptr == NULL)
       elog(WARN, "PrivateMemoryCreate: not enough memory to malloc");
    
    return (memid++);
}

char *
PrivateMemoryAttach ( memid , ignored1 , ignored2 )
     IpcMemoryId memid;
     int ignored1;
     int ignored2;
{
    return ( IpcPrivateMem[memid].memptr );
}


/* ----------------------------------------------------------------
 *	exitpg
 *
 *	this function calls all the callbacks registered
 *	for it (to free resources) and then calls exit.
 *	This should be the only function to call exit().
 *	-cim 2/6/90
 * ----------------------------------------------------------------
 */
int exitpg_inprogress = 0;

void
exitpg(code)
    int code;
{
    int i;

    /* ----------------
     *	if exitpg_inprocess is true, then it means that we
     *  are being invoked from within an on_exit() handler
     *  and so we return immediately to avoid recursion.
     * ----------------
     */
    if (exitpg_inprogress)
	return;

    exitpg_inprogress = 1;

    /* ----------------
     *	call all the callbacks registered before calling exit().
     * ----------------
     */
    for (i = onexit_index - 1; i >= 0; --i)
	(*onexit_list[i].function)(code, onexit_list[i].arg);

    exit(code);
}

/* ------------------
 * Run all of the on_exitpg routines but don't exit in the end.
 * This is used by the postmaster to re-initialize shared memory and
 * semaphores after a backend dies horribly
 * ------------------
 */
void
quasi_exitpg()
{
    int i;

    /* ----------------
     *	if exitpg_inprocess is true, then it means that we
     *  are being invoked from within an on_exit() handler
     *  and so we return immediately to avoid recursion.
     * ----------------
     */
    if (exitpg_inprogress)
	return;

    exitpg_inprogress = 1;

    /* ----------------
     *	call all the callbacks registered before calling exit().
     * ----------------
     */
    for (i = onexit_index - 1; i >= 0; --i)
	(*onexit_list[i].function)(0, onexit_list[i].arg);

    onexit_index = 0;
    exitpg_inprogress = 0;
}

/* ----------------------------------------------------------------
 *	on_exitpg
 *
 *	this function adds a callback function to the list of
 *	functions invoked by exitpg().	-cim 2/6/90
 * ----------------------------------------------------------------
 */

on_exitpg(function, arg)
    void (*function)();
    caddr_t arg;
{
    if (onexit_index >= MAX_ON_EXITS)
	return(-1);
    
    onexit_list[ onexit_index ].function = function;
    onexit_list[ onexit_index ].arg = arg;

    ++onexit_index;
    
    return(0);
}

/****************************************************************************/
/*   IPCPrivateSemaphoreKill(status, semId)				    */
/*									    */
/****************************************************************************/
void
IPCPrivateSemaphoreKill(status, semId)
    int	status;
    int	semId;		/* caddr_t */
{
    union semun	semun;
    semctl(semId, 0, IPC_RMID, semun);
}


/****************************************************************************/
/*   IPCPrivateMemoryKill(status, shmId)				    */
/*									    */
/****************************************************************************/
void
IPCPrivateMemoryKill(status, shmId)
    int	status;
    int	shmId;		/* caddr_t */
{
    if ( UsePrivateMemory ) {
	/* free ( IpcPrivateMem[shmId].memptr ); */
    } else {
	shmctl(shmId, IPC_RMID, (struct shmid_ds *)NULL);
    } 
}


/****************************************************************************/
/*   IpcSemaphoreCreate(semKey, semNum, permission, semStartValue)          */
/*    									    */
/*    - returns a semaphore identifier:					    */
/*    									    */
/* if key doesn't exist: return a new id,      status:= IpcSemIdNotExist    */
/* if key exists:        return the old id,    status:= IpcSemIdExist	    */
/* if semNum > MAX :     return # of argument, status:=IpcInvalidArgument   */
/*									    */
/****************************************************************************/

/*
 * Note:
 * XXX	This should be split into two different calls.  One should
 * XXX	be used to create a semaphore set.  The other to "attach" a
 * XXX	existing set.  It should be an error for the semaphore set
 * XXX	to to already exist or for it not to, respectively.
 *
 *	Currently, the semaphore sets are "attached" and an error
 *	is detected only when a later shared memory attach fails.
 */
    
IpcSemaphoreId
IpcSemaphoreCreate(semKey, semNum, permission, semStartValue, status)
    IpcSemaphoreKey semKey;   		/* input patameter  */
    int		    semNum;		/* input patameter  */
    int		    permission;		/* input patameter  */
    int		    semStartValue;	/* input patameter  */
    int		    *status;		/* output parameter */
{
    int		i;
    int		errStatus;
    int		semId;
    ushort	array[IPC_NMAXSEM];
    union semun	semun;
    
    /* get a semaphore if non-existent */
    /* check arguments	*/
    if (semNum > IPC_NMAXSEM || semNum <= 0)  {
	*status = IpcInvalidArgument;
	return(2);	/* returns the number of the invalid argument	*/
    }
    
    semId = semget(semKey, 0, 0);
    if (semId == -1) {
	*status = IpcSemIdNotExist;	/* there doesn't exist a semaphore */
#ifdef DEBUG_IPC
	fprintf(stderr,"calling semget with %d, %d , %d\n",
		semKey,
		semNum,
		IPC_CREAT|permission );
#endif
	semId = semget(semKey, semNum, IPC_CREAT|permission);

	if (semId < 0) {
	    perror("semget");
	    exitpg(3);
	}
	for (i = 0; i < semNum; i++) {
	    array[i] = semStartValue;
	}
	semun.array = array;
	errStatus = semctl(semId, 0, SETALL, semun);
	if (errStatus == -1) {
	    perror("semctl");
	}
	
	/* if (semKey == PrivateIPCKey) */
	on_exitpg(IPCPrivateSemaphoreKill, (caddr_t)semId);
	
    } else {
	/* there is a semaphore id for this key */
	*status = IpcSemIdExist;
    }

#ifdef DEBUG_IPC
    fprintf(stderr,"\nIpcSemaphoreCreate, status %d, returns %d\n",
	    *status,
	    semId );
    fflush(stdout);
    fflush(stderr);
#endif
    return(semId);
}

/* ----------------
 *	IpcSemaphoreCreateWithoutOnExit
 *
 *	this is a copy of IpcSemaphoreCreate, but it doesn't register
 *	an on_exitpg procedure.  This is necessary for the executor
 *	semaphores because we need a special on-exit procedure to
 *	do the right thing depending on if the postgres process is
 *	a master or a slave process.  In a master process, we should
 *	kill the semaphores when we exit but in a slave process we
 *	should release them.
 * ----------------
 */
IpcSemaphoreId
IpcSemaphoreCreateWithoutOnExit(semKey,
				semNum,
				permission,
				semStartValue,
				status)
    IpcSemaphoreKey semKey;   		/* input patameter  */
    int		    semNum;		/* input patameter  */
    int		    permission;		/* input patameter  */
    int		    semStartValue;	/* input patameter  */
    int		    *status;		/* output parameter */
{
    int		i;
    int		errStatus;
    int		semId;
    ushort	array[IPC_NMAXSEM];
    union semun	semun;
    
    /* get a semaphore if non-existent */
    /* check arguments	*/
    if (semNum > IPC_NMAXSEM || semNum <= 0)  {
	*status = IpcInvalidArgument;
	return(2);	/* returns the number of the invalid argument	*/
    }
    
    semId = semget(semKey, 0, 0);
    if (semId == -1) {
	*status = IpcSemIdNotExist;	/* there doesn't exist a semaphore */
	semId = semget(semKey, semNum, IPC_CREAT|permission);
	if (semId < 0) {
	    perror("semget");
	    exitpg(3);
	}
	for (i = 0; i < semNum; i++) {
	    array[i] = semStartValue;
	}
	semun.array = array;
	errStatus = semctl(semId, 0, SETALL, semun);
	if (errStatus == -1) {
	    perror("semctl");
	}
    } else {
	/* there is a semaphore id for this key */
	*status = IpcSemIdExist;
    }
    
    return(semId);
}


/****************************************************************************/
/*   IpcSemaphoreSet()		- sets the initial value of the semaphore   */
/*									    */
/*	note: the xxx_return variables are only used for debugging.	    */
/****************************************************************************/
int IpcSemaphoreSet_return;

void
IpcSemaphoreSet(semId, semno, value)
    int		semId;
    int		semno;
    int		value;
{
    int		errStatus;
    union semun	semun;

    semun.val = value;
    errStatus = semctl(semId, semno, SETVAL, semun);
    IpcSemaphoreSet_return = errStatus;
    
    if (errStatus == -1)
	perror("semctl");
}

/****************************************************************************/
/*   IpcSemaphoreKill(key)	- removes a semaphore			    */
/*									    */
/****************************************************************************/
void
IpcSemaphoreKill(key)
    IpcSemaphoreKey key;
{
    int		i;
    int 	semId;
    int		status;
    union semun	semun;
    
    /* kill semaphore if existent */
    
    semId = semget(key, 0, 0);
    if (semId != -1)
	semctl(semId, 0, IPC_RMID, semun);
}

/****************************************************************************/
/*   IpcSemaphoreLock(semId, sem, lock)	- locks a semaphore		    */
/*									    */
/*	note: the xxx_return variables are only used for debugging.	    */
/****************************************************************************/
int IpcSemaphoreLock_return;

void
IpcSemaphoreLock(semId, sem, lock)
    IpcSemaphoreId	semId;
    int			sem;
    int			lock;
{
    extern int		errno;
    int			errStatus;
    struct sembuf	sops;
    
    sops.sem_op = lock;
    sops.sem_flg = 0;
    sops.sem_num = sem;
    
    /* ----------------
     *	Note: if errStatus is -1 and errno == EINTR then it means we
     *        returned from the operation prematurely because we were
     *	      sent a signal.  So we try and lock the semaphore again.
     *	      I am not certain this is correct, but the semantics aren't
     *	      clear it fixes problems with parallel abort synchronization,
     *	      namely that after processing an abort signal, the semaphore
     *	      call returns with -1 (and errno == EINTR) before it should.
     *	      -cim 3/28/90
     * ----------------
     */
    do {
#ifndef SINGLE_USER
	errStatus = semop(semId, (struct sembuf **)&sops, 1);
#else
    errStatus = 0;
#endif
    } while (errStatus == -1 && errno == EINTR);
    
    IpcSemaphoreLock_return = errStatus;
    
    if (errStatus == -1) {
	if (!ParallelExecutorEnabled())
	    /*
	     * this is a hack. currently the master backend kills the
	     * slave backend by removing the semaphores that they are waiting
	     * on.  does not want it to complain.
	     */
	    perror("semop");
	exitpg(255);
    }
}

/* ----------------
 *	IpcSemaphoreSilentLock
 *
 *	This does what IpcSemaphoreLock() does but does not
 *	produce error messages.  
 *
 *	note: the xxx_return variables are only used for debugging.
 * ----------------
 */
int IpcSemaphoreSilentLock_return;

void
IpcSemaphoreSilentLock(semId, sem, lock)
    IpcSemaphoreId	semId;
    int			sem;
    int			lock;
{
    int			errStatus;
    struct sembuf	sops;
    
    sops.sem_op = lock;
    sops.sem_flg = 0;
    sops.sem_num = sem;
    
    IpcSemaphoreSilentLock_return =
#ifndef SINGLE_USER
	semop(semId, (struct sembuf **)&sops, 1);
#else
	0;
#endif
}

/****************************************************************************/
/*   IpcSemaphoreUnlock(semId, sem, lock)	- unlocks a semaphore	    */
/*									    */
/*	note: the xxx_return variables are only used for debugging.	    */
/****************************************************************************/
int IpcSemaphoreUnlock_return;
    
void
IpcSemaphoreUnlock(semId, sem, lock)
    IpcSemaphoreId	semId;
    int			sem;
    int			lock;
{
    extern int		errno;
    int			errStatus;
    struct sembuf	sops;
    
    sops.sem_op = -lock;
    sops.sem_flg = 0;
    sops.sem_num = sem;

    
    /* ----------------
     *	Note: if errStatus is -1 and errno == EINTR then it means we
     *        returned from the operation prematurely because we were
     *	      sent a signal.  So we try and lock the semaphore again.
     *	      I am not certain this is correct, but the semantics aren't
     *	      clear it fixes problems with parallel abort synchronization,
     *	      namely that after processing an abort signal, the semaphore
     *	      call returns with -1 (and errno == EINTR) before it should.
     *	      -cim 3/28/90
     * ----------------
     */
    do {
#ifndef SINGLE_USER
	errStatus = semop(semId, (struct sembuf **)&sops, 1);
#else
	errStatus = 0;
#endif
    } while (errStatus == -1 && errno == EINTR);
    
    IpcSemaphoreUnlock_return = errStatus;
    
    if (errStatus == -1) {
	perror("semop");
	exitpg(255);
    }
}

/* ----------------
 *	IpcSemaphoreSilentUnlock
 *
 *	This does what IpcSemaphoreUnlock() does but does not
 *	produce error messages.  This is used in the slave backends
 *	when they exit because it is possible that the master backend
 *	may have died and removed the semaphore before this code gets
 *	to execute in the slaves.
 *
 *	note: the xxx_return variables are only used for debugging.
 * ----------------
 */
int IpcSemaphoreSilentUnlock_return;

void
IpcSemaphoreSilentUnlock(semId, sem, lock)
    IpcSemaphoreId	semId;
    int			sem;
    int			lock;
{
    int			errStatus;
    struct sembuf	sops;
    
    sops.sem_op = -lock;
    sops.sem_flg = 0;
    sops.sem_num = sem;
    
    IpcSemaphoreSilentUnlock_return =
#ifndef SINGLE_USER
	semop(semId, (struct sembuf **)&sops, 1);
#else
	0;
#endif
}

int
IpcSemaphoreGetCount(semId, sem)
IpcSemaphoreId	semId;
int		sem;
{
    int semncnt;

    semncnt = semctl(semId, sem, GETNCNT, NULL);
    return semncnt;
}

int
IpcSemaphoreGetValue(semId, sem)
IpcSemaphoreId	semId;
int		sem;
{
    int semval;

    semval = semctl(semId, sem, GETVAL, NULL);
    return semval;
}

/****************************************************************************/
/*   IpcMemoryCreate(memKey)						    */
/*									    */
/*    - returns the memory identifier, if creation succeeds		    */
/*	returns IpcMemCreationFailed, if failure			    */
/****************************************************************************/

IpcMemoryId
IpcMemoryCreate(memKey, size, permission)
    IpcMemoryKey memKey;
    uint32	 size;
    int		 permission;
{
    IpcMemoryId	 shmid;
    int		 errStatus; 

    if ( memKey == PrivateIPCKey && !(ParallelExecutorEnabled()) ) {
	/* private */
	shmid = PrivateMemoryCreate ( memKey, size , IPC_CREAT|permission);
    } else {
	shmid = shmget(memKey, size, IPC_CREAT|permission); 
    }

    if (shmid < 0) {
	fprintf(stderr,"IpcMemoryCreate: memKey=%d , size=%d , permission=%d", 
	     memKey, size , permission );
	perror("IpcMemoryCreate: shmget(..., create, ...) failed");
	return(IpcMemCreationFailed);
    }
    
    /* if (memKey == PrivateIPCKey) */
    on_exitpg(IPCPrivateMemoryKill, (caddr_t)shmid);
    
    return(shmid);
}

/* ----------------
 *	IpcMemoryCreateWithoutOnExit
 *
 *	Like IpcSemaphoreCreateWithoutOnExit(), this function
 *	creates shared memory without registering an on_exit
 *	procedure.
 * ----------------
 */
IpcMemoryId
IpcMemoryCreateWithoutOnExit(memKey, size, permission)
    IpcMemoryKey memKey;
    uint32	 size;
    int		 permission;
{
    IpcMemoryId	 shmid;
    int		 errStatus; 
    
    if ( memKey == PrivateIPCKey && (!(ParallelExecutorEnabled())) ) {
	/* private */
	shmid = PrivateMemoryCreate ( memKey, size , IPC_CREAT|permission);
    } else {
	shmid = shmget(memKey, size, IPC_CREAT|permission); 
    }

    if (shmid < 0) {
	fprintf(stderr,"IpcMemoryCreate: memKey=%d , size=%d , permission=%d", 
	     memKey, size , permission );
	perror("IpcMemoryCreateWithoutOnExit: shmget(, IPC_CREAT,) failed");
	return(IpcMemCreationFailed);
    }

    return(shmid);
}


/****************************************************************************/
/*  IpcMemoryIdGet(memKey, size)    returns the shared memory Id 	    */
/*				    or IpcMemIdGetFailed	            */
/****************************************************************************/
IpcMemoryId
IpcMemoryIdGet(memKey, size)
    IpcMemoryKey	memKey;
    uint32		size;
{
    IpcMemoryId	shmid;
    
    shmid = shmget(memKey, size, 0);

    if (shmid < 0) {
	fprintf(stderr,"IpcMemoryIdGet: memKey=%d , size=%d , permission=%d", 
	     memKey, size , 0 );
	perror("IpcMemoryIdGet:  shmget() failed");
	return(IpcMemIdGetFailed);
    }
    
    return(shmid);
}


/****************************************************************************/
/*  IpcMemoryAttach(memId)    returns the adress of shared memory	    */
/*			      or IpcMemAttachFailed			    */
/*							                    */
/* CALL IT:  addr = (struct <MemoryStructure> *) IpcMemoryAttach(memId);    */
/*									    */
/****************************************************************************/
char *
IpcMemoryAttach(memId)
    IpcMemoryId	memId;
{
    char	*memAddress;
    int		errStatus;
    
    if ( UsePrivateMemory ) {
	memAddress = (char *)PrivateMemoryAttach ( memId , 0 , 0 );
    } else {
	memAddress = (char *) shmat(memId, 0, 0);
    }

    /*	if ( *memAddress == -1) { XXX ??? */
    if ( memAddress == (char *)-1) {
	perror("IpcMemoryAttach: shmat() failed");
	return(IpcMemAttachFailed);
    }
    
    return((char *) memAddress);
}


/****************************************************************************/
/*  IpcMemoryKill(memKey)    		removes a shared memory segment     */
/*									    */
/****************************************************************************/
void
IpcMemoryKill(memKey)
    IpcMemoryKey	memKey;
{	
    IpcMemoryId		shmid;
    
    if (!UsePrivateMemory && (shmid = shmget(memKey, 0, 0)) >= 0) {
	shmctl(shmid, IPC_RMID, (struct shmid_ds *) 0);
    }
} 

#ifdef HAS_TEST_AND_SET
/* ------------------
 *  use hardware locks to replace semaphores for sequent machines
 *  to avoid costs of swapping processes and to provide unlimited
 *  supply of locks.
 * ------------------
 */
static SLock *SLockArray = NULL;
static SLock **FreeSLockPP;
static int *UnusedSLockIP;
static slock_t *SLockMemoryLock;
static IpcMemoryId SLockMemoryId = -1;
static int SLockMemorySize = sizeof(SLock*) + sizeof(int) + 
			     sizeof(slock_t) + NSLOCKS*sizeof(SLock);
void
CreateAndInitSLockMemory(key)
IPCKey key;
{
    LockId id;
    SLock *slckP;

    SLockMemoryId = IpcMemoryCreate(key,
				    SLockMemorySize,
				    0700);
    AttachSLockMemory(key);
    *FreeSLockPP = NULL;
    *UnusedSLockIP = (int)FIRSTFREELOCKID;
    for (id=0; id<(int)FIRSTFREELOCKID; id++) {
	slckP = &(SLockArray[id]);
	S_INIT_LOCK(&(slckP->locklock));
	slckP->flag = NOLOCK;
	slckP->nshlocks = 0;
	S_INIT_LOCK(&(slckP->shlock));
	S_INIT_LOCK(&(slckP->exlock));
	S_INIT_LOCK(&(slckP->comlock));
	slckP->next = NULL;
      }
    return;
}

void
AttachSLockMemory(key)
IPCKey key;
{
    char *slockM;
    if (SLockMemoryId == -1)
	   SLockMemoryId = IpcMemoryIdGet(key,SLockMemorySize);
    if (SLockMemoryId == -1)
	   elog(FATAL, "SLockMemory not in shared memory");
    slockM = IpcMemoryAttach(SLockMemoryId);
    if (slockM == IpcMemAttachFailed)
	elog(FATAL, "AttachSLockMemory: could not attach segment");
    FreeSLockPP = (SLock**)slockM;
    UnusedSLockIP = (int*)(FreeSLockPP + 1);
    SLockMemoryLock = (slock_t*)(UnusedSLockIP + 1);
    S_INIT_LOCK(SLockMemoryLock);
    SLockArray = (SLock*)LONGALIGN((SLockMemoryLock + 1));
    return;
}

SemId
CreateLock()
{
    int lockid;
    SLock *slckP;

    S_LOCK(SLockMemoryLock);
    if (*FreeSLockPP != NULL) {
       lockid = *FreeSLockPP - SLockArray;
       *FreeSLockPP = (*FreeSLockPP)->next;
      }
    else {
       lockid = *UnusedSLockIP;
       (*UnusedSLockIP)++;
     }
    slckP = &(SLockArray[lockid]);
    S_INIT_LOCK(&(slckP->locklock));
    slckP->flag = NOLOCK;
    slckP->nshlocks = 0;
    S_INIT_LOCK(&(slckP->shlock));
    S_INIT_LOCK(&(slckP->exlock));
    S_INIT_LOCK(&(slckP->comlock));
    slckP->next = NULL;
    S_UNLOCK(SLockMemoryLock);
    return lockid;
}

void
RelinquishLock(lockid)
int lockid;
{
    SLock *slckP;
    slckP = &(SLockArray[lockid]);
    S_LOCK(SLockMemoryLock);
    slckP->next = *FreeSLockPP;
    *FreeSLockPP = slckP;
    S_UNLOCK(SLockMemoryLock);
    return;
}

#ifdef LOCKDEBUG
extern int MyPid;
#define PRINT_LOCK(LOCK) printf("(locklock = %d, flag = %d, nshlocks = %d, \
shlock = %d, exlock =%d)\n", LOCK->locklock, \
LOCK->flag, LOCK->nshlocks, LOCK->shlock, \
LOCK->exlock)
#endif

void
SharedLock(lockid)
SemId lockid;
{
    SLock *slckP;
#ifdef PARALLELDEBUG
    BeginParallelDebugInfo(PDI_SHAREDLOCK);
#endif
    slckP = &(SLockArray[lockid]);
#ifdef LOCKDEBUG
    printf("Proc %d SharedLock(%d)\n", MyPid, lockid);
    printf("IN: ");
    PRINT_LOCK(slckP);
#endif
sh_try_again:
    S_LOCK(&(slckP->locklock));
    switch (slckP->flag) {
    case NOLOCK:
       slckP->flag = SHAREDLOCK;
       slckP->nshlocks = 1;
       S_LOCK(&(slckP->exlock));
       S_UNLOCK(&(slckP->locklock));
#ifdef LOCKDEBUG
       printf("OUT: ");
       PRINT_LOCK(slckP);
#endif
#ifdef PARALLELDEBUG
       EndParallelDebugInfo(PDI_SHAREDLOCK);
#endif
       return;
    case SHAREDLOCK:
       (slckP->nshlocks)++;
       S_UNLOCK(&(slckP->locklock));
#ifdef LOCKDEBUG
       printf("OUT: ");
       PRINT_LOCK(slckP);
#endif
#ifdef PARALLELDEBUG
       EndParallelDebugInfo(PDI_SHAREDLOCK);
#endif
       return;
    case EXCLUSIVELOCK:
       (slckP->nshlocks)++;
       S_UNLOCK(&(slckP->locklock));
       S_LOCK(&(slckP->shlock));
       (slckP->nshlocks)--;
       S_UNLOCK(&(slckP->comlock));
       goto sh_try_again;
     }
}

void
SharedUnlock(lockid)
SemId lockid;
{
    SLock *slckP;
    slckP = &(SLockArray[lockid]);
#ifdef LOCKDEBUG
    printf("Proc %d SharedUnlock(%d)\n", MyPid, lockid);
    printf("IN: ");
    PRINT_LOCK(slckP);
#endif
    S_LOCK(&(slckP->locklock));
    (slckP->nshlocks)--;
    if (slckP->nshlocks == 0) {
       slckP->flag = NOLOCK;
       S_UNLOCK(&(slckP->exlock));
     }
    S_UNLOCK(&(slckP->locklock));
#ifdef LOCKDEBUG
       printf("OUT: ");
       PRINT_LOCK(slckP);
#endif
    return;
}

void
ExclusiveLock(lockid)
SemId lockid;
{
    SLock *slckP;
#ifdef PARALLELDEBUG
    BeginParallelDebugInfo(PDI_EXCLUSIVELOCK);
#endif
    slckP = &(SLockArray[lockid]);
#ifdef LOCKDEBUG
    printf("Proc %d ExclusiveLock(%d)\n", MyPid, lockid);
    printf("IN: ");
    PRINT_LOCK(slckP);
#endif
ex_try_again:
    S_LOCK(&(slckP->locklock));
    switch (slckP->flag) {
    case NOLOCK:
	slckP->flag = EXCLUSIVELOCK;
	S_LOCK(&(slckP->exlock));
	S_LOCK(&(slckP->shlock));
	S_UNLOCK(&(slckP->locklock));
#ifdef LOCKDEBUG
       printf("OUT: ");
       PRINT_LOCK(slckP);
#endif
#ifdef PARALLELDEBUG
	EndParallelDebugInfo(PDI_EXCLUSIVELOCK);
#endif
	return;
    case SHAREDLOCK:
    case EXCLUSIVELOCK:
	S_UNLOCK(&(slckP->locklock));
	S_LOCK(&(slckP->exlock));
	S_UNLOCK(&(slckP->exlock));
	goto ex_try_again;
      }
}

void
ExclusiveUnlock(lockid)
SemId lockid;
{
    SLock *slckP;
    int i;

    slckP = &(SLockArray[lockid]);
#ifdef LOCKDEBUG
    printf("Proc %d ExclusiveUnlock(%d)\n", MyPid, lockid);
    printf("IN: ");
    PRINT_LOCK(slckP);
#endif
    S_LOCK(&(slckP->locklock));
    /* -------------
     *  give favor to read processes
     * -------------
     */
    slckP->flag = NOLOCK;
    if (slckP->nshlocks > 0) {
	while (slckP->nshlocks > 0) {
	   S_UNLOCK(&(slckP->shlock));
	   S_LOCK(&(slckP->comlock));
	 }
	S_UNLOCK(&(slckP->shlock));
      }
    else {
      S_UNLOCK(&(slckP->shlock));
     }
    S_UNLOCK(&(slckP->exlock));
    S_UNLOCK(&(slckP->locklock));
#ifdef LOCKDEBUG
       printf("OUT: ");
       PRINT_LOCK(slckP);
#endif
    return;
}

bool
LockIsFree(lockid)
SemId lockid;
{
    return(SLockArray[lockid].flag == NOLOCK);
}

#endif /* HAS_TEST_AND_SET */
