
/*
 *  TASLOCK.H
 *
 *  $Header$
 *
 *  included by pladt.h
 */

#ifndef INCLUDE_TASKLOCK_H
#define INCLUDE_TASKLOCK_H
#include <sys/types.h>
#include <stdio.h>
#include <signal.h>

#define NUMPROCS	64

#include "port_taslock.h"	/* from port* directory */

typedef unsigned char ubyte;

typedef struct {
    slock_t MasterLock;	/* lock is in use for read or write 	*/
    slock_t MinorLock;	/* spin-lock (monitor access)		*/
    int  NHeld;		/* number of (read) locks held		*/
    int  NWait;		/* number of processes waiting for lock */
} TasLock;

typedef struct {
    TasLock	Tas[NUMTASLOCKS];	/* normal Tas locks		  */
    TasLock	TasInit;	/* spin lock for array allocation */
    int		NumProcs;	/* this + Array_* gov't by TasInit*/
    int		Array_Pid[NUMPROCS];
    int		Array_Lck[NUMPROCS];
    char	Monitor[NUMPROCS];
} TASStructure;

#endif

