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
 *	ExecGetExecutorSharedMemorySize()- get the size of shared segment
 *	ParallelExecutorEnabled()	- return parallel status
 *	SetParallelExecutorEnabled()	- assign parallel status
 *	GetNumberSlaveBackends()	- return parallel information
 *	SetNumberSlaveBackends()	- assign parallel information
 *
 *	I_xxx()			- initialize the specified semaphore
 *	P_xxx()			- P on the specified semaphore
 *	V_xxxV()		- V on the specified semaphore
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
#include "mcxt.h"

 RcsId("$Header$");

/* ----------------
 *	local defines and stuff
 * ----------------
 */
#define EXEC_SHM_SIZE		65535
#define EXEC_SHM_PERMISSION	0700

IpcMemoryId 	ExecutorMemoryId = -1;
IpcSemaphoreId 	ExecutorSemaphoreId = -1;
uint16 		ExecNumSem = 0;

char	*ExecutorSharedMemory = NULL;
int	ExecutorSharedMemorySize = 0;
int	ParallelExecutorEnableState = 0;
int     NumberSlaveBackends = 0;
int     *ExecutorSemaphoreLockCount = NULL;

int     ExecutorSemaphoreArraySize;
int    	ExecutorSMSemaphore;
int    	ExecutorMasterSemaphore;
int    	ExecutorSlaveSemStart;

/* ----------------------------------------------------------------
 *			accessor functions
 *
 *	these are here to provide access to variables kept
 *	hidden to other parts of the system.  In particular
 *
 *		SetParallelExecutorEnabled() and
 *		SetNumberSlaveBackends()
 *
 *	Are used to specify parallel information obtained
 *	from the command line,
 *
 *		ParallelExecutorEnabled()
 *
 *	Is used during initialization to decide whether or
 *	not to allocate shared memory and semaphores and also
 *	at runtime to decide how to plan/execute the query.
 *
 * ----------------------------------------------------------------
 */

char *
ExecGetExecutorSharedMemory()
{
    return ExecutorSharedMemory;
}

