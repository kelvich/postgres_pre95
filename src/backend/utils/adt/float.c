/*
 * float.c --
 *      Functions for the built-in floating-point types.
 *
 *	Basic float4 ops:
 * 	 float4in, float4out, float4abs, float4um
 *	Basic float8 ops:
 *	 float8in, float8inAd, float8out, float8outAd, float8abs, float8um
 *	Arithmetic operators:
 *	 float4pl, float4mi, float4mul, float4div
 *	 float8pl, float8mi, float8mul, float8div
 *	Comparison operators:
 *	 float4eq, float4ne, float4lt, float4le, float4gt, float4ge
 *	 float8eq, float8ne, float8lt, float8le, float8gt, float8ge
 *	Conversion routines:
 *	 ftod, dtof
 *
 *	Random float8 ops:
 * 	 dround, dtrunc, dsqrt, dcbrt, dpow, dexp, dlog1
 *	Arithmetic operators:
 *	 float48pl, float48mi, float48mul, float48div
 *	 float84pl, float84mi, float84mul, float84div
 *	Comparison operators:
 *	 float48eq, float48ne, float48lt, float48le, float48gt, float48ge
 *	 float84eq, float84ne, float84lt, float84le, float84gt, float84ge
 *
 *	(You can do the arithmetic and comparison stuff using conversion
 *	 routines, but then you pay the overhead of converting...)
 */

#include <strings.h>
#include <ctype.h>
#include <stdlib.h>
#include <errno.h>

#ifdef sparc /* this is because SunOS doesn't have all the ANSI C library headers */
#include <values.h>
#define FLT_MIN   MINFLOAT
#define FLT_MAX   MAXFLOAT
#define FLT_DIG   6
#define DBL_DIG   15
#else
#include <float.h>
#endif

#include <math.h>

#include "tmp/postgres.h"
#include "fmgr.h"
#include "utils/builtins.h"	/* for ftod() prototype */
#include "utils/log.h"

RcsId("$Header$");


#define FORMAT 		'g'	/* use "g" output format as standard format */
/* not sure what the following should be, but better to make it over-sufficient */
#define	MAXFLOATWIDTH 	64
#define MAXDOUBLEWIDTH	128

extern double	atof ARGS((const char *p));

#ifdef NEED_CBRT
#define cbrt my_cbrt
static double   cbrt ARGS((double x));
#else /* NEED_CBRT */
extern double   cbrt ARGS((double x));
#endif /* NEED_CBRT */

#ifdef NEED_RINT
#define rint my_rint
static double   rint ARGS((double x));
#else /* NEED_RINT */
extern double   rint ARGS((double x));
#endif /* NEED_RINT */

	    /* ========== USER I/O ROUTINES ========== */


/*
 *	float4in	- converts "num" to float
 *			  restricted syntax:
 *			  {<sp>} [+|-] {digit} [.{digit}] [<exp>]
 *			  where <sp> is a space, digit is 0-9,
 *			  <exp> is "e" or "E" followed by an integer.
 */
float32
float4in(num)
	char	*num;
{
  

	float32	result = (float32) palloc(sizeof(float32data));
	double val;
	char* endptr;

	errno = 0;
	val = strtod(num,&endptr);
	if (*endptr != '\0' || errno == ERANGE)
	  elog(WARN,"\tBad float4 input format\n");
	
	/* if we get here, we have a legal double, still need to check to see
	   if it's a legal float */

	if (fabs(val) > FLT_MAX)
	  elog(WARN,"\tBad float4 input format -- overflow\n");
	if (val > 0.0 && fabs(val) < FLT_MIN)
	  elog(WARN,"\tBad float4 input format -- underflow\n");

	*result = val;
	return result;

}


/*
 *	float4out	- converts a float4 number to a string
 *			  using a standard output format
 */
char *
float4out(num)
	float32	num;
{
	
	char	*ascii = (char *)palloc(MAXFLOATWIDTH+1);	
	
	if (!num)
		return strcpy(ascii, "(null)");

	sprintf(ascii, "%.*g", FLT_DIG, *num);
	return(ascii);
}


