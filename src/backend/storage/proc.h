/*
 * proc.h 
 * 
 * $Header$
 */

#ifndef _PROC_H
#define _PROC_H

#include "storage/shmem.h"
#include "storage/ipci.h"
#include <sys/sem.h>

typedef struct procQueue {
  SHM_QUEUE	links;
  int		size;
} PROC_QUEUE;

/*
 * flags explaining why process woke up
 */
#define NO_ERROR 	0
#define ERR_TIMEOUT	1
#define ERR_BUFFER_IO	2

#define MAX_PRIO	50
#define MIN_PRIO	(-1)

extern int ProcCurrTxId;

PROC_QUEUE *ProcQueueAlloc();

/* for txtime.c and txlog.c */
#define MAX_PROCS	100


typedef struct {
  int	 		sleeplock;
  int			semNum;
  IpcSemaphoreId	semId;
  IpcSemaphoreKey	semKey;
} SEMA;
#endif _PROC_H
