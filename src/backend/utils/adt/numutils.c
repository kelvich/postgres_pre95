
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
 * numutils.c --
 * 	utility functions for I/O of built-in numeric types.
 *
 *	integer:		itoa, ltoa
 *	floating point:		ftoa, atof1
 */

#include "c.h"
#include "postgres.h"

RcsId("$Header$");

/*
 *	itoa		- converts a short int to its string represention
 *
 *	Note:
 *		Based on ~ingres/source/gutil/atoi.c
 */
itoa(i, a)
	register int	i;
	register char	*a;
{
	register char	*t;
	char		temp[6];

	if (i < 0) {
		i = -i;
		*a++ = '-';
	}
	t = temp + 5;
	*t-- = '\0';
	do {
		*t-- = i % 10 + '0';
		i /= 10;
	} while (i != 0);
	do {
		*a++ = *++t;
	} while (*t != '\0');
}

/*
 *	ltoa		- converts a long int to its string represention
 *
 *	Note:
 *		Based on ~ingres/source/gutil/atoi.c
 */
ltoa(l, a)
	register long	l;
	register char	*a;
{
	register	char	*t;
	char			temp[11];

	if (l < 0) {
		l = -l;
		*a++ = '-';
	}
	t = temp + 10;
	*t-- = '\0';
	do {
		*t-- = l % 10 + '0';
		l /= 10;
	} while (l != 0);
	do {
		*a++ = *++t;
	} while (*t != '\0');
}

/*
 **  ftoa	- FLOATING POINT TO ASCII CONVERSION
 **
 **	CODE derived from ingres, ~ingres/source/gutil/ftoa.c
 **
 **	'Value' is converted to an ascii character string and stored
 **	into 'ascii'.  Ascii should have room for at least 'width' + 1
 **	characters.  'Width' is the width of the output field (max).
 **	'Prec' is the number of characters to put after the decimal
 **	point.  The format of the output string is controlled by
 **	'format'.
 **
 **	'Format' can be:
 **		e or E: "E" format output
 **		f or F:  "F" format output
 **		g or G:  "F" format output if it will fit, otherwise
 **			use "E" format.
 **		n or N:  same as G, but decimal points will not always
 **			be aligned.
 **
 **	If 'format' is upper case, the "E" comes out in upper case;
 **	otherwise it comes out in lower case.
 **
 **	When the field width is not big enough, it fills the field with
 **	stars ("*****") and returns zero.  Normal return is the width
 **	of the output field (sometimes shorter than 'width').
 */
ftoa(value, ascii, width, prec1, format)
	double	value;
	char	*ascii;
	int	width;
	int	prec1;
	char	format;
{
	auto int	expon;
	auto int	sign;
	register int	avail;
	register char	*a;
	register char	*p;
	char		mode;
	int		lowercase;
	int		prec;
	extern char	*ecvt(), *fcvt();
	
	prec = prec1;
	mode = format;
	lowercase = 'a' - 'A';
	if (mode >= 'a')
		mode -= 'a' - 'A';
	else
		lowercase = 0;
	
	if (mode != 'E') {
		/* try 'F' style output */
		p = fcvt(value, prec, &expon, &sign);
		avail = width;
		a = ascii;
		
		/* output sign */
		if (sign) {
			avail--;
			*a++ = '-';
		}
		
		/* output '0' before the decimal point */
		if (expon <= 0) {
			*a++ = '0';
			avail--;
		}
		
		/* compute space length left after dec pt and fraction */
		avail -= prec + 1;
		if (mode == 'G')
			avail -= 4;
		
		if (avail >= expon) {
			
			/* it fits.  output */
			while (expon > 0) {
				/* output left of dp */
				expon--;
				if (*p) {
					*a++ = *p++;
				} else
					*a++ = '0';
			}
			
			/* output fraction (right of dec pt) */
			avail = expon;
			goto frac_out;
		}
		/* won't fit; let's hope for G format */
	}
	
	if (mode != 'F') {
		/* try to do E style output */
		p = ecvt(value, prec + 1, &expon, &sign);
		avail = width - 5;
		a = ascii;
		
		/* output the sign */
		if (sign) {
			*a++ = '-';
			avail--;
		}
	}
	
	/* check for field too small */
	if (mode == 'F' || avail < prec) {
		/* sorry joker, you lose */
		a = ascii;
		for (avail = width; avail > 0; avail--)
			*a++ = '*';
		*a = 0;
		return (0);
	}
	
	/* it fits; output the number */
	mode = 'E';
	
	/* output the LHS single digit */
	*a++ = *p++;
	expon--;
	
	/* output the rhs */
	avail = 1;
	
 frac_out:
	*a++ = '.';
	while (prec > 0) {
		prec--;
		if (avail < 0) {
			avail++;
			*a++ = '0';
		} else {
			if (*p)
				*a++ = *p++;
			else
				*a++ = '0';
		}
	}
	
	/* output the exponent */
	if (mode == 'E') {
		*a++ = 'E' + lowercase;
		if (expon < 0) {
			*a++ = '-';
			expon = -expon;
		} else
			*a++ = '+';
		*a++ = (expon / 10) % 10 + '0';
		*a++ = expon % 10 + '0';
	}
	
	/* output spaces on the end in G format */
	if (mode == 'G') {
		*a++ = ' ';
		*a++ = ' ';
		*a++ = ' ';
		*a++ = ' ';
	}
	
	/* finally, we can return */
	*a = 0;
	avail = a - ascii;
	return (avail);
}

