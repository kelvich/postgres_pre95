
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
	int      typlen;      /* length of the return type */
	int      typbyval;    /* true if return type is pass by value */
	func_ptr func;	      /* address of function to call (for c funcs) */
	ObjectId foid;	      /* oid of the function in pg_proc */
	ObjectId language;    /* oid of the language in pg_language */
	int      nargs;	      /* number of arguments */
	ObjectId *argOidVect; /* oids of all the arguments */
	bool     *nullVect;   /* keep track of null arguments */
	char     *src;	      /* source code of the function */
	char     *bin;	      /* binary object code ?? */
	char     *func_state; /* fuction_state struct for execution */
}
FunctionCache, *FunctionCachePtr;

#endif FcacheIncluded
