/*
 * char.c --
 * 	Functions for the built-in type "char".
 * 	Functions for the built-in type "char16".
 */

#include <strings.h>

#include "tmp/c.h"

RcsId("$Header$");

#include "utils/palloc.h"

char *PG_username;

char *
pg_username()
{
    return(PG_username);
}

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
	char	*result = palloc(2);

	result[0] = (char) ch;
	result[1] = '\0';
	return(result);
}

/*
 *	char16in	- converts "..." to internal reprsentation
 *
 *	Note:
 *		Currently if strlen(s) < 14, the extra chars are garbage.
 */
char *
char16in(s)
	char	*s;
{
	char	*result;

	if (s == NULL)
		return(NULL);
	result = palloc(16);
	strncpy(result, s, 16);
	return(result);
}

/*
 *	char16out	- converts internal reprsentation to "..."
 */
char *
char16out(s)
	char	*s;
{
	char	*result = palloc(17);

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
int32 charpl(arg1, arg2)	int8 arg1, arg2; { return(arg1 + arg2); }
int32 charmi(arg1, arg2)	int8 arg1, arg2; { return(arg1 - arg2); }
int32 charmul(arg1, arg2)	int8 arg1, arg2; { return(arg1 * arg2); }
int32 chardiv(arg1, arg2)	int8 arg1, arg2; { return(arg1 / arg2); }


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
