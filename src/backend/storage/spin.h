/*
 * spin.h -- synchronization routines
 *
 * Identification:
 *	$Header$
 */

#ifndef	SPINIncluded	/* Include this file only once */
#define SPINIncluded	1

/* 
 * two implementations of spin locks
 *
 * sequent, sparc, sun3: real spin locks. uses a TAS instruction; see
 * src/storage/ipc/s_lock.c for details.
 *
 * default: fake spin locks using semaphores.  see spin.c
 *
 */

typedef int SPINLOCK;

#define is_LOCKED(lock)		Assert(SpinIsLocked(lock))

#endif	/* !defined(SPINIncluded) */
