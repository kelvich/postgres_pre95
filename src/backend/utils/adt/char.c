/*
 * char.c --
 * 	Functions for the built-in type "char".
 * 	Functions for the built-in type "cid".
 * 	Functions for the built-in type "char2".
 * 	Functions for the built-in type "char4".
 * 	Functions for the built-in type "char8".
 * 	Functions for the built-in type "char16".
 */

#include <strings.h>

#include "tmp/postgres.h"

RcsId("$Header$");

#include "utils/palloc.h"

	    /* ========== USER I/O ROUTINES ========== */


/*
 *	charin		- converts "x" to 'x'
 */
int32
charin(ch)
	char	*ch;
{
	if (ch == NULL)
		return((int32) NULL);
	return((int32) *ch);
}

/*
 *	charout		- converts 'x' to "x"
 */
char *
charout(ch)
	int32	ch;
{
	char	*result = (char *) palloc(2);

	result[0] = (char) ch;
	result[1] = '\0';
	return(result);
}

/*
 *	cidin	- converts "..." to internal representation.
 *
 * 	NOTE: we must not use 'charin' because cid might be a non
 *	printable character...
 */
int32
cidin(s)
char *s;
{
    CommandId c;

    if (s==NULL)
	c = 0;
    else
	c = atoi(s);

    return((int32)c);
}

/*
 *	cidout	- converts a cid to "..."
 *
 * 	NOTE: we must no use 'charout' because cid might be a non
 *	printable character...
 */
char *
cidout(c)
int32 c;
{
    char *result;
    CommandId c2;

    /*
     * cid is a number between 0 .. 2^16-1, therefore we need at most
     * 4 chars for the string (6 digits + '\0')
     * NOTE: print it as an UNSIGNED int!
     */
    result = palloc(8);
    c2 = (CommandId)c;
    sprintf(result, "%u", (unsigned)(c2));
    return(result);
}

/*
 *	char16in	- converts "..." to internal reprsentation
 *
 *	Note:
 *		Currently if strlen(s) < 14, the extra chars are nulls
 */
char *
char16in(s)
	char	*s;
{
	char	*result;
	int      i;
	if (s == NULL)
		return(NULL);
	result = (char *) palloc(16);
	strncpy(result, s, 16);
	/* null out the extra chars */
	for (i = strlen(s) + 1; i < 16; i++)
	  result[i] = '\0';
	return(result);
}

/*
 *	char16out	- converts internal reprsentation to "..."
 */
char *
char16out(s)
	char	*s;
{
	char	*result = (char *) palloc(17);

	if (s == NULL) {
		result[0] = '-';
		result[1] = '\0';
	} else {
		strncpy(result, s, 16);
		result[16] = '\0';
	}
	return(result);
}


	     /* ========== PUBLIC ROUTINES ========== */

int32 chareq(arg1, arg2)	int8 arg1, arg2; { return(arg1 == arg2); }
int32 charne(arg1, arg2)	int8 arg1, arg2; { return(arg1 != arg2); }
int32 charlt(arg1, arg2)	int8 arg1, arg2; { return(arg1 < arg2); }
int32 charle(arg1, arg2)	int8 arg1, arg2; { return(arg1 <= arg2); }
int32 chargt(arg1, arg2)	int8 arg1, arg2; { return(arg1 > arg2); }
int32 charge(arg1, arg2)	int8 arg1, arg2; { return(arg1 >= arg2); }
int8 charpl(arg1, arg2)         int8 arg1, arg2; { return(arg1 + arg2); }
int8 charmi(arg1, arg2)	        int8 arg1, arg2; { return(arg1 - arg2); }
int8 charmul(arg1, arg2)	int8 arg1, arg2; { return(arg1 * arg2); }
int8 chardiv(arg1, arg2)	int8 arg1, arg2; { return(arg1 / arg2); }

int32 cideq(arg1, arg2)		int8 arg1, arg2; { return(arg1 == arg2); }

/*
 *	char16eq	- returns 1 iff arguments are equal
 *	char16ne	- returns 1 iff arguments are not equal
 *
 *	BUGS:
 *		Assumes that "xy\0\0a" should be equal to "xy\0b".
 *		If not, can do the comparison backwards for efficiency.
 *
 *	char16lt	- returns 1 iff a < b
 *	char16le	- returns 1 iff a <= b
 *	char16gt	- returns 1 iff a < b
 *	char16ge	- returns 1 iff a <= b
 *
 */
