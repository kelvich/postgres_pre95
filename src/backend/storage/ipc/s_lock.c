
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
 *     while (test_and_set(char_address) != *char_address);
 * }
 *
 * If this is not done, Postgres will default to using System V semaphores,
 * which result in a 100% degradation of performance.
 */

/*
 * sun3 
 */
 
#if defined(sun3)

#endif

/*
 * Sparc or sun4
 */

#if defined(sparc) || defined(sun4)

static
tas_dummy()

{
	asm(".seg \"text\"");
	asm(".globl _S_LOCK");

	asm("loop:"); /* top of spin */

	/*
	 * Sparc atomic test and set (sparc calls it "atomic load-store")
	 */

	asm("ldstub  [%RETURN_VAL_REG], %RETURN_VAL_REG"); 

	/*
	 * Did test and set actually do the set?
	 */

	asm("tst %RETURN_VAL_REG");

	/*
	 * if it didn't, just keep spinning
	 */

	asm("bne,a loop");

	/*
	 * otherwise, just return.
	 */

	asm("retl");
	asm("nop");
}

/*
 * addr should be as in the above S_LOCK routine
 */

S_UNLOCK(addr)

char *addr;

{
	*addr = 0;
}

S_INIT_LOCK(addr)

char *addr;

{
	*addr = 0;
}

#endif /* sparc */
