/*
 * ipci.h --
 *	POSTGRES inter-process communication initialization definitions.
 *
 * $Header$
 */

typedef unsigned long	IPCKey;

#define DefaultIPCKey	17317

#define SystemPortAddressGetIPCKey(address) (28597 * (address) + 17491)

#define IPCKeyGetBufferMemoryKey(key)			(1 + (key))
#define IPCKeyGetBufferSemaphoreKey(key)		(2 + (key))
#define IPCKeyGetLockTableMemoryKey(key)		(3 + (key))
#define IPCKeyGetLockTableSemaphoreKey(key)		(4 + (key))
#define IPCKeyGetLockTableSemaphoreBlockKey(key)	(5 + (key))
#define IPCKeyGetSIBufferMemorySemaphoreKey(key)	(6 + (key))
#define IPCKeyGetSIBufferMemoryBlock(key)		(7 + (key))
#define IPCKeyGetExecutorSemaphoreKey(key)		(8 + (key))
#define IPCKeyGetExecutorSharedMemoryKey(key)		(9 + (key))
#define IPCKeyGetSLockSharedMemoryKey(key)		(10 + (key))
#define IPCKeyGetSpinLockSemaphoreKey(key)		(11 + (key))
#define IPCKeyGetWaitIOSemaphoreKey(key)		(12 + (key))
#ifdef SONY_JUKEBOX
#define IPCKeyGetSJWaitSemaphoreKey(key)		(13 + (key))
#endif /* SONY_JUKEBOX */
#define IPCGetProcessSemaphoreInitKey(key)		(14 + (key))
