/*
 * ipc.h --
 *	POSTGRES inter-process communication definitions.
 *
 * Identification:
 *	$Header$
 */

#ifndef	IPCIncluded	/* Include this file only once */
#define IPCIncluded	1

#include "c.h"

typedef uint16	SystemPortAddress;


/* semaphore definitions */

#define IPCProtection	(0600)		/* access/modify by user only */

#define IpcSemaphoreDefaultStartValue	255
#define IpcSharedLock					(-1)
#define IpcExclusiveLock			  (-255)

#define IpcMaxNumSem	50

#define IpcUnknownStatus	(-1)
#define IpcInvalidArgument	(-2)
#define IpcSemIdExist		(-3)
#define IpcSemIdNotExist	(-4)

typedef uint32	IpcSemaphoreKey;		/* semaphore key */
typedef int	IpcSemaphoreId;


/* shared memory definitions */ 

#define IpcMemCreationFailed	(-1)
#define IpcMemIdGetFailed	(-2)
#define IpcMemAttachFailed	0


typedef uint32  IpcMemoryKey;			/* shared memory key */
typedef int	IpcMemoryId;

/*
 * XXX improperly styled declarations
 */
extern IpcSemaphoreId IpcSemaphoreCreate();
extern void IpcSemaphoreKill();
extern void IpcSemaphoreLock();
extern void IpcSemaphoreUnlock();
extern IpcMemoryId IpcMemoryCreate();
extern IpcMemoryId IpcMemoryIdGet();
extern char *IpcMemoryAttach();
extern void IpcMemoryKill();

#endif	/* !defined(IPCIncluded) */
