/*
 * oid.c --
 *	Functions for the built-in type ObjectId.
 */

#include "tmp/postgres.h"

RcsId("$Header$");

#include "utils/palloc.h"
#include "utils/builtins.h"

#include "utils/log.h"

	    /* ========== USER I/O ROUTINES ========== */

/*
 *	oid8in		- converts "num num ..." to internal form
 *
 *	Note:
 *		Fills any nonexistent digits with NULL oids.
 */
ObjectId *
oid8in(oidString)
	char	*oidString;
{
	register ObjectId	(*result)[];
	int			nums;

	if (oidString == NULL)
		return(NULL);
	result = (ObjectId (*)[]) palloc(sizeof(ObjectId [8]));
	if ((nums = sscanf(oidString, "%d%d%d%d%d%d%d%d",
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
	return((ObjectId *) result);
}

/*
 *	oid8out	- converts internal form to "num num ..."
 */
char *
oid8out(oidArray)
	ObjectId	(*oidArray)[];
{
	register int		num;
	register ObjectId	*sp;
	register char		*rp;
	char			*result;

	if (oidArray == NULL) {
		result = (char *) palloc(2);
		result[0] = '-';
		result[1] = '\0';
		return(result);
	}

	/* assumes sign, 10 digits, ' ' */
	rp = result = (char *) palloc(8 * 12);
	sp = *oidArray;
	for (num = 8; num != 0; num--) {
		ltoa((long) *sp++, rp);
		while (*++rp != '\0')
			;
		*rp++ = ' ';
	}
	*--rp = '\0';
	return(result);
}

ObjectId
oidin(s)
	char *s;
{
	extern int32 int4in();

	return(int4in(s));
}

char *
oidout(o)
	ObjectId o;
{
	extern char *int4out();

	return(int4out(o));
}

	     /* ========== PUBLIC ROUTINES ========== */

int32	oideq(arg1, arg2) ObjectId arg1, arg2;	{ return(arg1 == arg2); }

int32	oidne(arg1, arg2) ObjectId arg1, arg2;	{ return(arg1 != arg2); }

int32
oid8eq(arg1, arg2)
         ObjectId arg1[], arg2[];
{
        return (int32)(bcmp(arg1, arg2, 8 * sizeof(ObjectId)) == 0);
}

	     /* ========== PRIVATE ROUTINES ========== */
