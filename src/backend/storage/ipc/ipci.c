
/*
 * 
 * POSTGRES Data Base Management System
 * 
 * Copyright (c) 1988 Regents of the University of California
 * 
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for educational, research, and non-profit purposes and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of the University of California not be used in advertising
 * or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Permission to incorporate this
 * software into commercial products can be obtained from the Campus
 * Software Office, 295 Evans Hall, University of California, Berkeley,
 * Ca., 94720 provided only that the the requestor give the University
 * of California a free licence to any derived software for educational
 * and research purposes.  The University of California makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 * 
 */



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

	LtSynchKill(IPCKeyGetLockTableMemoryKey(key));
	LtTransactionSemaphoreKill(IPCKeyGetLockTableSemaphoreKey(key));

	/* kill and create the buffer manager buffer pool (and semaphore) */
	CreateBufferSemaphore(IPCKeyGetBufferSemaphoreKey(key));
	CreateBufferPoolMemory(IPCKeyGetBufferMemoryKey(key));

	LtSynchInit(IPCKeyGetLockTableMemoryKey(key));
	LtTransactionSemaphoreInit(IPCKeyGetLockTableSemaphoreKey(key));

	status = LMSegmentInit(true, IPCKeyGetLockTableSemaphoreBlockKey(key));

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
	InitBufferSemaphore(IPCKeyGetBufferSemaphoreKey(key));
	InitBufferPoolMemory(IPCKeyGetBufferMemoryKey(key), false);
	AttachSharedBuffers(IPCKeyGetBufferMemoryKey(key));

	LtSynchInit(IPCKeyGetLockTableMemoryKey(key));
	LtTransactionSemaphoreInit(IPCKeyGetLockTableSemaphoreKey(key));

	status = LMSegmentInit(false, IPCKeyGetLockTableSemaphoreBlockKey(key));

	if (status == -1) {
		elog(FATAL, "AttachSharedMemory: failed segment init");
	}

	PageLockTableId = LMLockTableCreate("page", PageLockTable,
		LockTableNormal);
	MultiLevelLockTableId = LMLockTableCreate("multiLevel",
		MultiLevelLockTable, LockTableNormal);

	AttachSharedInvalidationState(key);
}
