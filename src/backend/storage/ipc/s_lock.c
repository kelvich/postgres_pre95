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
 *	If this is not done, POSTGRES will default to using System V
 *	semaphores (and take a large performance hit -- around 40% of
 *	its time on a DS5000/240 is spent in semop(3)...).
 *
 *   NOTES
 *	AIX has a test-and-set but the recommended interface is the cs(3)
 *	system call.  This provides an 8-instruction (plus system call 
 *	overhead) uninterruptible compare-and-set operation.  True 
 *	spinlocks might be faster but using cs(3) still speeds up the 
 *	regression test suite by about 25%.  I don't have an assembler
 *	manual for POWER in any case.
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
 * Solaris 2
 *
 * Note that slock_t on the Alpha AXP is long instead of char
 * (see storage/ipc.h).
 */

#if defined(PORTNAME_alpha) || \
    defined(PORTNAME_sparc_solaris)

/* defined in port/.../tas.s */
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
 * Note that slock_t on POWER/POWER2/PowerPC is int instead of char
 * (see storage/ipc.h).
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
 * HP-UX (PA-RISC)
 *
 * Note that slock_t on PA-RISC is a structure instead of char
 * (see storage/ipc.h).
 */

#if defined(PORTNAME_hpux)

/* defined in port/.../tas.s */
extern int tas ARGS((slock_t *lock));

/*
* a "set" slock_t has a single word cleared.  a "clear" slock_t has 
* all words set to non-zero.
*/
static slock_t clear_lock = { -1, -1, -1, -1 };

S_LOCK(lock)
    slock_t *lock;
{
    while (tas(lock))
	;
}

S_UNLOCK(lock)
    slock_t *lock;
{
    *lock = clear_lock;	/* struct assignment */
}

S_INIT_LOCK(lock)
    slock_t *lock;
{
    S_UNLOCK(lock);
}

S_LOCK_FREE(lock)
    slock_t *lock;
{
    register int *lock_word = (int *) (((long) lock + 15) & ~15);

    return(*lock_word != 0);
}

#endif /* PORTNAME_hpux */

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
 * SPARC (SunOS 4)
 */

#if defined(PORTNAME_sparc)

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

#endif /* PORTNAME_sparc */

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

#endif /* __i386__ && __GNUC__ */

#endif /* HAS_TEST_AND_SET */