int32
char16eq(arg1, arg2)
    char	*arg1, *arg2;
{

    if (arg1 == NULL || arg2 == NULL)
	return((int32) NULL);
    return((int32) (strncmp(arg1, arg2, 16) == 0));
}

int32
char16ne(arg1, arg2)
    char	*arg1, *arg2;
{
    return((int32) !char16eq(arg1, arg2));
}

int32
char16lt(arg1, arg2)
    char	*arg1, *arg2;
{
    if (arg1 == NULL || arg2 == NULL)
	return((int32) NULL);

    return((int32) (strncmp(arg1, arg2, 16) < 0));
}

int32
char16le(arg1, arg2)
    char	*arg1, *arg2;
{
    if (arg1 == NULL || arg2 == NULL)
	return((int32) NULL);

    return((int32) (strncmp(arg1, arg2, 16) <= 0));
}

int32
char16gt(arg1, arg2)
    char	*arg1, *arg2;
{
    if (arg1 == NULL || arg2 == NULL)
	return((int32) NULL);

    return((int32) (strncmp(arg1, arg2, 16) > 0));
}

int32
char16ge(arg1, arg2)
    char	*arg1, *arg2;
{
    if (arg1 == NULL || arg2 == NULL)
	return((int32) NULL);

    return((int32) (strncmp(arg1, arg2, 16) >= 0));
}


/* ============================== char2 ============================== */
uint16
char2in(s)
	char	*s;
{
	char	*p;
	int      i;
	uint16	res;

	if (s == NULL)
		return(NULL);

	p = (char *) &res;
	for (i = 0; i < 2; i++) {
	    if (*s)
		*p++ = *s++;
	    else
		*p++ = '\0';
	}

	return(res);
}

char *
char2out(s)
	uint16 s;
{
	char	*result = (char *) palloc(3);
	char	*p;

	p = (char *) &s;

	strncpy(result, p, 2);
	result[2] = '\0';

	return(result);
}

int32
char2eq(a, b)
    uint16 a, b;
{
    char *arg1, *arg2;

    arg1 = (char *) &a;
    arg2 = (char *) &b;

    if (*arg1 != *arg2)
	return ((int32) NULL);
    if (*arg1 && (*(++arg1) != *(++arg2)))
	return ((int32) NULL);
    return((int32) 1);
}

int32
char2ne(a, b)
    uint16 a, b;
{
    return((int32) !char2eq(a, b));
}

int32
char2lt(a, b)
    uint16 a, b;
{
    char *arg1, *arg2;

    arg1 = (char *) &a;
    arg2 = (char *) &b;

    if (*arg1 < *arg2) {
	return ((int32) 1);
    } else if (*arg1 == *arg2) {
	if (*arg1 != '\0' && (*(++arg1) < *(++arg2)))
	    return ((int32) 1);
    }

    return ((int32) NULL);
}

int32
char2le(a, b)
    uint16 a, b;
{
    char *arg1, *arg2;

    arg1 = (char *) &a;
    arg2 = (char *) &b;

    if (*arg1 < *arg2) {
	return ((int32) 1);
    } else if (*arg1 == *arg2) {
	if (*arg1 == '\0' || (*(++arg1) <= *(++arg2)))
	    return ((int32) 1);
    }

    return ((int32) NULL);
}

int32
char2gt(a, b)
    uint16 a, b;
{
    char *arg1, *arg2;

    arg1 = (char *) &a;
    arg2 = (char *) &b;

    if (*arg1 > *arg2) {
	return ((int32) 1);
    } else if (*arg1 == *arg2) {
	if (*arg2 != '\0' && (*(++arg1) > *(++arg2)))
	    return ((int32) 1);
    }

    return ((int32) NULL);
}

int32
char2ge(a, b)
    uint16 a, b;
{
    char *arg1, *arg2;

    arg1 = (char *) &a;
    arg2 = (char *) &b;

    if (*arg1 > *arg2) {
	return ((int32) 1);
    } else if (*arg1 == *arg2) {
	if (*arg1 == '\0' || (*(++arg1) >= *(++arg2)))
	    return ((int32) 1);
    }

    return ((int32) NULL);
}

