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

#include "tmp/c.h"

#include "storage/ipc.h"

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

#define IPCKeyGetSLockSharedMemoryKey(key) \
	((key == PrivateIPCKey) ? key : 10 + (key))

#define IPCKeyGetSpinLockSemaphoreKey(key) \
	((key == PrivateIPCKey) ? key : 11 + (key))
#define IPCKeyGetWaitIOSemaphoreKey(key) \
	((key == PrivateIPCKey) ? key : 12 + (key))

#ifdef SONY_JUKEBOX
#define IPCKeyGetSJWaitSemaphoreKey(key) \
	((key == PrivateIPCKey) ? key : 13 + (key))
#endif /* SONY_JUKEBOX */

/* --------------------------
 * NOTE: This macro must always give the highest numbered key as every backend
 * process forked off by the postmaster will be trying to acquire a semaphore
 * with a unique key value starting at key+14 and incrementing up.  Each
 * backend uses the current key value then increments it by one.
 * --------------------------
 */
#define IPCGetProcessSemaphoreInitKey(key) \
	((key == PrivateIPCKey) ? key : 14 + (key))

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
