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
 *	ParallelExecutorEnabled()	- has become a macro defined in slaves.h
 *	SetParallelExecutorEnabled()	- assign parallel status
 *	GetNumberSlaveBackends()	- has become a macro defined in slaves.h
 *	SetNumberSlaveBackends()	- assign parallel information
 *
 *	I_xxx()			- initialize the specified semaphore
 *	P_xxx()			- P on the specified semaphore
 *	V_xxx()			- V on the specified semaphore
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
#include "tmp/c.h"

#include "storage/ipc.h"
#include "storage/ipci.h"
#include "storage/sinval.h"

#include "tcop/slaves.h"
#include "utils/mcxt.h"
#include "utils/log.h"

 RcsId("$Header$");

/* ----------------
 *	local defines and stuff
 * ----------------
 */
#define EXEC_SHM_SIZE		1024*1024
#define EXEC_SHM_PERMISSION	0700

IpcMemoryId 	ExecutorMemoryId = -1;
IpcSemaphoreId 	ExecutorSemaphoreId = -1;
IpcSemaphoreId 	ExecutorSemaphoreId1 = -1;
uint16 		ExecNumSem = 0;

char	*ExecutorSharedMemory = NULL;
int	ExecutorSharedMemorySize = 0;
int	ParallelExecutorEnableState = 0;
int     NumberSlaveBackends = 0;
int     *ExecutorSemaphoreLockCount = NULL;

int     ExecutorSemaphoreArraySize;
int    	ExecutorSMSemaphore;
int    	ExecutorAbortSemaphore;
int    	ExecutorMasterSemaphore;
int    	ExecutorSlaveSemStart;

ProcGroupInfo ProcGroupInfoP;  /* have to define it here for postmaster to
				  be happy to link, dumb!  */
SlaveInfo SlaveInfoP;
int MyPid = -1;
SlaveLocalInfoData SlaveLocalInfoD = {1, 0, false, -1, 0, false, NULL, NULL};

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
 *
 *	Note: in the following code, we always pass negative
 *	      arguments to IpcSemaphore{Lock,Unlock}.  Positive
 *	      values do the opposite of what the name of the
 *	      routine would suggest.
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
    int semno;
    IpcSemaphoreId semid;

    for (i=0; i<ExecutorSemaphoreArraySize; i++) {
	if (i < IPC_NMAXSEM) {
	    semno = i;
	    semid = ExecutorSemaphoreId;
	  }
	else {
	    semno = i - IPC_NMAXSEM;
	    semid = ExecutorSemaphoreId1;
	  }
	cnt = ExecutorSemaphoreLockCount[i];
	if (cnt > 0)
	    IpcSemaphoreSilentUnlock(semid, semno, -cnt);
    }
}

/* --------------------------------
 *	Exec_I sets the value of the specified executor semaphore
 *	We set the lock count to 1-value because a semaphore value
 *	of 1 indicates no processes hold locks on that semaphore.
 * --------------------------------
 */

void
Exec_I(semno, value)
    int semno;
    int value;
{
    int i;
    IpcSemaphoreId semid;

    if (semno < IPC_NMAXSEM) {
	i = semno;
	semid = ExecutorSemaphoreId;
      }
    else {
       i = semno - IPC_NMAXSEM;
       semid = ExecutorSemaphoreId1;
      }
    IpcSemaphoreSet(semid, i, value);
    ExecutorSemaphoreLockCount[semno] = 1-value;
}

/* --------------------------------
 *	Exec_P requests a P (plaatsen) operation on the specified
 *	executor semaphore.  It places "nlocks" locks at once.
 * --------------------------------
 */

void
Exec_P(semno, nlocks)
    int semno;
    int nlocks;
{
    int i;
    IpcSemaphoreId semid;

    if (semno < IPC_NMAXSEM) {
	i = semno;
	semid = ExecutorSemaphoreId;
      }
    else {
       i = semno - IPC_NMAXSEM;
       semid = ExecutorSemaphoreId1;
      }
    IpcSemaphoreLock(semid, i, -nlocks);
    ExecutorSemaphoreLockCount[semno] += nlocks;
}

/* --------------------------------
 *	Exec_V requests a V (vrijlaten) operation on the specified
 *	executor semaphore.  It removes "nlocks" locks at once.
 * --------------------------------
 */

void
Exec_V(semno, nlocks)
    int semno;
    int nlocks;
{
    int i;
    IpcSemaphoreId semid;

    if (semno < IPC_NMAXSEM) {
	i = semno;
	semid = ExecutorSemaphoreId;
      }
    else {
       i = semno - IPC_NMAXSEM;
       semid = ExecutorSemaphoreId1;
      }
    IpcSemaphoreUnlock(semid, i, -nlocks);
    ExecutorSemaphoreLockCount[semno] -= nlocks;
}

