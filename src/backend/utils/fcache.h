
/*
 *	FCACHE.H
 *
 *	$Header$
 */

#ifndef FcacheIncluded
#define FcacheIncluded 1 /* include once only */

#include "fmgr.h"

typedef struct
{
	int      typlen;      /* length of the return type */
	int      typbyval;    /* true if return type is pass by value */
	func_ptr func;	      /* address of function to call (for c funcs) */
	ObjectId foid;	      /* oid of the function in pg_proc */
	ObjectId language;    /* oid of the language in pg_language */
	int      nargs;	      /* number of arguments */

	/* Might want to make these two arrays of size MAXFUNCARGS */

	ObjectId *argOidVect; /* oids of all the arguments */
	bool     *nullVect;   /* keep track of null arguments */

	char     *src;	      /* source code of the function */
	char     *bin;	      /* binary object code ?? */
	char     *func_state; /* fuction_state struct for execution */

	bool	 oneResult;   /* true we only want 1 result from the function */
	bool	 hasSetArg;   /* true if func is part of a nested dot expr
			       * whose argument is func returning a set ugh!
			       */

	Pointer  funcSlot;    /* if one result we need to copy it before we
			       * end execution of the function and free stuff
			       */

	char	 *setArg;     /* current argument for nested dot execution
			       * Nested dot expressions mean we have funcs
			       * whose argument is a set of tuples
			       */

	bool     istrusted;   /* trusted fn? */
}
FunctionCache, *FunctionCachePtr;

#endif FcacheIncluded
