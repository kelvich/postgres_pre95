/*
 * ipc.c --
 *      POSTGRES inter-process communication definitions.
 *
 * Identification:
 *      $Header$
 */

#include <sys/types.h>
#include <sys/file.h>
#ifndef	_IPC_
#define _IPC_
#include <sys/ipc.h>
#endif
#include <sys/sem.h>
#include <sys/shm.h>
#include <errno.h>
#include "ipci.h"		/* for PrivateIPCKey XXX */

#include "ipc.h"

/*
 * IPCPrivateSemaphoreKill --
 *	On_exit(3) termination handler.
 */
extern
void
IPCPrivateSemaphoreKill ARGS((
	int	status,
	int	semId
));

/*
 * IPCPrivateMemoryKill --
 *	On_exit(3) termination handler.
 */
extern
void
IPCPrivateMemoryKill ARGS((
	int	status,
	int	shmId
));

/****************************************************************************/
/*   IpcSemaphoreCreate(semKey, semNum, permission, semStartValue)          */
/*		- returns a semaphore identifier:									*/
/*		  if key doesn't exist: return a new id, status:= IpcSemIdNotExist	*/
/*		  if key exists:        return the old id, status:= IpcSemIdExist	*/
/*        if semNum > MAX : return # of argument, status:=IpcInvalidArgument*/
/*									*/
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
IpcSemaphoreKey		semKey;   				/* input patameter  */
int					semNum;					/* input patameter  */
int					permission;				/* input patameter  */
int					semStartValue;			/* input patameter  */
int					*status;				/* output parameter */
{
	int	i;
	int	errStatus;
	int	semId;
	ushort		array[IpcMaxNumSem];
	union semun	semun;

	/* get a semaphore if non-existent */
	/* check arguments	*/
	if (semNum > IpcMaxNumSem || semNum <= 0)  {
		*status = IpcInvalidArgument;
		return(2);		/* returns the number of the invalid argument	*/
	}	
	semId = semget(semKey, 0, 0);
	if (semId == -1) {
		*status = IpcSemIdNotExist;	/* there doesn't exist a semaphore 	*/
		semId = semget(semKey, semNum, IPC_CREAT|permission);
		if (semId < 0) {
			perror("semget");
			exit(3);
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
		on_exit(IPCPrivateSemaphoreKill, (caddr_t)semId);

	} else {	/* there is a semaphore id for this key */
		*status = IpcSemIdExist;
	}
	return(semId);
}


/****************************************************************************/
/*   IpcSemaphoreKill(key)	- removes a semaphore						    */
/*																			*/
/****************************************************************************/
void
IpcSemaphoreKill(key)
IpcSemaphoreKey key;
{
	int	i, semId;
	int	status;

	/* kill semaphore if existent */

	semId = semget(key, 0, 0);
	if (semId != -1) {
		union semun	semun;

		semctl(semId, 0, IPC_RMID, semun);
	}
}

/****************************************************************************/
/*   IpcSemaphoreLock(semId, sem, lock)	- locks a semaphore				    */
/*																			*/
/****************************************************************************/
void
IpcSemaphoreLock(semId, sem, lock)
IpcSemaphoreId		semId;
int					sem;
int					lock;
{
	int		errStatus;
	struct sembuf	sops;

	sops.sem_op = lock;
	sops.sem_flg = 0;
	sops.sem_num = sem;

	errStatus = semop(semId, (struct sembuf **)&sops, 1);
	if (errStatus == -1) {
		perror("semop");
		exit(255);
	}
}


/****************************************************************************/
/*   IpcSemaphoreUnLock(semId, sem, lock)	- unlocks a semaphore		    */
/*																			*/
/****************************************************************************/
void
IpcSemaphoreUnlock(semId, sem, lock)
IpcSemaphoreId		semId;
int					sem;
int					lock;
{
	int		errStatus;
	struct sembuf	sops;

	sops.sem_op = -lock;
	sops.sem_flg = 0;
	sops.sem_num = sem;

	errStatus = semop(semId, (struct sembuf **)&sops, 1);
	if (errStatus == -1) {
		perror("semop");
		exit(255);
	}
}




/****************************************************************************/
/*   IpcMemoryCreate(memKey)												*/
/*                     - returns the memory identifier, if creation succeeds*/
/*						 returns IpcMemCreationFailed, if failure			*/
/****************************************************************************/
IpcMemoryId
IpcMemoryCreate(memKey, size, permission)
IpcMemoryKey		memKey;
uint32				size;
int					permission;
{
	IpcMemoryId	shmid;
	int			errStatus; 
	
	shmid = shmget(memKey, size, IPC_CREAT|permission); 
	if (shmid < 0) {
		perror("IpcMemoryCreate: shmget(..., create, ...) failed");
		return(IpcMemCreationFailed);
	}

	/* if (memKey == PrivateIPCKey) */
	on_exit(IPCPrivateMemoryKill, (caddr_t)shmid);

	return(shmid);
}


/****************************************************************************/
/*  IpcMemoryIdGet(memKey, size)    returns the shared memory Id 			*/
/*									or IpcMemIdGetFailed					*/
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
/*  IpcMemoryAttach(memId)    returns the adress of shared memory			*/
/*							  or IpcMemAttachFailed		 					*/
/*																			*/
/* CALL IT:  addr = (struct <MemoryStructure> *) IpcMemoryAttach(memId);	*/
/*																			*/
/****************************************************************************/
char *
IpcMemoryAttach(memId)
IpcMemoryId		memId;
{
	char	*memAddress;
	int		errStatus;
	
/*	memAddress = (char *) malloc(sizeof(int));	XXX ??? */
	memAddress = (char *) shmat(memId, 0, 0);
/*	if ( *memAddress == -1) {			XXX ??? */
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
/*  IpcMemoryKill(memKey)    		removes a shared memory segment			*/
/*																			*/
/****************************************************************************/
void
IpcMemoryKill(memKey)
IpcMemoryKey		memKey;
{	
	IpcMemoryId	shmid;

	if ((shmid = shmget(memKey, 0, 0)) >= 0) {
		shmctl(shmid, IPC_RMID, (struct shmid_ds *) 0);
	}
} 

static
void
IPCPrivateSemaphoreKill(status, semId)
	int	status;
	int	semId;		/* caddr_t */
{
	union semun	semun;
	semctl(semId, 0, IPC_RMID, semun);
}

static
void
IPCPrivateMemoryKill(status, shmId)
	int	status;
	int	shmId;		/* caddr_t */
{
	shmctl(shmId, IPC_RMID, (struct shmid_ds *)NULL);
}
