/*
 *  btcompare.c -- Comparison functions for btree access method.
 *
 *	These functions are stored in pg_amproc.  For each operator class
 *	defined on btrees, they compute
 *
 *		compare(a, b):
 *			< 0 if a < b,
 *			= 0 if a == b,
 *			> 0 if a > b.
 */

#include "tmp/c.h"
#include "tmp/postgres.h"

RcsId("$Header$");

int32
btint2cmp(a, b)
    int16 a, b;
{
    return ((int32) (a - b));
}

int32
btint4cmp(a, b)
    int32 a, b;
{
    return (a - b);
}

int32
btint24cmp(a, b)
    int16 a;
    int32 b;
{
    return (((int32) a) - b);
}

int32
btint42cmp(a, b)
    int32 a;
    int16 b;
{
    return (a - ((int32) b));
}

int32
btfloat4cmp(a, b)
    float32 a, b;
{
    if (a > b)
	return (1);
    else if (a == b)
	return (0);
    else
	return (-1);
}

int32
btfloat8cmp(a, b)
    float64 a, b;
{
    if (a > b)
	return (1);
    else if (a == b)
	return (0);
    else
	return (-1);
}

int32
btoidcmp(a, b)
    ObjectId a, b;
{
    if (a > b)
	return (1);
    else if (a == b)
	return (0);
    else
	return (-1);
}

int32
btabstimecmp(a, b)
    AbsoluteTime a, b;
{
    if (AbsoluteTimeIsBefore(a, b))
	return (1);
    else if (AbsoluteTimeIsBefore(b, a))
	return (-1);
    else
	return (0);
}

int32
btcharcmp(a, b)
    char a, b;
{
    return ((int32) (a - b));
}

int32
btchar16cmp(a, b)
    Name a;
    Name b;
{
    return (strncmp(a, b, 16));
}

int32
bttextcmp(a, b)
    struct varlena *a, *b;
{
    char *ap, *bp;
    int len;

    ap = VARDATA(a);
    bp = VARDATA(b);

    /* len is the length of the shorter of the two strings */
    if ((len = VARSIZE(a)) > VARSIZE(b))
	len = VARSIZE(b);

    /*
     *  If the two strings differ in the first len bytes, or if they're
     *  the same in the first len bytes and they're both len bytes long,
     *  we're done.
     */

    if ((res = strncmp(ap, bp, len)) != 0 || VARSIZE(a) == VARSIZE(b))
	return (res);

    /*
     *  The two strings are the same in the first len bytes, and they
     *  are of different lengths.
     */

    if (VARSIZE(a) < VARSIZE(b))
	return (-1);
    else
	return (1);
}
