
/*
 * This routine contains compiled assembly language files for spin locks.
 * In doing ports, machines that have a native test and set instruction should
 * implement the following code fragment in assembly language:
 *
 * void
 * S_LOCK(char_address)
 *
 * char *char_address;
 *
 * {
 *     while (test_and_set(char_address));
 * }
 *
 * If this is not done, Postgres will default to using System V semaphores,
 * which result in a 100% degradation of performance.
 *
 * RcsId("$Header$");
 */

#include "storage/ipc.h"

/*
 * sun3 (and eventually amiga)
 */
 
#if (defined(sun) && ! defined(sparc)) || defined(amiga)

    
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

S_CLOCK(lock)
slock_t *lock;
{
    return(tas(lock));
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
