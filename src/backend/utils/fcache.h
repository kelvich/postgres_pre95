
/*
 *	FCACHE.H
 *
 *	$Header$
 */

#ifndef FcacheIncluded
#define FcacheIncluded 1 /* include once only */

typedef char *  ((*func_ptr)());

typedef struct
{
	func_ptr func;
	int typlen;
	int nargs;
	int typbyval;
}
FunctionCache, *FunctionCachePtr;

#endif FcacheIncluded
