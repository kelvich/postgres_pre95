/*
 * proc.h 
 * 
 * $Header$
 */

#ifndef _PROC_H_
#define _PROC_H_

#include "storage/ipci.h"
#include "storage/lock.h"
#include <sys/sem.h>


typedef struct {
  int	 		sleeplock;
  int			semNum;
  IpcSemaphoreId	semId;
  IpcSemaphoreKey	semKey;
} SEMA;

/*
 * Each backend has:
 */
typedef struct proc {

  /* proc->links MUST BE THE FIRST ELEMENT OF STRUCT (see ProcWakeup()) */

  SHM_QUEUE         links;	/* proc can be waiting for one event(lock) */
  SEMA              sem;	/* ONE semaphore to sleep on */
  int               errType; 	/* error code tells why we woke up */

  int               procId;  	/* unique number for this structure
			 	 * NOT unique per backend, these things
				 * are reused after the backend dies.
				 */

  int               critSects;	/* If critSects > 0, we are in sensitive
				 * routines that cannot be recovered when
				 * the process fails.
				 */

  int               prio;	/* priority for sleep queue */

  TransactionIdData xid;	/* transaction currently being executed
				 * by this proc
				 */

  LOCK *            waitLock;	/* Lock we're sleeping on */
  int               token;	/* info for proc wakeup routines */	
  SHM_QUEUE	lockQueue;	/* locks associated with current transaction */
} PROC;

/*
 * Global variables for this module.
 *	This is only used for garbage collection.
 */
typedef struct procglobal {
  SHMEM_OFFSET	freeProcs;
  int		numProcs;
  IPCKey	currKey;
} PROC_HDR;

/*
 * flags explaining why process woke up
 */
#define NO_ERROR 	0
#define ERR_TIMEOUT	1
#define ERR_BUFFER_IO	2

#define MAX_PRIO	50
#define MIN_PRIO	(-1)

PROC_QUEUE *ProcQueueAlloc();

#endif _PROC_H_