/*
 **   atof1	- ASCII TO FLOATING CONVERSION
 **
 **	CODE derived from ~ingres/source/gutil/atof.c
 **
 **	Converts the string 'str' to floating point and stores the
 **	result into the cell pointed to by 'val'.
 **
 **	The syntax which it accepts is pretty much what you would
 **	expect.  Basically, it is:
 **		{<sp>} [+|-] {<sp>} {<digit>} [.{digit}] {<sp>} [<exp>]
 **	where <exp> is "e" or "E" followed by an integer, <sp> is a
 **	space character, <digit> is zero through nine, [] is zero or
 **	one, and {} is zero or more.
 **
 **	Parameters:
 **		str -- string to convert.
 **		val -- pointer to place to put the result (which
 **			must be type double).
 **
 **	Returns:
 **		zero -- ok.
 **		-1 -- syntax error.
 **		+1 -- overflow (not implemented).
 **
 **	Side Effects:
 **		clobbers *val.
 */
atof1(str, val)
	char	*str;
	double	*val;
{
	register char	*p;
	double		v;
	double		fact;
	int		minus;
	register char	c;
	int		expon;
	register int	gotmant;
	
	v = 0.0;
	p = str;
	minus = 0;
	
	/* skip leading blanks */
	while (c = *p) {
		if (c != ' ')
			break;
		p++;
	}
	
	/* handle possible sign */
	switch (c) {
	case '-':
		minus++;
		
	case '+':
		p++;
	}
	
	/* skip blanks after sign */
	while (c = *p) {
		if (c != ' ')
			break;
		p++;
	}
	
	/* start collecting the number to the decimal point */
	gotmant = 0;
	for (;;) {
		c = *p;
		if (c < '0' || c > '9')
			break;
		v = v * 10.0 + (c - '0');
		gotmant++;
		p++;
	}
	
	/* check for fractional part */
	if (c == '.') {
		fact = 1.0;
		for (;;) {
			c = *++p;
			if (c < '0' || c > '9')
				break;
			fact *= 0.1;
			v += (c - '0') * fact;
			gotmant++;
		}
	}
	
	/* skip blanks before possible exponent */
	while (c = *p) {
		if (c != ' ')
			break;
		p++;
	}
	
	/* test for exponent */
	if (c == 'e' || c == 'E') {
		p++;
		expon = atoi(p);
		if (!gotmant)
			v = 1.0;
		fact = expon;
		v *= pow(10.0, fact);
	} else {
		/* if no exponent, then nothing */
		if (c != 0)
			return (-1);
	}
	
	/* store the result and exit */
	if (minus)
		v = -v;
	*val = v;
	return (0);
}