/* --------------------------------
 *	external interface to semaphore operations
 * --------------------------------
 */
/* ----------------
 *	the Start(n) semaphores are used to signal the slave backends
 *	to start processing.
 *
 *	Since I_Start sets the semaphore to 0, a process doing P_Start
 *	will wait until after some other process executes a V_Start.
 *
 *	Note: we use local variables because we can tweak them in
 *	      dbx easier...
 * ----------------
 */
void
I_Start(x)
    int x;
{
    int value = 0;
    Exec_I(x + ExecutorSlaveSemStart, value);
}

void
P_Start(x)
    int x;
{
    int value = 1;
    Exec_P(x + ExecutorSlaveSemStart, value);
}

void
V_Start(x)
    int x;
{
    int value = 1;
    Exec_V(x + ExecutorSlaveSemStart, value);
}

/* ----------------
 *	the Finished() semaphore is used for the slaves to signal
 *	the master backend that processing is complete.
 *
 *	Note: P_Finished() places nslaves locks at once so that
 *	      after nslaves execute V_Finished(), the P will be granted.
 *
 *	Since I_Finished sets the semaphore to 0, a process doing P_Finished
 *	will wait until after some other process executes a V_Finished.
 * ----------------
 */
void
I_Finished()
{
    int value = 0;
    Exec_I(ExecutorMasterSemaphore, value);
}

void
P_Finished()
{
    int value = 1;
    Exec_P(ExecutorMasterSemaphore, value);
}

void
V_Finished(groupid, scounter, status)
int groupid;
SharedCounter *scounter;
ProcGroupStatus status;
{
    bool is_lastone;

    if (scounter->count <= 0) {
	elog(WARN, "V_Finished counter negative.\n");
      }
#ifdef sequent
    S_LOCK(&(scounter->exlock));
#else
    Assert(0); /* you are not supposed to call this routine if you are not
		  running on sequent */
#endif
    scounter->count--;
    is_lastone = (bool)(scounter->count == 0);
#ifdef sequent
    S_UNLOCK(&(scounter->exlock));
#endif
    if (is_lastone) {
	/* ----------------
	 *  the last slave wakes up the master
	 * ----------------
	 */
        int value = 1;
	ProcGroupInfoP[groupid].status = status;;
        Exec_V(ExecutorMasterSemaphore, value);
      }
}

void
P_FinishedAbort()
{
    int value = GetNumberSlaveBackends();
    Exec_P(ExecutorMasterSemaphore, value);
}

void
V_FinishedAbort()
{
    int value = 1;
    Exec_V(ExecutorMasterSemaphore, value);
}

/* --------------------------------
 *	InitMWaitOneLock
 *
 *	initialize a one producer multiple consumer lock
 * ---------------------------------
 */
void
InitMWaitOneLock(m1lock)
M1Lock *m1lock;
{
    m1lock->count = 0;
#ifdef HAS_TEST_AND_SET
    S_INIT_LOCK(&(m1lock->waitlock));
    S_LOCK(&(m1lock->waitlock));
#endif
}
    

/* --------------------------------
 *	MWaitOne
 *
 *	consumer waits on the producer
 * ---------------------------------
 */
void
MWaitOne(m1lock)
M1Lock *m1lock;
{
#ifdef HAS_TEST_AND_SET
    S_LOCK(&(m1lock->waitlock));
    m1lock->count--;
    if (m1lock->count > 0)
	S_UNLOCK(&(m1lock->waitlock));
#endif
}

/* --------------------------------
 *	OneSignalM
 *
 *	producer wakes up all consumers
 * ---------------------------------
 */
void
OneSignalM(m1lock, m)
M1Lock *m1lock;
int m;
{
    m1lock->count = m;
#ifdef HAS_TEST_AND_SET
    S_UNLOCK(&(m1lock->waitlock));
#endif
}


/* ----------------
 *	the SharedMemoryMutex semaphore is used to restrict concurrent
 *	updates to the executor shared memory allocator meta-data.
 *
 *	Since I_SharedMemoryMutex sets the semaphore to 1, a process
 *	doing P_SharedMemoryMutex will be immediately granted the semaphore.
 * ----------------
 */
void
I_SharedMemoryMutex()
{
#ifndef sequent
    int value = 1;
    Exec_I(ExecutorSMSemaphore, value);
#endif
}

void
P_SharedMemoryMutex()
{
#ifdef sequent
    ExclusiveLock(ExecutorSMSemaphore);
#else
    int value = 1;
    Exec_P(ExecutorSMSemaphore, value);
#endif
}

