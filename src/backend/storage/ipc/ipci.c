/*
 * ipci.c --
 *	POSTGRES inter-process communication initialization code.
 */

#include "tmp/c.h"

#include "storage/ipc.h"
#include "utils/log.h"
#include "storage/pladt.h"
#include "storage/sinval.h"
#include "storage/ipci.h"
#include "storage/bufmgr.h"
#include "tcop/slaves.h"

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
    int		size;

#ifdef HAS_TEST_AND_SET
    /* ---------------
     *  create shared memory for slocks
     * --------------
     */
    CreateAndInitSLockMemory(IPCKeyGetSLockSharedMemoryKey(key));
#endif
    /* ----------------
     *	kill and create the buffer manager buffer pool (and semaphore)
     * ----------------
     */
    CreateSpinlocks(IPCKeyGetSpinLockSemaphoreKey(key));
    size = BufferShmemSize() + LockShmemSize();
    ShmemCreate(IPCKeyGetBufferMemoryKey(key), size);
    InitShmem(key, size);
    InitBufferPool(key);

    /* ----------------
     *	create and initialize the executor shared segment (and semaphore)
     * ----------------
     */
    if (key == PrivateIPCKey && ParallelExecutorEnabled()) {
	ExecInitExecutorSemaphore(PrivateIPCKey);
	ExecCreateExecutorSharedMemory(PrivateIPCKey);
	ExecAttachExecutorSharedMemory();
    }
    
    /* ----------------
     *	do the lock table stuff
     * ----------------
     */
    InitLocks();
    InitMultiLevelLockm();
    InitProcess(key);

    CreateSharedInvalidationState(key);
}


void
AttachSharedMemoryAndSemaphores(key)
    IPCKey	key;
{
    int	status;
    int size;

    /* ----------------
     *	create rather than attach if using private key
     * ----------------
     */
    if (key == PrivateIPCKey) {
	CreateSharedMemoryAndSemaphores(key);
	return;
    }

#ifdef HAS_TEST_AND_SET
    /* ----------------
     *  attach the slock shared memory
     * ----------------
     */
    AttachSLockMemory(IPCKeyGetSLockSharedMemoryKey(key));
#endif
    /* ----------------
     *	attach the buffer manager buffer pool (and semaphore)
     * ----------------
     */
    size = BufferShmemSize() + LockShmemSize();
    InitShmem(key, size);
    InitBufferPool(key);

    /* ----------------
     *	create and initialize the executor shared segment (and semaphore)
     * ----------------
     */
    if (ParallelExecutorEnabled()) {
	ExecInitExecutorSemaphore(PrivateIPCKey);
	ExecCreateExecutorSharedMemory(PrivateIPCKey);
	ExecAttachExecutorSharedMemory();
    }

    /* ----------------
     *	initialize lock table stuff
     * ----------------
     */
    InitLocks();
    InitMultiLevelLockm();
    InitProcess(key);

    AttachSharedInvalidationState(key);
}