int32
char2cmp(a, b)
    uint16 a, b;
{
    return (strncmp((char *) &a, (char *) &b, 2));
}

/* ============================== char8 ============================== */
uint32
char4in(s)
	char	*s;
{
	char	*p;
	int      i;
	uint32	res;

	if (s == NULL)
		return(NULL);

	p = (char *) &res;
	for (i = 0; i < 4; i++) {
	    if (*s)
		*p++ = *s++;
	    else
		*p++ = '\0';
	}

	return(res);
}

char *
char4out(s)
	uint32 s;
{
	char	*result = (char *) palloc(5);
	char	*p;

	p = (char *) &s;

	strncpy(result, p, 4);
	result[4] = '\0';

	return(result);
}

int32
char4eq(a, b)
    uint32 a, b;
{
    if (strncmp((char *) &a, (char *) &b, 4) == 0)
	return((int32) 1);

    return ((int32) NULL);
}

int32
char4ne(a, b)
    uint32 a, b;
{
    return((int32) !char4eq(a, b));
}

int32
char4lt(a, b)
    uint32 a, b;
{
    if (strncmp((char *) &a, (char *) &b, 4) < 0)
	return ((int32) 1);
    return ((int32) NULL);
}

int32
char4le(a, b)
    uint32 a, b;
{
    if (strncmp((char *) &a, (char *) &b, 4) <= 0)
	return ((int32) 1);

    return ((int32) NULL);
}

int32
char4gt(a, b)
    uint32 a, b;
{
    if (strncmp((char *) &a, (char *) &b, 4) > 0)
	return ((int32) 1);

    return ((int32) NULL);
}

int32
char4ge(a, b)
    uint32 a, b;
{
    if (strncmp((char *) &a, (char *) &b, 4) >= 0)
	return ((int32) 1);

    return ((int32) NULL);
}

int32
char4cmp(a, b)
    uint32 a, b;
{
    return (strncmp((char *) &a, (char *) &b, 4));
}

/* ============================== char8 ============================== */
char *
char8in(s)
	char	*s;
{
	char	*result, *p;
	int      i;

	if (s == NULL)
		return(NULL);

	p = result = (char *) palloc(8);
	for (i = 0; i < 8; i++) {
	    if (*s)
		*p++ = *s++;
	    else
		*p++ = '\0';
	}

	return(result);
}

char *
char8out(s)
	char	*s;
{
	char	*result = (char *) palloc(9);

	if (s == NULL) {
		result[0] = '-';
		result[1] = '\0';
	} else {
		strncpy(result, s, 8);
		result[8] = '\0';
	}
	return(result);
}

int32
char8eq(arg1, arg2)
    char *arg1, *arg2;
{

    if (arg1 == NULL || arg2 == NULL)
	return((int32) NULL);
    if (strncmp(arg1, arg2, 8) != 0)
	return ((int32) NULL);
    return((int32) 1);
}

int32
char8ne(arg1, arg2)
    char	*arg1, *arg2;
{
    return((int32) !char8eq(arg1, arg2));
}

int32
char8lt(arg1, arg2)
    char	*arg1, *arg2;
{
    if (arg1 == NULL || arg2 == NULL)
	return((int32) NULL);

    if (strncmp(arg1, arg2, 8) < 0)
	return ((int32) 1);

    return ((int32) NULL);
}

int32
char8le(arg1, arg2)
    char	*arg1, *arg2;
{
    if (arg1 == NULL || arg2 == NULL)
	return((int32) NULL);

    if (strncmp(arg1, arg2, 8) < 0)
	return ((int32) 1);

    return ((int32) NULL);
}

int32
char8gt(arg1, arg2)
    char	*arg1, *arg2;
{
    if (arg1 == NULL || arg2 == NULL)
	return((int32) NULL);

    if (strncmp(arg1, arg2, 8) > 0)
	return ((int32) 1);

    return ((int32) NULL);
}

int32
char8ge(arg1, arg2)
    char	*arg1, *arg2;
{
    if (arg1 == NULL || arg2 == NULL)
	return((int32) NULL);

    if (strncmp(arg1, arg2, 8) >= 0)
	return ((int32) 1);

    return ((int32) NULL);
}

int32
char8cmp(arg1, arg2)
    char *arg1, *arg2;
{
    return (strncmp(arg1, arg2, 8));
}