/*
 *	float8in	- converts "num" to float8
 *			  restricted syntax:
 *			  {<sp>} [+|-] {digit} [.{digit}] [<exp>]
 *			  where <sp> is a space, digit is 0-9,
 *			  <exp> is "e" or "E" followed by an integer.
 */
float64
float8in(num)
	char	*num;
{
	float64	result = (float64) palloc(sizeof(float64data));
	double val;
	char* endptr;

	errno = 0;
	val = strtod(num,&endptr);
	if (*endptr != '\0' || errno == ERANGE)
	  elog(WARN,"\tBad float8 input format\n");
	
	*result = val;
	return(result);

}


/*
 *	float8out	- converts float8 number to a string
 *			  using a standard output format
 */
char *
float8out(num)
	float64	num;
{
	char	*ascii = (char *)palloc(MAXDOUBLEWIDTH+1);

	if (!num)
		return strcpy(ascii, "(null)");

	sprintf(ascii, "%.*g", DBL_DIG, *num);
	return(ascii);
}

	     /* ========== PUBLIC ROUTINES ========== */


/*
 *	======================
 *	FLOAT4 BASE OPERATIONS
 *	======================
 */

/*
 *	float4abs	- returns a pointer to |arg1| (absolute value)
 */
float32
float4abs(arg1)
	float32	arg1;
{
	float64	dblarg1;
	float32	result;
	double tmp;
	
	if (!arg1)
		return (float32)NULL;

	dblarg1 = ftod(arg1);
	result = (float32) palloc(sizeof(float32data));

	tmp = *dblarg1;
	*result = (float32data) fabs(tmp);
	return(result);
}

/*
 *	float4um 	- returns a pointer to -arg1 (unary minus)
 */
float32
float4um(arg1)
	float32	arg1;
{
	float32	result;
	
	if (!arg1)
		return (float32)NULL;

	result = (float32) palloc(sizeof(float32data));

	*result = -(*arg1);
	return(result);
}

float32
float4larger(arg1, arg2)
	float32 arg1;
	float32 arg2;
{
	float32	result;

	if (!arg1 || !arg2)
		return (float32)NULL;

	result = (float32) palloc(sizeof(float32data));

	*result = ((*arg1 > *arg2) ? *arg1 : *arg2);
	return result;
}

float32
float4smaller(arg1, arg2)
	float32 arg1;
	float32 arg2;
{
	float32	result;

	if (!arg1 || !arg2)
		return (float32)NULL;

	result = (float32) palloc(sizeof(float32data));

	*result = ((*arg1 > *arg2) ? *arg2 : *arg1);
	return result;
}

/*
 *	======================
 *	FLOAT8 BASE OPERATIONS
 *	======================
 */

/*
 *	float8abs	- returns a pointer to |arg1| (absolute value)
 */
float64
float8abs(arg1)
 	float64	arg1;
{
	float64	result;
	double tmp;
	
	if (!arg1)
		return (float64)NULL;

	result = (float64) palloc(sizeof(float64data));

	tmp = *arg1;
	*result = fabs(tmp);
	return(result);
}


/*
 *	float8um	- returns a pointer to -arg1 (unary minus)
 */
float64
float8um(arg1)
	float64	arg1;
{
	float64	result;
	
	if (!arg1)
		return (float64)NULL;

	result = (float64) palloc(sizeof(float64data));

	*result = -(*arg1);
	return(result);
}

float64
float8larger(arg1, arg2)
	float64 arg1;
	float64 arg2;
{
	float64	result;

	if (!arg1 || !arg2)
		return (float64)NULL;

	result = (float64) palloc(sizeof(float64data));

	*result = ((*arg1 > *arg2) ? *arg1 : *arg2);
	return result;
}

float64
float8smaller(arg1, arg2)
	float64 arg1;
	float64 arg2;
{
	float64	result;

	if (!arg1 || !arg2)
		return (float64)NULL;

	result = (float64) palloc(sizeof(float64data));

	*result = ((*arg1 > *arg2) ? *arg2 : *arg1);
	return result;
}


/*
 *	====================
 *	ARITHMETIC OPERATORS
 *	====================
 */

