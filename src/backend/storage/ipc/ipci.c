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

	/* REMOVED
	LtSynchKill(IPCKeyGetLockTableMemoryKey(key));
	*/
	/* REMOVED
	 * LtTransactionSemaphoreKill(IPCKeyGetLockTableSemaphoreKey(key));
	*/

	status = LMSegmentInit(true, IPCKeyGetLockTableMemoryKey(key));

	/* kill and create the buffer manager buffer pool (and semaphore) */
	/* REMOVED
	 * CreateBufferSemaphore(IPCKeyGetBufferSemaphoreKey(key));
	*/
	CreateBufferPoolMemory(IPCKeyGetBufferMemoryKey(key));

	/* REMOVED
	LtSynchInit(IPCKeyGetLockTableMemoryKey(key));
	*/
	/* REMOVED
	 *LtTransactionSemaphoreInit(IPCKeyGetLockTableSemaphoreKey(key));
	*/


	if (status == -1) {
		elog(FATAL, "CreateSharedMemory: failed segment init");
	}

	PageLockTableId = LMLockTableCreate("page", PageLockTable,
		LockTableNormal);
	MultiLevelLockTableId = LMLockTableCreate("multiLevel",
		MultiLevelLockTable, LockTableNormal);

	CreateSharedInvalidationState(key);
}

void
AttachSharedMemoryAndSemaphores(key)
	IPCKey	key;
{
	int	status;

	if (key == PrivateIPCKey) {
		CreateSharedMemoryAndSemaphores(key);
		return;
	}

	/* attach the buffer manager buffer pool (and semaphore) */
	/* REMOVED
	 * InitBufferSemaphore(IPCKeyGetBufferSemaphoreKey(key));
	*/
	InitBufferPoolMemory(IPCKeyGetBufferMemoryKey(key), false);

	/* REMOVED
	LtSynchInit(IPCKeyGetLockTableMemoryKey(key));
	*/
	/* REMOVED 
	 * LtTransactionSemaphoreInit(IPCKeyGetLockTableSemaphoreKey(key));
	*/

	status = LMSegmentInit(false, IPCKeyGetLockTableMemoryKey(key));

	AttachSharedBuffers(IPCKeyGetBufferMemoryKey(key));

	if (status == -1) {
		elog(FATAL, "AttachSharedMemory: failed segment init");
	}

	PageLockTableId = LMLockTableCreate("page", PageLockTable,
		LockTableNormal);
	MultiLevelLockTableId = LMLockTableCreate("multiLevel",
		MultiLevelLockTable, LockTableNormal);

	AttachSharedInvalidationState(key);
}
