
/*
 *	FCACHE.H
 *
 *	$Header$
 */

#ifndef FcacheIncluded
#define FcacheIncluded 1 /* include once only */

#include "utils/fmgr.h"

typedef struct
{
	func_ptr func;
	int typlen;
	int nargs;
	int typbyval;
	ObjectId foid;
}
FunctionCache, *FunctionCachePtr;

#endif FcacheIncluded