/*
 *	float4pl	- returns a pointer to arg1 + arg2
 *	float4mi	- returns a pointer to arg1 - arg2
 *	float4mul	- returns a pointer to arg1 * arg2
 *	float4div	- returns a pointer to arg1 / arg2
 *	float4inc	- returns a poniter to arg1 + 1.0
 */
float32 
float4pl(arg1, arg2)
	float32	arg1, arg2;
{
	float32	result;

	if (!arg1 || !arg2)
		return (float32)NULL;

	result = (float32) palloc(sizeof(float32data));

	*result = *arg1 + *arg2;
	return(result);
}

float32
float4mi(arg1, arg2)
	float32	arg1, arg2;
{
	float32	result;

	if (!arg1 || !arg2)
		return (float32)NULL;

	result = (float32) palloc(sizeof(float32data));

	*result = *arg1 - *arg2;
	return(result);
}

float32
float4mul(arg1, arg2)
	float32	arg1, arg2;
{
	
	float32	result;

	if (!arg1 || !arg2)
		return (float32)NULL;

	result = (float32) palloc(sizeof(float32data));

	*result = *arg1 * *arg2;
	return(result);
}

float32
float4div(arg1, arg2)
	float32	arg1, arg2;
{
	float32	result;

	if (!arg1 || !arg2)
		return (float32)NULL;

	result = (float32) palloc(sizeof(float32data));

	*result = *arg1 / *arg2;
	return(result);
}

float32
float4inc(arg1)
	float32 arg1;
{
	if (!arg1)
		return (float32)NULL;

	*arg1 = *arg1 + (float32data)1.0;
	return arg1;
}

/*
 *	float8pl	- returns a pointer to arg1 + arg2
 *	float8mi	- returns a pointer to arg1 - arg2
 *	float8mul	- returns a pointer to arg1 * arg2
 *	float8div	- returns a pointer to arg1 / arg2
 *	float8inc	- returns a pointer to arg1 + 1.0
 */
float64 
float8pl(arg1, arg2)
	float64	arg1, arg2;
{
	float64	result;

	if (!arg1 || !arg2)
		return (float64)NULL;

	result = (float64) palloc(sizeof(float64data));

	*result = *arg1 + *arg2;
	return(result);
}

float64
float8mi(arg1, arg2)
	float64	arg1, arg2;
{
	float64	result;

	if (!arg1 || !arg2)
		return (float64)NULL;

	result = (float64) palloc(sizeof(float64data));

	*result = *arg1 - *arg2;
	return(result);
}

float64
float8mul(arg1, arg2)
	float64	arg1, arg2;
{
	float64	result;

	if (!arg1 || !arg2)
		return (float64)NULL;

	result = (float64) palloc(sizeof(float64data));

	*result = *arg1 * *arg2;
	return(result);
}

float64
float8div(arg1, arg2)
	float64	arg1, arg2;
{
	float64	result;

	if (!arg1 || !arg2)
		return (float64)NULL;

	result = (float64) palloc(sizeof(float64data));

	*result = *arg1 / *arg2;
	return(result);
}

float64
float8inc(arg1)
	float64	arg1;
{
	if (!arg1)
		return (float64)NULL;

	*arg1 = *arg1 + (float64data)1.0;
	return(arg1);
}


/*
 *	====================
 *	COMPARISON OPERATORS
 *	====================
 */

/*
 *	float4{eq,ne,lt,le,gt,ge}	- float4/float4 comparison operations
 */
long
float4eq(arg1, arg2)
	float32	arg1, arg2;
{
	if (!arg1 || !arg2)
		return (long)NULL;

	return(*arg1 == *arg2);
}

long
float4ne(arg1, arg2)
	float32	arg1, arg2;
{
	if (!arg1 || !arg2)
		return (long)NULL;

	return(*arg1 != *arg2);
}

long
float4lt(arg1, arg2)
	float32	arg1, arg2;
{
	if (!arg1 || !arg2)
		return (long)NULL;

	return(*arg1 < *arg2);
}

long
float4le(arg1, arg2)
	float32	arg1, arg2;
{
	if (!arg1 || !arg2)
		return (long)NULL;

	return(*arg1 <= *arg2);
}

