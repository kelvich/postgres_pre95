/*
 * ipci.c --
 *	POSTGRES inter-process communication initialization code.
 */

#include "tmp/c.h"

#include "storage/ipci.h"
#include "storage/ipc.h"
#include "utils/log.h"
#include "storage/sinval.h"
#include "storage/bufmgr.h"
#include "storage/lock.h"
#include "tcop/slaves.h"
#include "tmp/miscadmin.h"	/* for DebugLvl */

RcsId("$Header$");

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

#ifdef SONY_JUKEBOX
    size += SJShmemSize();
#endif /* SONY_JUKEBOX */

#ifdef MAIN_MEMORY
    size += MMShmemSize();
#endif /* MAIN_MEMORY */

    if (DebugLvl > 1) {
	fprintf(stderr, "binding ShmemCreate(key=%x, size=%d)\n",
		IPCKeyGetBufferMemoryKey(key), size);
    }
    ShmemCreate(IPCKeyGetBufferMemoryKey(key), size);
    ShmemBindingTabReset();
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

#ifdef SONY_JUKEBOX
    /* ----------------
     *	create and initialize the sony jukebox shared semaphore
     * ----------------
     */
    SJInitSemaphore(key);
#endif

    /* ----------------
     *	do the lock table stuff
     * ----------------
     */
    InitLocks();
    InitMultiLevelLockm();
    if (InitMultiLevelLockm() == INVALID_TABLEID)
	elog(FATAL, "Couldn't create the lock table");

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

#ifdef SONY_JUKEBOX
    /* ----------------
     *	create and initialize the sony jukebox shared semaphore
     * ----------------
     */
    SJInitSemaphore(key);
#endif

    /* ----------------
     *	initialize lock table stuff
     * ----------------
     */
    InitLocks();
    if (InitMultiLevelLockm() == INVALID_TABLEID)
	elog(FATAL, "Couldn't attach to the lock table");

    AttachSharedInvalidationState(key);
}
