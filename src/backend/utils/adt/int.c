
/*
 * 
 * POSTGRES Data Base Management System
 * 
 * Copyright (c) 1988 Regents of the University of California
 * 
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for educational, research, and non-profit purposes and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of the University of California not be used in advertising
 * or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Permission to incorporate this
 * software into commercial products can be obtained from the Campus
 * Software Office, 295 Evans Hall, University of California, Berkeley,
 * Ca., 94720 provided only that the the requestor give the University
 * of California a free licence to any derived software for educational
 * and research purposes.  The University of California makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 * 
 */



/*
 * int.c --
 * 	Functions for the built-in integer types.
 *
 *	I/O routines:
 *	 int2in, int2out, int28in, int28out, int4in, int4out
 *	Conversion routines:
 *	 itoi
 *	Boolean operators:
 *	 inteq, intne, intlt, intle, intgt, intge
 *	Arithmetic operators:
 *	 intpl, intmi, int4mul, intdiv
 *
 * 	Routines for (non-builtin) integer operations.
 *		(included if FMGR_MATH is defined in h/fmgr.h)
 *
 *	Arithmetic operators:
 *	 intmod, int4fac
 */

#include "c.h"
#include "postgres.h"
#include "fmgr.h"

RcsId("$Header$");


	    /* ========== USER I/O ROUTINES ========== */


/*
 *	int2in		- converts "num" to short
 */
int32
int2in(num)
	char	*num;
{
	extern int	atoi();

	return((int32) atoi(num));
}

/*
 *	int2out		- converts short to "num"
 */
char *
int2out(sh)
	int32	sh;
{
	char		*result;
	extern int	itoa();

	result = palloc(7);		/* assumes sign, 5 digits, '\0' */
	itoa((int) sh, result);
	return(result);
}

/*
 *	int28in		- converts "num num ..." to internal form
 *
 *	Note:
 *		Fills any nonexistent digits with NULLs.
 */
int16 *
int28in(shs)
	char	*shs;
{
	register int16	(*result)[];
	int		nums;

	if (shs == NULL)
		return(NULL);
	result = (int16 (*)[]) palloc(sizeof(int16 [8]));
	if ((nums = sscanf(shs, "%hd%hd%hd%hd%hd%hd%hd%hd",
			   *result,
			   *result + 1,
			   *result + 2,
			   *result + 3,
			   *result + 4,
			   *result + 5,
			   *result + 6,
			   *result + 7)) != 8) {
		do
			(*result)[nums++] = 0;
		while (nums < 8);
	}
	return((int16 *) result);
}

/*
 *	int28out	- converts internal form to "num num ..."
 */
char *
int28out(shs)
	int16	(*shs)[];
{
	register int	num;
	register int16	*sp;
	register char	*rp;
	char 		*result;
	extern int	itoa();

	if (shs == NULL) {
		result = palloc(2);
		result[0] = '-';
		result[1] = '\0';
		return(result);
	}
	rp = result = palloc(8 * 7);	/* assumes sign, 5 digits, ' ' */
	sp = *shs;
	for (num = 8; num != 0; num--) {
		itoa(*sp++, rp);
		while (*++rp != '\0')
			;
		*rp++ = ' ';
	}
	*--rp = '\0';
	return(result);
}


	     /* ========== PUBLIC ROUTINES ========== */


/*
 *	int4in		- converts "num" to int4
 */
int32
int4in(num)
	char	*num;
{
	extern long	atol();

	return((int32) atol(num));
}

/*
 *	int4out		- converts int4 to "num"
 */
char *
int4out(l)
	int32	l;
{
	char		*result;
	extern int	ltoa();

	result = palloc(12);		/* assumes sign, 10 digits, '\0' */
	ltoa((long) l, result);
	return(result);
}


/*
 *	===================
 *	CONVERSION ROUTINES
 *	===================
 */

/*
 *	itoi - "convert" arg1 to another integer type
 */
int32
itoi(arg1)
	int32	arg1;
{
	return(arg1);
}


/*
 *	=========================
 *	BOOLEAN OPERATOR ROUTINES
 *	=========================
 *
 *	These operations are used (and work correctly) for nearly
 *	every integer type, e.g., int2, int32, bool, char, OID, ...
 */

/*
 *	inteq		- returns 1 iff arg1 == arg2
 *	intne		- returns 1 iff arg1 != arg2
 *	intlt		- returns 1 iff arg1 < arg2
 *	intle		- returns 1 iff arg1 <= arg2
 *	intgt		- returns 1 iff arg1 > arg2
 *	intge		- returns 1 iff arg1 >= arg2
 */
int32
inteq(arg1, arg2)
	int32	arg1, arg2;
{ return(arg1 == arg2); }

int32
intne(arg1, arg2)
	int32	arg1, arg2;
{ return(arg1 != arg2); }

int32
intlt(arg1, arg2)
	int32	arg1, arg2;
{ return(arg1 < arg2); }

int32
intle(arg1, arg2)
	int32	arg1, arg2;
{ return(arg1 <= arg2); }

int32
intgt(arg1, arg2)
	int32	arg1, arg2;
{ return(arg1 > arg2); }

int32
intge(arg1, arg2)
	int32	arg1, arg2;
{ return(arg1 >= arg2); }

/*
 *	intpl	- returns arg1 + arg2
 *	intmi	- returns arg1 - arg2
 *	int4mul	- returns arg1 * arg2
 *	intdiv	- returns arg1 / arg2
 */
int32
intpl(arg1, arg2)
	int32	arg1, arg2;
{ return(arg1 + arg2); }

int32
intmi(arg1, arg2)
	int32	arg1, arg2;
{ return(arg1 - arg2); }

int32
int4mul(arg1, arg2)
	int32 	arg1, arg2;
{ return(arg1 * arg2); }

int32
intdiv(arg1, arg2)
	int32	arg1, arg2;
{ return(arg1 / arg2); }

#ifdef FMGR_MATH
/*
 *	intmod	- returns arg1 mod arg2
 */
int32
intmod(arg1, arg2)
	int32	arg1, arg2;
{
	return(arg1 % arg2);
}

/*
 *	int4fac	- returns arg1!
 */
int32
int4fac(arg1)
	int32 	arg1;
{
	int32	result;

	if (arg1 < 1)
		result = 0;
	else 
		for (result = 1; arg1 > 0; --arg1)
			result *= arg1;
	return(result);
}
#endif FMGR_MATH


	     /* ========== PRIVATE ROUTINES ========== */


			     /* (none) */