long
float4gt(arg1, arg2)
	float32	arg1, arg2;
{
	if (!arg1 || !arg2)
		return (long)NULL;

	return(*arg1 > *arg2);
}

long
float4ge(arg1, arg2)
	float32	arg1, arg2;
{
	if (!arg1 || !arg2)
		return (long)NULL;

	return(*arg1 >= *arg2);
}

/*
 *	float8{eq,ne,lt,le,gt,ge}	- float8/float8 comparison operations
 */
long
float8eq(arg1, arg2)
	float64	arg1, arg2;
{
	if (!arg1 || !arg2)
		return (long)NULL;

	return(*arg1 == *arg2);
}

long
float8ne(arg1, arg2)
	float64	arg1, arg2;
{
	if (!arg1 || !arg2)
		return (long)NULL;

	return(*arg1 != *arg2);
}

long
float8lt(arg1, arg2)
	float64	arg1, arg2;
{
	if (!arg1 || !arg2)
		return (long)NULL;

	return(*arg1 < *arg2);
}

long
float8le(arg1, arg2)
	float64	arg1, arg2;
{
	if (!arg1 || !arg2)
		return (long)NULL;

	return(*arg1 <= *arg2);
}

long
float8gt(arg1, arg2)
	float64	arg1, arg2;
{
	if (!arg1 || !arg2)
		return (long)NULL;

	return(*arg1 > *arg2);
}

long
float8ge(arg1, arg2)
	float64	arg1, arg2;
{
	if (!arg1 || !arg2)
		return (long)NULL;

	return(*arg1 >= *arg2);
}


/*
 *	===================
 *	CONVERSION ROUTINES
 *	===================
 */

/*
 *	ftod		- converts a float4 number to a float8 number
 */
float64
ftod(num)
	float32	num;
{
	float64	result;
	
	if (!num)
		return (float64)NULL;

	result = (float64) palloc(sizeof(float64data));

	*result = *num;
	return(result);
}


/*
 *	dtof		- converts a float8 number to a float4 number
 */
float32
dtof(num)
	float64	num;
{
	float32	result;
	
	if (!num)
		return (float32)NULL;

	result = (float32) palloc(sizeof(float32data));

	*result = *num;
	return(result);
}


/*
 *	=======================
 *	RANDOM FLOAT8 OPERATORS
 *	=======================
 */

/*
 *	dround		- returns a pointer to  ROUND(arg1)
 */
float64
dround(arg1)
	float64	arg1;
{
	float64	result;
	double tmp;

	if (!arg1)
		return (float64)NULL;

	result = (float64) palloc(sizeof(float64data));

	tmp = *arg1;
	*result = (float64data) rint(tmp);
	return(result);
}


/*
 *	dtrunc		- returns a pointer to  truncation of arg1,
 *			  arg1 >= 0 ... the greatest integer as float8 less 
 *					than or equal to arg1
 *			  arg1 < 0  ...	the greatest integer as float8 greater
 *					than or equal to arg1
 */
float64
dtrunc(arg1)
 	float64	arg1;
{
	float64	result;
	double tmp;

	if (!arg1)
		return (float64)NULL;

	result = (float64) palloc(sizeof(float64data));

	tmp = *arg1;
	if (*arg1 > 0)
		*result = (float64data) floor(tmp);
	else
		*result = (float64data) -(floor(-tmp));
	return(result);
}


/*	
 *	dsqrt		- returns a pointer to square root of arg1
 */
float64
dsqrt(arg1)
	float64	arg1;
{
	float64	result;
	double tmp;

	if (!arg1)
		return (float64)NULL;

	result = (float64) palloc(sizeof(float64data));

	tmp = *arg1;
	*result = (float64data) sqrt(tmp);
	return (result);
}


/*
 *	dcbrt		- returns a pointer to cube root of arg1	
 */
float64
dcbrt(arg1)
	float64	arg1;
{
	float64	result;
	double tmp;

	if (!arg1)
		return (float64)NULL;

	result = (float64) palloc(sizeof(float64data));

	tmp = *arg1;
	*result = (float64data) cbrt(tmp);
	return(result);
}


/*
 *	dpow		- returns a pointer to pow(arg1,arg2)
 */
