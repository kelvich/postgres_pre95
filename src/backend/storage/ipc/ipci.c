/*
 * ipci.c --
 *	POSTGRES inter-process communication initialization code.
 */

#include "c.h"

#include "ipc.h"
#include "log.h"
#include "pladt.h"
#include "sinval.h"

#include "ipci.h"

RcsId("$Header$");

LockTableId	PageLockTableId;
LockTableId	MultiLevelLockTableId;

IPCKey
SystemPortAddressCreateIPCKey(address)
	SystemPortAddress	address;
{
	Assert(address < 32768);	/* XXX */

	return (SystemPortAddressGetIPCKey(address));
}

/**************************************************

  CreateSharedMemoryAndSemaphores
  is called exactly *ONCE* by the postmaster.
  It is *NEVER* called by the postgres backend

  0) destroy any existing semaphores for both buffer
     and lock managers.
  1) create the appropriate *SHARED* memory segments
     for the two resource managers.
  
 **************************************************/

void
CreateSharedMemoryAndSemaphores(key)
    IPCKey	key;
{
    int		status;

    /* ----------------
     *	kill the lock table stuff
     * ----------------
     */
    LtSynchKill(IPCKeyGetLockTableMemoryKey(key));
    LtTransactionSemaphoreKill(IPCKeyGetLockTableSemaphoreKey(key));

    /* ----------------
     *	kill and create the buffer manager buffer pool (and semaphore)
     * ----------------
     */
    CreateBufferSemaphore(IPCKeyGetBufferSemaphoreKey(key));
    CreateBufferPoolMemory(IPCKeyGetBufferMemoryKey(key));

    /* ----------------
     *	create and initialize the executor shared segment (and semaphore)
     * ----------------
     */
    if (ParallelExecutorEnabled()) {
	IpcSemaphoreKill(IPCKeyGetExecutorSemaphoreKey(key));
	ExecInitExecutorSemaphore(IPCKeyGetExecutorSemaphoreKey(key));
    
	IpcMemoryKill(IPCKeyGetExecutorSharedMemoryKey(key));
	ExecCreateExecutorSharedMemory(IPCKeyGetExecutorSharedMemoryKey(key));
	ExecAttachExecutorSharedMemory();
    }
    
    /* ----------------
     *	do the lock table stuff
     * ----------------
     */
    LtSynchInit(IPCKeyGetLockTableMemoryKey(key));
    LtTransactionSemaphoreInit(IPCKeyGetLockTableSemaphoreKey(key));

    status = LMSegmentInit(true, IPCKeyGetLockTableSemaphoreBlockKey(key));

    if (status == -1)
	elog(FATAL, "CreateSharedMemoryAndSemaphores: failed LMSegmentInit");

    PageLockTableId = LMLockTableCreate("page",
					PageLockTable,
					LockTableNormal);
    
    MultiLevelLockTableId = LMLockTableCreate("multiLevel",
					      MultiLevelLockTable,
					      LockTableNormal);

    CreateSharedInvalidationState(key);
}


void
AttachSharedMemoryAndSemaphores(key)
    IPCKey	key;
{
    int	status;

    /* ----------------
     *	create rather than attach if using private key
     * ----------------
     */
    if (key == PrivateIPCKey) {
	CreateSharedMemoryAndSemaphores(key);
	return;
    }

    /* ----------------
     *	attach the buffer manager buffer pool (and semaphore)
     * ----------------
     */
    InitBufferSemaphore(IPCKeyGetBufferSemaphoreKey(key));
    InitBufferPoolMemory(IPCKeyGetBufferMemoryKey(key), false);
    AttachSharedBuffers(IPCKeyGetBufferMemoryKey(key));

    /* ----------------
     *	attach the parallel executor memory (and semaphore)
     * ----------------
     */
    if (ParallelExecutorEnabled()) {
	ExecInitExecutorSemaphore(IPCKeyGetExecutorSemaphoreKey(key));
	ExecLocateExecutorSharedMemory(IPCKeyGetExecutorSharedMemoryKey(key));
	ExecAttachExecutorSharedMemory();
    }
    
    /* ----------------
     *	initialize lock table stuff
     * ----------------
     */
    LtSynchInit(IPCKeyGetLockTableMemoryKey(key));
    LtTransactionSemaphoreInit(IPCKeyGetLockTableSemaphoreKey(key));

    status = LMSegmentInit(false, IPCKeyGetLockTableSemaphoreBlockKey(key));

    if (status == -1)
	elog(FATAL, "AttachSharedMemoryAndSemaphores: failed LMSegmentInit");

    PageLockTableId = LMLockTableCreate("page",
					PageLockTable,
					LockTableNormal);
    
    MultiLevelLockTableId = LMLockTableCreate("multiLevel",
					      MultiLevelLockTable,
					      LockTableNormal);

    AttachSharedInvalidationState(key);
}
