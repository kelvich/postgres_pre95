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

#ifndef	_IPC_
#define _IPC_
#ifndef sequent
#include <sys/ipc.h>
#else
#include "/usr/att/usr/include/sys/ipc.h"
#endif
#endif

#ifndef sequent
#include <sys/sem.h>
#include <sys/shm.h>
#else
#include "/usr/att/usr/include/sys/sem.h"
#include "/usr/att/usr/include/sys/shm.h"
#endif

#include <errno.h>

#include "storage/ipci.h"		/* for PrivateIPCKey XXX */
#include "storage/ipc.h"

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
    shmctl(shmId, IPC_RMID, (struct shmid_ds *)NULL);
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
    ushort	array[IpcMaxNumSem];
    union semun	semun;
    
    /* get a semaphore if non-existent */
    /* check arguments	*/
    if (semNum > IpcMaxNumSem || semNum <= 0)  {
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
	
	/* if (semKey == PrivateIPCKey) */
	on_exitpg(IPCPrivateSemaphoreKill, (caddr_t)semId);
	
    } else {
	/* there is a semaphore id for this key */
	*status = IpcSemIdExist;
    }
    
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
    ushort	array[IpcMaxNumSem];
    union semun	semun;
    
    /* get a semaphore if non-existent */
    /* check arguments	*/
    if (semNum > IpcMaxNumSem || semNum <= 0)  {
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
	errStatus = semop(semId, (struct sembuf **)&sops, 1);
    } while (errStatus == -1 && errno == EINTR);
    
    IpcSemaphoreLock_return = errStatus;
    
    if (errStatus == -1) {
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
	semop(semId, (struct sembuf **)&sops, 1);
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
	errStatus = semop(semId, (struct sembuf **)&sops, 1);
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
	semop(semId, (struct sembuf **)&sops, 1);
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
    
    shmid = shmget(memKey, size, IPC_CREAT|permission); 
    if (shmid < 0) {
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
    
    shmid = shmget(memKey, size, IPC_CREAT|permission); 
    if (shmid < 0) {
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
    
    /*	memAddress = (char *) malloc(sizeof(int)); XXX ??? */
    memAddress = (char *) shmat(memId, 0, 0);
    
    /*	if ( *memAddress == -1) { XXX ??? */
    if ( memAddress == (char *)-1) {
	perror("IpcMemoryAttach: shmat() failed");
	return(IpcMemAttachFailed);
    }

    /* XXX huh?
       switch (errno) {
       case EINVAL:
       case EACCES:
       case ENOMEM:
       case EMFILE: perror("IpcMemoryAttach: shmat() failed:");
       return(IpcMemAttachFailed);
       }
       */
    
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
    
    if ((shmid = shmget(memKey, 0, 0)) >= 0) {
	shmctl(shmid, IPC_RMID, (struct shmid_ds *) 0);
    }
} 