void
V_SharedMemoryMutex()
{
#ifdef sequent
    ExclusiveUnlock(ExecutorSMSemaphore);
#else
    int value = 1;
    Exec_V(ExecutorSMSemaphore, value);
#endif
}

/* ----------------
 *	the Abort semaphore is used to synchronize abort transaction
 *	processing in the master and slave backends.
 *
 *	Since I_Abort sets the semaphore to 0, a process doing P_Abort
 *	will wait until after some other process executes a V_Abort.
 * ----------------
 */
void
I_Abort()
{
    int value = 0;
    Exec_I(ExecutorAbortSemaphore, value);
}

void
P_Abort()
{
    int value = 1;
    Exec_P(ExecutorAbortSemaphore, value);
}

void
V_Abort()
{
    int value = 1;
    Exec_V(ExecutorAbortSemaphore, value);
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
#ifndef sequent
	1 +			    /* 1 for shared memory meta-data access */
#endif
	1 +			    /* 1 for abort synchronization */
	1 +			    /* 1 for master backend synchronization */
	NumberSlaveBackends;        /* n for slave backend synchronization */
    
    /* ----------------
     *	calculate semaphore numbers (indexes into semaphore array)
     * ----------------
     */
    ExecutorAbortSemaphore = 0;
    ExecutorMasterSemaphore = ExecutorAbortSemaphore + 1;
    ExecutorSlaveSemStart =   ExecutorMasterSemaphore + 1;
#ifdef sequent
    ExecutorSMSemaphore = CreateLock();
#else
    ExecutorSMSemaphore = ExecutorSlaveSemStart + 1;
#endif
    
    /* ----------------
     *	create the executor semaphore array.  Note, we don't
     *  want register an on_exit procedure here to kill the semaphores
     *  because this code is executed before we fork().  We have to
     *  register our on_exit progedure after we fork() because only
     *  the master backend should register the on_exit procedure.
     * ----------------
     */
    if (ExecutorSemaphoreArraySize <= IPC_NMAXSEM) {
    ExecutorSemaphoreId =
	IpcSemaphoreCreateWithoutOnExit(key,
					ExecutorSemaphoreArraySize,
					IPCProtection,
					0,
					&status);
	}
    else {
    ExecutorSemaphoreId =
	IpcSemaphoreCreateWithoutOnExit(key,
					IPC_NMAXSEM,
					IPCProtection,
					0,
					&status);
    ExecutorSemaphoreId1 =
	IpcSemaphoreCreateWithoutOnExit(key,
					ExecutorSemaphoreArraySize - IPC_NMAXSEM,
					IPCProtection,
					0,
					&status);
	}
       
    
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
     *	register a function to cleanup when we leave.  This function
     *  only releases semaphore locks so it should be called by all
     *  backends when they exit.  In fact, we have to register this
     *  on_exit procedure here, because later we register the semaphore
     *  kill procedure.  If we registered the kill procedure BEFORE
     *  the release procedure, then the unlock code will try to unlock
     *  non-existant semaphores.  But this isn't a big problem because
     *  we're about to die anyway... -cim 3/15/90
     * ----------------
     */
    on_exitpg(ExecImmediateReleaseSemaphore, (caddr_t) 0);
    
    /* ----------------
     *	return to old memory context
     * ----------------
     */
    (void) MemoryContextSwitchTo(oldContext);
}

/* ----------------
 *	ExecSemaphoreOnExit
 *
 *	this function registers the proper on_exit procedure
 *	for the executor semaphores.  This is called in the master
 *	backend AFTER the slaves are fork()'ed so the cleanup
 *	procedure will only fire in the master backend.
 * ----------------
 */
void
ExecSemaphoreOnExit(procedure)
    void (*procedure)();
{
    on_exitpg(procedure, ExecutorSemaphoreId);
    if (ExecutorSemaphoreArraySize > IPC_NMAXSEM)
        on_exitpg(procedure, ExecutorSemaphoreId1);
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
     *	create the shared memory segment.  As with the creation
     *  of the semaphores, we don't want to register a cleanup procedure
     *  until after we have fork()'ed, and then we should only register
     *  it in the Master backend.
     * ----------------
     */
    ExecutorMemoryId =
	IpcMemoryCreateWithoutOnExit(key,
				     ExecutorSharedMemorySize,
				     EXEC_SHM_PERMISSION);
	
    if (ExecutorMemoryId < 0)
	elog(FATAL, "ExecCreateExecutorSharedMemory: IpcMemoryCreate failed");
}

/* --------------------------------
 *	ExecSharedMemoryOnExit
 *
 *	this function registers the proper on_exit procedure
 *	for the executor shared memory.
 * --------------------------------
 */
void
ExecSharedMemoryOnExit(procedure)
    void (*procedure)();
{
    on_exitpg(procedure, ExecutorMemoryId);
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