int
ExecGetExecutorSharedMemorySize()
{
    return ExecutorSharedMemorySize;
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

int
GetNumberSlaveBackends()
{
    return NumberSlaveBackends;
}

/* ----------------------------------------------------------------
 *		       semaphore operations
 *
 *	The IpcSemaphoreXXX routines allow operations on named
 *	arrays of semaphores.  The executor semaphores are all
 *	kept in a single array and thus managable via a single
 *	IPCKey.  That way they are easy to clean up from the outside
 *	world.
 *
 *	Exec_I, Exec_P and Exec_V are the "internal" semaphore
 *	interface routines to the Ipc code.  These routines provide
 *	access to the entire array of executor semaphores.  In
 *	addition, these keep track of the number of locks made by
 *	each backend so that when a backend dies, the semaphore locks
 *	can be released.
 *
 *	I_Start, etc. are routines which access specific-purposed
 *	semaphores in the array.  These I_, P_ and V_ routines
 *	are the only ones expected to call the Exec_I, etc routines.
 *	Hence if the code changed or new semaphores are needed,
 *	then this interface guideline should be kept in mind.
 * ----------------------------------------------------------------
 */

/* --------------------------------
 *	ExecImmediateReleaseSemaphore releases all the semaphore
 *	locks placed by this backend.  This is called by exitpg().
 * --------------------------------
 */

void
ExecImmediateReleaseSemaphore()
{
    int i;
    int cnt;
    
    for (i=0; i<ExecutorSemaphoreArraySize; i++) {
	cnt = ExecutorSemaphoreLockCount[i];
	if (cnt > 0)
	    IpcSemaphoreUnlock(ExecutorSemaphoreId, i, cnt);
    }
}

/* --------------------------------
 *	Exec_I sets the value of the specified executor semaphore
 * --------------------------------
 */

void
Exec_I(semno, value)
    int semno;
    int value;
{
    IpcSemaphoreSet(ExecutorSemaphoreId, semno, value);
    ExecutorSemaphoreLockCount[semno] = -value;
}

/* --------------------------------
 *	Exec_P requests a P (plaatsen) operation on the specified
 *	executor semaphore.  It places nlocks-locks at once.
 * --------------------------------
 */

void
Exec_P(semno, nlocks)
    int semno;
    int nlocks;
{
    IpcSemaphoreLock(ExecutorSemaphoreId, semno, -nlocks);
    ExecutorSemaphoreLockCount[semno] += nlocks;
}

/* --------------------------------
 *	Exec_V requests a V (vrijlaten) operation on the specified
 *	executor semaphore.  It removes nlocks-locks at once.
 * --------------------------------
 */

void
Exec_V(semno, nlocks)
    int semno;
    int nlocks;
{
    IpcSemaphoreUnlock(ExecutorSemaphoreId, semno, -nlocks);
    ExecutorSemaphoreLockCount[semno] -= nlocks;
}

/* --------------------------------
 *	external interface to semaphore operations
 *
 *	Note: P_Finished() places nslaves-1 locks at once so that
 *	      after nslaves execute V_Finished(), the P will be granted.
 * --------------------------------
 */
/* ----------------
 *	the Start(n) semaphores are used to signal the slave backends
 *	to start processing.
 * ----------------
 */
void
I_Start(x)
    int x;
{
    Exec_I(x + ExecutorSlaveSemStart, 0);
}

void
P_Start(x)
    int x;
{
    Exec_P(x + ExecutorSlaveSemStart, 1);
}

void
V_Start(x)
    int x;
{
    Exec_V(x + ExecutorSlaveSemStart, 1);
}

/* ----------------
 *	the Finished() semaphore is used for the slaves to signal
 *	the master backend that processing is complete.
 * ----------------
 */
void
I_Finished()
{
    Exec_I(ExecutorMasterSemaphore, 0);
}

void
P_Finished()
{
    int nslaves;
    nslaves = GetNumberSlaveBackends();
    Exec_P(ExecutorMasterSemaphore, nslaves-1);
}

void
V_Finished()
{
    Exec_V(ExecutorMasterSemaphore, 1);
}

/* ----------------
 *	the SharedMemoryMutex semaphore is used to restrict concurrent
 *	updates to the executor shared memory allocator meta-data.
 * ----------------
 */
void
I_SharedMemoryMutex()
{
    Exec_I(ExecutorSMSemaphore, 0);
}

void
P_SharedMemoryMutex()
{
    Exec_P(ExecutorSMSemaphore, 1);
}

void
V_SharedMemoryMutex()
{
    Exec_V(ExecutorSMSemaphore, 1);
}

/* --------------------------------
 *	ExecInitExecutorSemaphore
 *
 *	Note: if the semaphore already exists, IpcSemaphoreCreate()
 *	      will return the existing semaphore id.
 * --------------------------------
 */

ExecInitExecutorSemaphore(key)
    IPCKey key;
{
    MemoryContext oldContext;
    int status;

    /* ----------------
     *	make certain we are in the top memory context
     *  or else allocated stuff will go away after the
     *  first transaction.
     * ----------------
     */
    oldContext = MemoryContextSwitchTo(TopMemoryContext);

    /* ----------------
     *	calculate size of semaphore array
     * ----------------
     */
    ExecutorSemaphoreArraySize =
	1 +			    /* 1 for shared memory meta-data access */
	1 +			    /* 1 for master backend synchronization */
	NumberSlaveBackends;        /* n for slave backend synchronization */

    /* ----------------
     *	calculate semaphore numbers (indexes into semaphore array)
     * ----------------
     */
    ExecutorSMSemaphore = 0;
    ExecutorMasterSemaphore = ExecutorSMSemaphore + 1;
    ExecutorSlaveSemStart =   ExecutorMasterSemaphore + 1;
	
    /* ----------------
     *	create the executor semaphore array and initialize all
     *  semaphores to 0.
     * ----------------
     */
    ExecutorSemaphoreId = IpcSemaphoreCreate(key,
					     ExecutorSemaphoreArraySize,
					     IPCProtection,
					     0,		/* initial value */
					     &status);

    /* ----------------
     *	create the semaphore lock count array.  This array keeps
     *  track of the number of locks placed by a backend on the
     *  executor semaphores.  This is needed for releasing locks
     *  on the semaphores when a backend dies.
     * ----------------
     */
    ExecutorSemaphoreLockCount = (int *)
	palloc(ExecutorSemaphoreArraySize * sizeof(int));
					
    /* ----------------
     *	register a function to cleanup when we leave
     * ----------------
     */
    on_exitpg(ExecImmediateReleaseSemaphore, (caddr_t) 0);
    
    /* ----------------
     *	return to old memory context
     * ----------------
     */
    (void) MemoryContextSwitchTo(oldContext);
}

/* ----------------------------------------------------------------
 *			shared memory ops
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
     *	calculate the size of the shared memory segment we need.
     *  XXX make this a function of NumberSlaveBackends.
     * ----------------
     */
    ExecutorSharedMemorySize = EXEC_SHM_SIZE;
    
    /* ----------------
     *	create the shared memory segment
     * ----------------
     */
    ExecutorMemoryId = IpcMemoryCreate(key,
				       ExecutorSharedMemorySize,
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
