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
 *	Arithmetic operators:
 *	 intmod, int4fac
 */

#include "tmp/postgres.h"
#include "utils/fmgr.h"
#include "utils/log.h"

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
	int16	sh;
{
	char		*result;
	extern int	itoa();

	result = (char *)palloc(7);	/* assumes sign, 5 digits, '\0' */
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
		result = (char *)palloc(2);
		result[0] = '-';
		result[1] = '\0';
		return(result);
	}
	rp = result = (char *)palloc(8 * 7); /* assumes sign, 5 digits, ' ' */
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

/*
 *	int28in		- converts "num num ..." to internal form
 *
 *	Note:
 *		Fills any nonexistent digits with NULLs.
 */
int32 *
int44in(input_string)
	char	*input_string;

{
    int32 *foo = (int32 *)palloc(4*sizeof(int32));
    register int i = 0;

    i = sscanf(input_string,
	       "%ld, %ld, %ld, %ld",
	       &foo[0],
	       &foo[1],
	       &foo[2],
	       &foo[3]);
    while (i < 4)
	foo[i++] = 0;

    return(foo);
}

/*
 *	int28out	- converts internal form to "num num ..."
 */
char *
int44out(an_array)
	int32	an_array[];
{
    int temp = 4;
    char *output_string = NULL;
    extern int itoa();
    int i;

    if ( temp > 0 ) {
	char *walk;
	output_string = (char *)palloc(16*temp); /* assume 15 digits + sign */
	walk = output_string;
	for ( i = 0 ; i < temp ; i++ ) {
	    itoa(an_array[i],walk);
	    while (*++walk != '\0')
	      ;
	    *walk++ = ' ';
	}
	*--walk = '\0';
    }
    return(output_string);
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

	result = (char *)palloc(12);	/* assumes sign, 10 digits, '\0' */
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
int32 int4eq(arg1, arg2)	int32	arg1, arg2; { return(arg1 == arg2); }
int32 int4ne(arg1, arg2) 	int32	arg1, arg2; { return(arg1 != arg2); }
int32 int4lt(arg1, arg2) 	int32	arg1, arg2; { return(arg1 < arg2); }
int32 int4le(arg1, arg2) 	int32	arg1, arg2; { return(arg1 <= arg2); }
int32 int4gt(arg1, arg2)	int32	arg1, arg2; { return(arg1 > arg2); } 
int32 int4ge(arg1, arg2)	int32	arg1, arg2; { return(arg1 >= arg2); }

int32 int2eq(arg1, arg2)	int16	arg1, arg2; { return(arg1 == arg2); }
int32 int2ne(arg1, arg2)	int16	arg1, arg2; { return(arg1 != arg2); }
int32 int2lt(arg1, arg2)	int16	arg1, arg2; { return(arg1 < arg2); }
int32 int2le(arg1, arg2)	int16	arg1, arg2; { return(arg1 <= arg2); }
int32 int2gt(arg1, arg2)	int16	arg1, arg2; { return(arg1 > arg2); } 
int32 int2ge(arg1, arg2)	int16	arg1, arg2; { return(arg1 >= arg2); }

int32 keyfirsteq(arg1, arg2)	int16	*arg1,arg2; { return(*arg1 == arg2); }

/*
 *	int[24]pl	- returns arg1 + arg2
 *	int[24]mi	- returns arg1 - arg2
 *	int[24]mul	- returns arg1 * arg2
 *	int[24]div	- returns arg1 / arg2
 */
int32 int4pl(arg1, arg2) 	int32	arg1, arg2; { return(arg1 + arg2); }
int32 int4mi(arg1, arg2)	int32	arg1, arg2; { return(arg1 - arg2); }
int32 int4mul(arg1, arg2)	int32	arg1, arg2; { return(arg1 * arg2); }
int32 int4div(arg1, arg2)	int32	arg1, arg2; { return(arg1 / arg2); }
int32 int4inc(arg)		int32   arg;        { return(arg + (int32)1); }

int32 int2pl(arg1, arg2)	int16	arg1, arg2; { return(arg1 + arg2); }
int32 int2mi(arg1, arg2)	int16	arg1, arg2; { return(arg1 - arg2); }
int32 int2mul(arg1, arg2)	int16	arg1, arg2; { return(arg1 * arg2); }
int32 int2div(arg1, arg2)	int16	arg1, arg2; { return(arg1 / arg2); }
int32 int2inc(arg)		int16   arg;	    { return(arg + (int16)1); }

/*
 *	int[24]mod	- returns arg1 mod arg2
 */
int32 int4mod(arg1, arg2)	int32	arg1, arg2; { return(arg1 % arg2); }
int32 int2mod(arg1, arg2)	int16	arg1, arg2; { return(arg1 % arg2); }

/*
 *	int[24]fac	- returns arg1!
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

int32
int2fac(arg1)
	int16 	arg1;
{
	int16	result;

	if (arg1 < 1)
		result = 0;
	else 
		for (result = 1; arg1 > 0; --arg1)
			result *= arg1;
	return(result);
}

int16
int2larger(arg1, arg2)
	int16	arg1;
	int16	arg2;
{
	return ((arg1 > arg2) ? arg1 : arg2);
}

int16
int2smaller(arg1, arg2)
	int16	arg1;
	int16	arg2;
{
	return ((arg1 > arg2) ? arg2 : arg1);
}

int32
int4larger(arg1, arg2)
	int32	arg1;
	int32	arg2;
{
	return ((arg1 > arg2) ? arg1 : arg2);
}

int32
int4smaller(arg1, arg2)
	int32	arg1;
	int32	arg2;
{
	return ((arg1 > arg2) ? arg2 : arg1);
}
