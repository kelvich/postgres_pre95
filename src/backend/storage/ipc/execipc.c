
/* ----------------------------------------------------------------
 *   FILE
 *	execipc.c
 *	
 *   DESCRIPTION
 *	executor shared memory / semaphore stuff
 *
 *	This code was swiped from the buffer manager and
 *	somewhat simplified.  The whole shared memory / semaphore mess
 *	should probably be thought out better because as it is,
 *	each module that needs shared memory or semaphores has to have
 *	its own functions to create, attach and initialize them.
 *
 *	In my opinion, several modules should register initialization
 *	routines to be called by a single create/attach mechanism at
 *	the appropriate times. -cim 2/27/90
 *
 *	The create/initialize functions are currently called from
 *	{Attach,Create}SharedMemoryAndSemaphores in storage/ipc/ipci.c
 *	which is called from InitCommunication().
 *
 *   INTERFACE ROUTINES
 *	the following are used by the executor proper:
 *	
 *	ExecGetExecutorSharedMemory()	- get the start of shared mem
 *	ExecGetAndHoldSemaphore()	- grab the semaphore
 *	ExecReleaseSemaphore()		- release the semaphore
 *
 *	the rest of the functions are used by the ipci.c code during
 *	initialization.
 *
 *   NOTES
 *	This file exists here and not in the executor directory
 *	because these functions are called by the initialization
 *	code which is compiled into cinterface.a.  The executor
 *	directory contains code which is not compiled into cinterface.a
 *	and so this file is here because it should not be there.
 *	It's that simple.
 *
 *	This is once case where its important to know what code
 *	ends up being compiled where.  If this code is moved, this
 *	problem should be kept in mind...
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

/* ----------------
 *	this group of #includes stolen from ipci.c -- these should
 *	be condensed.
 * ----------------
 */
#include "c.h"

#include "ipc.h"
#include "log.h"
#include "pladt.h"
#include "sinval.h"

#include "ipci.h"

 RcsId("$Header$");

/* ----------------
 *	local defines and stuff
 * ----------------
 */
#define EXEC_SEM_INIT        	255
#define EXEC_SEM_LOCK        	(-255)

#define EXEC_SHM_SIZE		65535
#define EXEC_SHM_PERMISSION	0700

IpcMemoryId 	ExecutorMemoryId = -1;
IpcSemaphoreId 	ExecutorSemaphoreId = -1;
uint16 		ExecNumSem = 0;

char	*ExecutorSharedMemory = NULL;
int	ParallelExecutorEnableState = 0;
int     NumberSlaveBackends = 0;

/* ----------------------------------------------------------------
 *			accessor functions
 *
 *	these are here to provide access to variables kept
 *	hidden to other parts of the system.
 * ----------------------------------------------------------------
 */

char *
ExecGetExecutorSharedMemory()
{
    return ExecutorSharedMemory;
}

int
ParallelExecutorEnabled()
{    
    return ParallelExecutorEnableState;
}

void
SetParallelExecutorEnabled(state)
    bool state;
{    
    ParallelExecutorEnableState = (state == true);
}

void
SetNumberSlaveBackends(numslaves)
    int numslaves;
{
    NumberSlaveBackends = numslaves;
}

/* ----------------------------------------------------------------
 *		       semaphore operations
 * ----------------------------------------------------------------
 */
/* --------------------------------
 *	ExecImmediateReleaseSemaphore
 * --------------------------------
 */

void
ExecImmediateReleaseSemaphore()
{
    if (ExecNumSem != 0) {
	IpcSemaphoreUnlock(ExecutorSemaphoreId, 0, EXEC_SEM_LOCK);
	ExecNumSem = 0;
    }
}

/* --------------------------------
 *	ExecGetAndHoldSemaphore
 * --------------------------------
 */

void
ExecGetAndHoldSemaphore()
{
    if (ExecNumSem == 0) {
	IpcSemaphoreLock(ExecutorSemaphoreId, 0, EXEC_SEM_LOCK);
    }
    
    ExecNumSem += 1;
}

/* --------------------------------
 *	ExecReleaseSemaphore
 * --------------------------------
 */

void
ExecReleaseSemaphore()
{
    if (ExecNumSem >= 1) {
	IpcSemaphoreUnlock(ExecutorSemaphoreId, 0, EXEC_SEM_LOCK);
	ExecNumSem = 0;
    }
}

/* --------------------------------
 *	ExecInitExecutorSemaphore
 *
 *	Note: if the semaphore already exists, IpcSemaphoreCreate()
 *	      will return the existing semaphore id.
 * --------------------------------
 */

void
ExecInitExecutorSemaphore(key)
    IPCKey key;
{
    int status;

    /* ----------------
     *	create the executor semaphore
     * ----------------
     */
    ExecutorSemaphoreId = IpcSemaphoreCreate(key,
					     1,
					     IPCProtection,
					     EXEC_SEM_INIT,
					     &status);

    /* ----------------
     *	register a function to cleanup when we leave
     * ----------------
     */
    on_exitpg(ExecImmediateReleaseSemaphore, (caddr_t) 0);
}

/* ----------------------------------------------------------------
 *			shared memory ops
 *
 *	Note: the shared memory stuff relies on the semaphore
 *	stuff so the semaphore stuff should be initialized
 *	first.
 * ----------------------------------------------------------------
 */

/* --------------------------------
 *	ExecCreateExecutorSharedMemory
 * --------------------------------
 */

void
ExecCreateExecutorSharedMemory(key)
    IPCKey key;
{
    /* ----------------
     *	create the shared memory segment
     * ----------------
     */
    ExecutorMemoryId = IpcMemoryCreate(key,
				       EXEC_SHM_SIZE,
				       EXEC_SHM_PERMISSION);
	
    if (ExecutorMemoryId < 0)
	elog(FATAL, "ExecCreateExecutorSharedMemory: IpcMemoryCreate failed");

}

/* --------------------------------
 *	ExecLocateExecutorSharedMemory
 * --------------------------------
 */

void
ExecLocateExecutorSharedMemory(key)
    IPCKey key;
{
    /* ----------------
     *	find the shared memory segment
     * ----------------
     */
    ExecutorMemoryId = IpcMemoryIdGet(key, 0);
	
    if (ExecutorMemoryId < 0)
	elog(FATAL, "ExecLocateExecutorSharedMemory: IpcMemoryIdGet failed");
}

/* --------------------------------
 *	ExecAttachExecutorSharedMemory
 * --------------------------------
 */

void
ExecAttachExecutorSharedMemory()
{
    /* ----------------
     *	attach to the shared segment
     * ----------------
     */
    ExecutorSharedMemory = IpcMemoryAttach(ExecutorMemoryId);

    if (ExecutorSharedMemory == IpcMemAttachFailed)
	elog(FATAL, "ExecAttachExecutorSharedMemory: IpcMemoryAttach failed");
    
}
