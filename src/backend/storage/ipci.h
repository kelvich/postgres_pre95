
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

#include "c.h"

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
