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

#ifdef NOBTREE
#include "tmp/postgres.h"
RcsId("$Header$");

int32
nobtint2cmp(a, b)
    int16 a, b;
{
    return ((int32) (a - b));
}

int32
nobtint4cmp(a, b)
    int32 a, b;
{
    return (a - b);
}

int32
nobtint24cmp(a, b)
    int16 a;
    int32 b;
{
    return (((int32) a) - b);
}

int32
nobtint42cmp(a, b)
    int32 a;
    int16 b;
{
    return (a - ((int32) b));
}

int32
nobtfloat4cmp(a, b)
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
nobtfloat8cmp(a, b)
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
nobtoidcmp(a, b)
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
nobtabstimecmp(a, b)
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
nobtcharcmp(a, b)
    char a, b;
{
    return ((int32) (a - b));
}

int32
nobtchar16cmp(a, b)
    Name a;
    Name b;
{
    return (strncmp(a, b, 16));
}

int32
nobttextcmp(a, b)
    char *a, *b;
{
    return (strcmp(a, b));
}
#endif /* NOBTREE */
