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
 * sequent: real spin locks. uses a TAS instruction.
 *
 * default: fake spin locks using semaphores.  see spin.c
 *
 */

typedef int SPINLOCK;

#define is_LOCKED(lock)		Assert(SpinIsLocked(lock))

#endif	/* !defined(SPINIncluded) */
