/* ----------------------------------------------------------------
 *   FILE
 *	s_lock.c
 *
 *   DESCRIPTION
 *	This file contains the implementation (if any) for spinlocks.
 *	The following code fragment should be written (in assembly 
 *	language) on machines that have a native test-and-set instruction:
 *
 *	void
 *	S_LOCK(char_address)
 *	    char *char_address;
 *	{
 *	    while (test_and_set(char_address))
 *		;
 *	}
 *
 *	If this is not done, Postgres will default to using System V
 *	semaphores (and take a large performance hit -- around 40% of
 *	its time on a DS5000/240 is spent in semop(3)...).
 *
 *   NOTES
 *	AIX has a test-and-set but the recommended interface is the cs(3)
 *	system call.  This provides an 8-instruction (plus system call 
 *	overhead) uninterruptible compare-and-set operation.  True 
 *	spinlocks might be faster but using cs(3) still speeds up the 
 *	regression test suite by about 25%.
 *
 *	None of the TAS code we've written is likely to work on shared-
 *	memory multiprocessors (at least, those that require explicit
 *	memory barriers or such, like the Alpha).
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#include "storage/ipci.h"
#include "storage/ipc.h"

RcsId("$Header$");

#if defined(HAS_TEST_AND_SET)

/*
 * OSF/1 (Alpha AXP)
 *
 * Note that slock_t is long instead of char (see storage/ipc.h).
 */

#if defined(PORTNAME_alpha)

/* defined in port/alpha/tas.s */
extern int tas ARGS((slock_t *lock));

S_LOCK(lock)
    slock_t *lock;
{
    while (tas(lock))
	;
}

S_UNLOCK(lock)
    slock_t *lock;
{
    *lock = 0;
}

S_INIT_LOCK(lock)
    slock_t *lock;
{
    S_UNLOCK(lock);
}

#endif /* PORTNAME_alpha */

/*
 * AIX (POWER)
 *
 * Note that slock_t is int instead of char (see storage/ipc.h).
 */

#if defined(PORTNAME_aix)

S_LOCK(lock)
    slock_t *lock;
{
    while (cs((int *) lock, 0, 1))
	;
}

S_UNLOCK(lock)
    slock_t *lock;
{
    *lock = 0;
}

S_INIT_LOCK(lock)
    slock_t *lock;
{
    S_UNLOCK(lock);
}

#endif /* PORTNAME_aix */

/*
 * sun3
 */
 
#if (defined(sun) && ! defined(sparc))

    
S_LOCK(lock)
slock_t *lock;
{
    while (tas(lock));
}

S_UNLOCK(lock)
slock_t *lock;
{
    *lock = 0;
}

S_INIT_LOCK(lock)
slock_t *lock;
{
    S_UNLOCK(lock);
}

static
tas_dummy()
{
asm("LLA0:");
asm("	.data");
asm("	.text");
asm("|#PROC# 04");
asm("	.globl	_tas");
asm("_tas:");
asm("|#PROLOGUE# 1");
asm("	movel   sp@(0x4),a0");
asm("	tas	a0@");
asm("	beq	LLA1");
asm("	moveq   #-128,d0");
asm("	rts");
asm("LLA1:");
asm("	moveq   #0,d0");
asm("	rts");
asm("	.data");
}

#endif

/*
 * Sparc or sun4
 */

#if defined(sparc) || defined(sun4)

static
tas_dummy()

{
	asm(".seg \"data\"");
	asm(".seg \"text\"");
	asm(".global _tas");
	asm("_tas:");

	/*
	 * Sparc atomic test and set (sparc calls it "atomic load-store")
	 */

	asm("ldstub [%r8], %r8");

	/*
	 * Did test and set actually do the set?
	 */

	asm("tst %r8");

	asm("be,a ReturnZero");

	/*
	 * otherwise, just return.
	 */

	asm("clr %r8");
	asm("mov 0x1, %r8");
	asm("ReturnZero:");
	asm("retl");
	asm("nop");
}

S_LOCK(addr)

unsigned char *addr;

{
	while (tas(addr));
}


/*
 * addr should be as in the above S_LOCK routine
 */

S_UNLOCK(addr)

unsigned char *addr;

{
	*addr = 0;
}

S_INIT_LOCK(addr)

unsigned char *addr;

{
	*addr = 0;
}

#endif /* sparc */

/*
 * Linux and friends
 */

#if defined(__i386__) && defined(__GNUC__)

tas(lock)
    slock_t *lock;
{
    slock_t res;
    __asm__("xchgb %0,%1":"=q" (res),"=m" (*m):"0" (0x1));
    return(res);
}

S_LOCK(lock)
    slock_t *lock;
{
    while (tas(addr))
	;
}

S_UNLOCK(lock)
    slock_t *lock;
{
    *lock = 0;
}

S_INIT_LOCK(lock)
    slock_t *lock;
{
    S_UNLOCK(lock);
}

#endif /* __i386__ && __GNUC__ */

#endif /* HAS_TEST_AND_SET */
