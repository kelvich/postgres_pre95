/*
 * ipci.h --
 *	POSTGRES inter-process communication initialization definitions.
 *
 * Identification:
 *	$Header$
 */

#ifndef	IPCIIncluded	/* Include this file only once */
#define IPCIIncluded	1

#include <sys/types.h>
#ifndef	_IPC_
#define _IPC_
#include <sys/ipc.h>
#endif

#ifndef C_H
#include "c.h"
#endif

#include "ipc.h"
#include "pladt.h"

typedef uint32	IPCKey;

#define PrivateIPCKey	IPC_PRIVATE
#define DefaultIPCKey	17317

/*
 * Note:
 *	These must not hash to DefaultIPCKey or PrivateIPCKey.
 */
#define SystemPortAddressGetIPCKey(address) \
	(28597 * (address) + 17491)

#define IPCKeyGetBufferMemoryKey(key) \
	((key == PrivateIPCKey) ? key : 1 + (key))
#define IPCKeyGetBufferSemaphoreKey(key) \
	((key == PrivateIPCKey) ? key : 2 + (key))

#define IPCKeyGetLockTableMemoryKey(key) \
	((key == PrivateIPCKey) ? key : 3 + (key))
#define IPCKeyGetLockTableSemaphoreKey(key) \
	((key == PrivateIPCKey) ? key : 4 + (key))
#define IPCKeyGetLockTableSemaphoreBlockKey(key) \
	((key == PrivateIPCKey) ? key : 5 + (key))
#define IPCKeyGetSIBufferMemorySemaphoreKey(key) \
	((key == PrivateIPCKey) ? key : 6 + (key))
#define IPCKeyGetSIBufferMemoryBlock(key) \
	((key == PrivateIPCKey) ? key : 7 + (key))

#define IPCKeyGetExecutorSemaphoreKey(key) \
	((key == PrivateIPCKey) ? key : 8 + (key))
#define IPCKeyGetExecutorSharedMemoryKey(key) \
	((key == PrivateIPCKey) ? key : 9 + (key))

extern LockTableId	PageLockTableId;
extern LockTableId	MultiLevelLockTableId;

/*
 * SystemPortAddressCreateMemoryKey --
 *	Returns a memory key given a port address.
 */
extern
IPCKey
SystemPortAddressCreateMemoryKey ARGS((
	SystemPortAddress	address
));

/*
 * CreateSharedMemoryAndSemaphores --
 *	Creates and initializes shared memory and semaphores.
 */
extern
void
CreateSharedMemoryAndSemaphores ARGS((
	IPCKey	key
));

/*
 * AttachSharedMemoryAndSemaphores --
 *	Attachs existant shared memory and semaphores.
 */
extern
void
AttachSharedMemoryAndSemaphores ARGS((
	IPCKey	key
));

#endif	/* !defined(IPCIIncluded) */
