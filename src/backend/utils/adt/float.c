/*
 * float.c --
 *      Functions for the built-in floating-point types.
 *
 *	Basic float4 ops:
 * 	 float4in, float4inAd, float4out, float4outAd, float4abs, float4um
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
#include <math.h>

#include "tmp/postgres.h"
#include "utils/fmgr.h"
#include "utils/builtins.h"	/* for ftod() prototype */
#include "utils/log.h"

RcsId("$Header$");


#define FORMAT 		'e'	/* use "E" output format as standard format */
#define	MAXFLOATWIDTH 	12	/* "n.nnnnnE+nn\0" format */
#define FLOATPRECISION	5  	/* number of significant digits */
				/*    after decimal point */
#define MAXDOUBLEWIDTH	24	/* "n.nnnnnnnnnnnnnnnnnE+nn\0" format */
#define DOUBLEPRECISION	17	/* number of significant digits */
				/*    after decimal point */

extern double	atof();
extern double   cbrt();


char *float8outAd ARGS((float64 num , int precision , char format ));
float64 float8inAd ARGS((char *num ));
char *float4outAd ARGS((float32 num , int precision , char format ));
float32 float4inAd ARGS((char *num ));

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
	
	*result = (float32data) atof(num);
	return(result);
}


/*
 *	float4inAd	- advanced float4in, converts "num" to float4,
 *			  extended syntax:
 *			 {<sp>}[+|-]{<sp>}{<digit>}[.{digit}]{<sp>}[<exp>]
 *			 where <exp> is "e" or "E" followed by an integer, 
 *			 <sp> is a space character, <digit> is zero through 
 *			 nine, [] is zero or one, and {} is zero or more.
 *
 *			 uses atof1, code of ingres; file atof1.c
 */
float32
float4inAd(num)
	char	*num;
{
	float64data	val;
	float32		result = (float32) palloc(sizeof(float32data));
	int	status;
	
	status=atof1(num,&val);
	if (status==0) {
		*result = val;
		return(result);
	}
	if (status==-1) 
		printf("\tSyntax error\n");
	if (status==1) 
		printf("\tOverflow\n");
	*result = val;
	return(result);
}


/*
 *	float4out	- converts a float4 number to a string
 *			  using a standard output format
 */
char *
float4out(num)
	float32	num;
{
	
	char	*ascii = (char *)palloc(MAXFLOATWIDTH);	
	
	if (!num)
		return strcpy(ascii, "(null)");

	ftoa((double) *num, ascii, MAXFLOATWIDTH, FLOATPRECISION, FORMAT);
	return(ascii);
}


/*	float4outAd	- converts a float4 number to a string
 *			  allowing to specify the output format
 *
 *	'format' can be:
 *		e or E:  "E" format output
 *		f or F:  "F" format output
 *		g or G:  "F" format output if it will fit, otherwise
 *			 use "E" format.
 *		n or N:  same as G, but decimal points will not always
 *			 be aligned.
 *
 *	'precision' is the number of digits after decimal point.
 */
char *
float4outAd(num, precision, format)
	float32	num;
	int	precision;
	char	format;
{
	char	*ascii = (char *)palloc(MAXFLOATWIDTH);	

	if (!num)
		return strcpy(ascii, "(null)");

	ftoa(*num, ascii, MAXFLOATWIDTH, precision, format);
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
	
	*result = (float64data) atof(num);
	return(result);
}


/*
 *	float8inAd	- advanced float8in, converts "num" to float8,
 *			  extended syntax:
 *			 {<sp>}[+|-]{<sp>}{<digit>}[.{digit}]{<sp>}[<exp>]
 *			 where <exp> is "e" or "E" followed by an integer, 
 *			 <sp> is a space character, <digit> is zero through 
 *			 nine, [] is zero or one, and {} is zero or more.
 *
 */
float64
float8inAd(num)
	char	*num;
{
	float64data	val;
	float64	result = (float64) palloc(sizeof(float64data));
	int	status;
	
	status = atof1(num,&val);
	if (status==0) {
		*result = val;
		return(result);
	}
	if (status == -1)
		elog(WARN, "Bad float8 constant");
	if (status == 1)
		elog(WARN, "float8 overflow");
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
	char	*ascii = (char *)palloc(MAXDOUBLEWIDTH);

	if (!num)
		return strcpy(ascii, "(null)");

	ftoa(*num, ascii, MAXDOUBLEWIDTH, DOUBLEPRECISION, FORMAT);
	return(ascii);
}


/*
 *	float8outAd	- advanced float8out, converts float8 number to a
 *			  string allowing to specify the output format
 *
 *	'format' can be:
 *		e or E:  "E" format output
 *		f or F:  "F" format output
 *		g or G:  "F" format output if it will fit, otherwise
 *			 use "E" format.
 *		n or N:  same as G, but decimal points will not always
 *			 be aligned.
 *
 *	'precision' is the number of digits after decimal point.
 */
char *
float8outAd(num, precision, format)
	float64	num;
	int	precision;
	char	format;
{
	char	*ascii = (char *)palloc(MAXDOUBLEWIDTH);
	
	if (!num)
		return strcpy(ascii, "(null)");

	ftoa(*num, ascii, MAXDOUBLEWIDTH, precision, format);
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
	
	if (!arg1)
		return (float32)NULL;

	dblarg1 = ftod(arg1);
	result = (float32) palloc(sizeof(float32data));

	*result = (float32data) fabs(*dblarg1);
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
	
	if (!arg1)
		return (float64)NULL;

	result = (float64) palloc(sizeof(float64data));

	*result = fabs(*arg1);
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
#ifdef sun
	float64	result = (float64) palloc(sizeof(float64data));

	*result = (float64data) rint((double) *arg1);
	return(result);
#else
	elog(WARN, "dround: not implemented yet for non-sun machines");
#endif
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

	if (!arg1)
		return (float64)NULL;

	result = (float64) palloc(sizeof(float64data));

	if (*arg1 > 0)
		*result = (float64data) floor((double) *arg1);
	else
		*result = (float64data) -(floor((double) -(*arg1)));
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

	if (!arg1)
		return (float64)NULL;

	result = (float64) palloc(sizeof(float64data));

	*result = (float64data) sqrt((double) *arg1);
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

	if (!arg1)
		return (float64)NULL;

	result = (float64) palloc(sizeof(float64data));

	*result = (float64data) cbrt((double) *arg1);
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

	if (!arg1 || !arg2)
		return (float64)NULL;

	result = (float64) palloc(sizeof(float64data));

	*result = (float64data) pow((double) *arg1, (double) *arg2);
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

	if (!arg1)
		return (float64)NULL;

	result = (float64) palloc(sizeof(float64data));

	*result = (float64data) exp((double) *arg1);
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

	if (!arg1)
		return (float64)NULL;

	result = (float64) palloc(sizeof(float64data));

	*result = (float64data) log((double) *arg1);
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

			     /* (none) */