float64
dpow(arg1, arg2)
	float64	arg1, arg2;
{
	float64	result;
	double tmp1, tmp2;

	if (!arg1 || !arg2)
		return (float64)NULL;

	result = (float64) palloc(sizeof(float64data));

	tmp1 = *arg1;
	tmp2 = *arg2;
	*result = (float64data) pow(tmp1, tmp2);
	return(result);
}


/*
 *	dexp		- returns a pointer to the exponential function of arg1
 */
float64
dexp(arg1)
 	float64	arg1;
{
	float64	result;
	double tmp;

	if (!arg1)
		return (float64)NULL;

	result = (float64) palloc(sizeof(float64data));

	tmp = *arg1;
	*result = (float64data) exp(tmp);
	return(result);
}


/*
 *	dlog1		- returns a pointer to the natural logarithm of arg1
 *			  ("dlog" is already a logging routine...)
 */
float64
dlog1(arg1)
	float64	arg1;
{
	float64	result;
	double tmp;

	if (!arg1)
		return (float64)NULL;

	result = (float64) palloc(sizeof(float64data));

	tmp = *arg1;
	*result = (float64data) log(tmp);
	return(result);
}


/*
 *	====================
 *	ARITHMETIC OPERATORS
 *	====================
 */

/*
 *	float48pl	- returns a pointer to arg1 + arg2
 *	float48mi	- returns a pointer to arg1 - arg2
 *	float48mul	- returns a pointer to arg1 * arg2
 *	float48div	- returns a pointer to arg1 / arg2
 */
float64 
float48pl(arg1, arg2)
	float32	arg1;
	float64	arg2;
{
	float64	result;

	if (!arg1 || !arg2)
		return (float64)NULL;

	result = (float64) palloc(sizeof(float64data));

	*result = *arg1 + *arg2;
	return(result);
}

float64
float48mi(arg1, arg2)
	float32	arg1;
	float64	arg2;
{
	float64	result;

	if (!arg1 || !arg2)
		return (float64)NULL;

	result = (float64) palloc(sizeof(float64data));

	*result = *arg1 - *arg2;
	return(result);
}

float64
float48mul(arg1, arg2)
	float32	arg1;
	float64	arg2;
{
	float64	result;

	if (!arg1 || !arg2)
		return (float64)NULL;

	result = (float64) palloc(sizeof(float64data));

	*result = *arg1 * *arg2;
	return(result);
}

float64
float48div(arg1, arg2)
	float32	arg1;
	float64	arg2;
{
	float64	result;
	
	if (!arg1 || !arg2)
		return (float64)NULL;

	result = (float64) palloc(sizeof(float64data));

	*result = *arg1 / *arg2;
	return(result);
}

/*
 *	float84pl	- returns a pointer to arg1 + arg2
 *	float84mi	- returns a pointer to arg1 - arg2
 *	float84mul	- returns a pointer to arg1 * arg2
 *	float84div	- returns a pointer to arg1 / arg2
 */
float64 
float84pl(arg1, arg2)
	float64	arg1;
	float32	arg2;
{
	float64	result;

	if (!arg1 || !arg2)
		return (float64)NULL;

	result = (float64) palloc(sizeof(float64data));

	*result = *arg1 + *arg2;
	return(result);
}

float64
float84mi(arg1, arg2)
	float64	arg1;
	float32	arg2;
{
	float64	result;

	if (!arg1 || !arg2)
		return (float64)NULL;

	result = (float64) palloc(sizeof(float64data));

	*result = *arg1 - *arg2;
	return(result);
}

float64
float84mul(arg1, arg2)
	float64	arg1;
	float32	arg2;
{
	
	float64	result;

	if (!arg1 || !arg2)
		return (float64)NULL;

	result = (float64) palloc(sizeof(float64data));

	*result = *arg1 * *arg2;
	return(result);
}

float64
float84div(arg1, arg2)
	float64	arg1;
	float32	arg2;
{
	float64	result;

	if (!arg1 || !arg2)
		return (float64)NULL;

	result = (float64) palloc(sizeof(float64data));

	*result = *arg1 / *arg2;
	return(result);
}

/*
 *	====================
 *	COMPARISON OPERATORS
 *	====================
 */

