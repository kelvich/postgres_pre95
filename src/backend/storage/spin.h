/*
 * spin.h -- synchronization routines
 *
 * Identification:
 *	$Header$
 */

#ifndef	SPINIncluded	/* Include this file only once */
#define SPINIncluded	1

/* 
 * Three (or more) possible implementations of spinlocks.
 *
 * SPIN_DBG: running as single user so we can fake the
 * 	spin locks with a normal integer.  Any blocking
 *	causes an error.
 *
 * SPIN_OK: real spin locks. requires a TAS instruction.
 *
 * default: fake spin locks using semaphores.  see spin.c
 *
 */


#ifdef SPIN_DBG

typedef int SPINLOCK;

#define is_LOCKED(a) Assert((a->tas)==1)
#define SpinAcquire(spinlock) Assert(! (*spinlock));(*spinlock)= 1;
#define SpinRelease(spinlock) Assert(*spinlock); (*spinlock) = 0;
#define SpinCreate(spinlock) (*spinlock)=1;


#else 
struct spinlock {
  int tas;
};

#ifdef SPIN_OK

typedef struct spinlock SPINLOCK;

#define is_LOCKED(a) Assert((a->tas)==1)
#define SpinAcquire(spinlock) while( ! TAS(*spinlock) );
#define SpinRelease(spinlock) (*spinlock) = 0;
#define SpinCreate(spinlock) SpinAcquire(spinlock);

#else

#define is_LOCKED(a) Assert(SpinIsLocked(a))
typedef struct spinlock SPINLOCK;

#endif
#endif

/* spin.c */
InitSpinLocks();
SPINLOCK *SpinAlloc();
#endif	/* !defined(SPINIncluded) */
