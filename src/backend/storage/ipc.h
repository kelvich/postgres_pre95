
; /*
; * 
; * POSTGRES Data Base Management System
; * 
; * Copyright (c) 1988 Regents of the University of California
; * 
; * Permission to use, copy, modify, and distribute this software and its
; * documentation for educational, research, and non-profit purposes and
; * without fee is hereby granted, provided that the above copyright
; * notice appear in all copies and that both that copyright notice and
; * this permission notice appear in supporting documentation, and that
; * the name of the University of California not be used in advertising
; * or publicity pertaining to distribution of the software without
; * specific, written prior permission.  Permission to incorporate this
; * software into commercial products can be obtained from the Campus
; * Software Office, 295 Evans Hall, University of California, Berkeley,
; * Ca., 94720 provided only that the the requestor give the University
; * of California a free licence to any derived software for educational
; * and research purposes.  The University of California makes no
; * representations about the suitability of this software for any
; * purpose.  It is provided "as is" without express or implied warranty.
; * 
; */



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