/*
 *	float48{eq,ne,lt,le,gt,ge}	- float4/float8 comparison operations
 */
long
float48eq(arg1, arg2)
	float32	arg1;
	float64	arg2;
{
	if (!arg1 || !arg2)
		return (long)NULL;

	return(*arg1 == *arg2);
}

long
float48ne(arg1, arg2)
	float32	arg1;
	float64	arg2;
{
	if (!arg1 || !arg2)
		return (long)NULL;

	return(*arg1 != *arg2);
}

long
float48lt(arg1, arg2)
	float32	arg1;
	float64	arg2;
{
	if (!arg1 || !arg2)
		return (long)NULL;

	return(*arg1 < *arg2);
}

long
float48le(arg1, arg2)
	float32	arg1;
	float64	arg2;
{
	if (!arg1 || !arg2)
		return (long)NULL;

	return(*arg1 <= *arg2);
}

long
float48gt(arg1, arg2)
	float32	arg1;
	float64	arg2;
{
	if (!arg1 || !arg2)
		return (long)NULL;

	return(*arg1 > *arg2);
}

long
float48ge(arg1, arg2)
	float32	arg1;
	float64	arg2;
{
	if (!arg1 || !arg2)
		return (long)NULL;

	return(*arg1 >= *arg2);
}

/*
 *	float84{eq,ne,lt,le,gt,ge}	- float4/float8 comparison operations
 */
long
float84eq(arg1, arg2)
	float64	arg1;
	float32	arg2;
{
	if (!arg1 || !arg2)
		return (long)NULL;

	return(*arg1 == *arg2);
}

long
float84ne(arg1, arg2)
	float64	arg1;
	float32	arg2;
{
	if (!arg1 || !arg2)
		return (long)NULL;

	return(*arg1 != *arg2);
}

long
float84lt(arg1, arg2)
	float64	arg1;
	float32	arg2;
{
	if (!arg1 || !arg2)
		return (long)NULL;

	return(*arg1 < *arg2);
}

long
float84le(arg1, arg2)
	float64	arg1;
	float32	arg2;
{
	if (!arg1 || !arg2)
		return (long)NULL;

	return(*arg1 <= *arg2);
}

long
float84gt(arg1, arg2)
	float64	arg1;
	float32	arg2;
{
	if (!arg1 || !arg2)
		return (long)NULL;

	return(*arg1 > *arg2);
}

long
float84ge(arg1, arg2)
	float64	arg1;
	float32	arg2;
{
	if (!arg1 || !arg2)
		return (long)NULL;

	return(*arg1 >= *arg2);
}

	     /* ========== PRIVATE ROUTINES ========== */

/* From "fdlibm" @ netlib.att.com */

#ifdef NEED_RINT

/* @(#)s_rint.c 5.1 93/09/24 */
/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunPro, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice 
 * is preserved.
 * ====================================================
 */

/*
 * rint(x)
 * Return x rounded to integral value according to the prevailing
 * rounding mode.
 * Method:
 *	Using floating addition.
 * Exception:
 *	Inexact flag raised if x not equal to rint(x).
 */

#ifdef __STDC__
static const double
#else
static double 
#endif
one = 1.0,
TWO52[2]={
  4.50359962737049600000e+15, /* 0x43300000, 0x00000000 */
 -4.50359962737049600000e+15, /* 0xC3300000, 0x00000000 */
};

#ifdef __STDC__
	static double rint(double x)
#else
	static double rint(x)
	double x;
