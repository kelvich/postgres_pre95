/*
 * int4_abs.c -- absolute value comparison functions
 *               for int4 data
 *
 * example from chapter 13
 *
 * $Header$
 */

#include "tmp/c.h"

#define ABS(a) ((a < 0) ? -a : a)

/* routines to implement operators */

bool int4_abs_lt(a, b) int32 a, b; { return(ABS(a) < ABS(b)); }

bool int4_abs_le(a, b) int32 a, b; { return(ABS(a) <= ABS(b)); }

bool int4_abs_eq(a, b) int32 a, b; { return(ABS(a) == ABS(b)); }

bool int4_abs_ge(a, b) int32 a, b; { return(ABS(a) >= ABS(b)); }

bool int4_abs_gt(a, b) int32 a, b; { return(ABS(a) > ABS(b)); }

/* support (signed comparison) routine */

int int4_abs_cmp(a, b) int32 a, b; { return(ABS(a) - ABS(b)); }