#endif
{
	int i0,n0,j0,sx;
	unsigned i,i1;
	double w,t;
	n0 = (*((int *)&one)>>29)^1;
	i0 =  *(n0+(int*)&x);
	sx = (i0>>31)&1;
	i1 =  *(1-n0+(int*)&x);
	j0 = ((i0>>20)&0x7ff)-0x3ff;
	if(j0<20) {
	    if(j0<0) { 	
		if(((i0&0x7fffffff)|i1)==0) return x;
		i1 |= (i0&0x0fffff);
		i0 &= 0xfffe0000;
		i0 |= ((i1|-i1)>>12)&0x80000;
		*(n0+(int*)&x)=i0;
	        w = TWO52[sx]+x;
	        t =  w-TWO52[sx];
	        i0 = *(n0+(int*)&t);
	        *(n0+(int*)&t) = (i0&0x7fffffff)|(sx<<31);
	        return t;
	    } else {
		i = (0x000fffff)>>j0;
		if(((i0&i)|i1)==0) return x; /* x is integral */
		i>>=1;
		if(((i0&i)|i1)!=0) {
		    if(j0==19) i1 = 0x40000000; else
		    i0 = (i0&(~i))|((0x20000)>>j0);
		}
	    }
	} else if (j0>51) {
	    if(j0==0x400) return x+x;	/* inf or NaN */
	    else return x;		/* x is integral */
	} else {
	    i = ((unsigned)(0xffffffff))>>(j0-20);
	    if((i1&i)==0) return x;	/* x is integral */
	    i>>=1;
	    if((i1&i)!=0) i1 = (i1&(~i))|((0x40000000)>>(j0-20));
	}
	*(n0+(int*)&x) = i0;
	*(1-n0+(int*)&x) = i1;
	w = TWO52[sx]+x;
	return w-TWO52[sx];
}

#endif /* NEED_RINT */

#ifdef NEED_CBRT

/* @(#)s_cbrt.c 5.1 93/09/24 */
/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunPro, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice 
 * is preserved.
 * ====================================================
 *
 */

/* cbrt(x)
 * Return cube root of x
 */
#ifdef __STDC__
static const unsigned 
#else
static unsigned 
#endif
	B1 = 715094163, /* B1 = (682-0.03306235651)*2**20 */
	B2 = 696219795; /* B2 = (664-0.03306235651)*2**20 */

#ifdef __STDC__
static const double
#else
static double
#endif
C =  5.42857142857142815906e-01, /* 19/35     = 0x3FE15F15, 0xF15F15F1 */
D = -7.05306122448979611050e-01, /* -864/1225 = 0xBFE691DE, 0x2532C834 */
E =  1.41428571428571436819e+00, /* 99/70     = 0x3FF6A0EA, 0x0EA0EA0F */
F =  1.60714285714285720630e+00, /* 45/28     = 0x3FF9B6DB, 0x6DB6DB6E */
G =  3.57142857142857150787e-01; /* 5/14      = 0x3FD6DB6D, 0xB6DB6DB7 */

#ifdef __STDC__
	double cbrt(double x) 
#else
	double cbrt(x) 
	double x;
#endif
{
	double	one	= 1.0;
	int	n0,hx;
	double r,s,t=0.0,w;
	unsigned *pt = (unsigned *) &t, sign;


	n0 = ((*(int*)&one)>>29)^1;	/* index of high word */
	hx = *( n0 + (int*)&x);		/* high word of x */
	sign=hx&0x80000000; 		/* sign= sign(x) */
	hx  ^=sign;
	if(hx>=0x7ff00000) return(x+x); /* cbrt(NaN,INF) is itself */
	if((hx|*(1-n0+(int*)&x))==0) 
	    return(x);		/* cbrt(0) is itself */

	*(n0+(int*)&x) = hx;	/* x <- |x| */
    /* rough cbrt to 5 bits */
	if(hx<0x00100000) 		/* subnormal number */
	  {pt[n0]=0x43500000; 		/* set t= 2**54 */
	   t*=x; pt[n0]=pt[n0]/3+B2;
	  }
	else
	  pt[n0]=hx/3+B1;	


    /* new cbrt to 23 bits, may be implemented in single precision */
	r=t*t/x;
	s=C+r*t;
	t*=G+F/(s+E+D/s);	

    /* chopped to 20 bits and make it larger than cbrt(x) */ 
	pt[1-n0]=0; pt[n0]+=0x00000001;


    /* one step newton iteration to 53 bits with error less than 0.667 ulps */
	s=t*t;		/* t*t is exact */
	r=x/s;
	w=t+t;
	r=(r-t)/(w+r);	/* r-s is exact */
	t=t+t*r;

    /* retore the sign bit */
	pt[n0] |= sign;
	return(t);
}

#endif /* NEED_CBRT */
