/*---------------------------------------------------------------
 * FILE:
 *  fcache.c
 *
 * $Header$
 *
 * Code for the 'function cache' used in Oper and Func nodes....
 *
 *---------------------------------------------------------------
 */
#include "tmp/c.h"
#include "access/htup.h"
#include "utils/catcache.h"
#include "catalog/syscache.h"
#include "catalog/pg_type.h"
#include "catalog/pg_proc.h"
#include "utils/fcache.h"
#include "utils/log.h"

/*-----------------------------------------------------------------
 *
 * Initialize the 'FunctionCache' given the PG_PROC oid.
 *
 *-----------------------------------------------------------------
 */
FunctionCache *
init_fcache(foid)
ObjectId foid;
{
	HeapTuple	procedureTuple;
	HeapTuple	typeTuple;
	struct proc	*procedureStruct;
	Form_pg_type	typeStruct;
	FunctionCache *retval;
	
	/* ----------------
	 *   get the procedure tuple corresponding to the given
	 *   functionOid.  If this fails, returnValue has been
	 *   pre-initialized to "null" so we just return it.
	 * ----------------
	 */
	procedureTuple = SearchSysCacheTuple(PROOID,
					     (char *) foid,
					     NULL,
					     NULL,
					     NULL);
    
	if (!HeapTupleIsValid(procedureTuple)) {
	    elog(WARN, "init_fcache: %s %d",
		 "Cache lookup failed for procedure",
		 foid);
	}

	/* ----------------
	 *   get the return type from the procedure tuple
	 * ----------------
	 */
	procedureStruct = (struct proc *) GETSTRUCT(procedureTuple);
	
	/* ----------------
	 *   get the type tuple corresponding to the return type
	 *   If this fails, returnValue has been pre-initialized
	 *   to "null" so we just return it.
	 * ----------------
	 */
	typeTuple = SearchSysCacheTuple(TYPOID,
					(procedureStruct)->prorettype,
					NULL,
					NULL,
					NULL);

	if (!HeapTupleIsValid(typeTuple)) {
	    elog(WARN, "init_fcache: %s %d",
		 "Cache lookup failed for type",
		 (procedureStruct)->prorettype);
	}
	
	/* ----------------
	 *   get the type length and by-value from the type tuple and
	 *   save the information in our one element cache.
	 * ----------------
	 */
	typeStruct = (Form_pg_type) GETSTRUCT(typeTuple);

    /*
	 * This must persist throughout the duration of the query.
	 */

	retval = (FunctionCache *) palloc(sizeof(FunctionCache));

	fmgr_info(foid, &(retval->func), &(retval->nargs));
	retval->typlen = (typeStruct)->typlen;
	retval->typbyval = (typeStruct)->typbyval ? true : false ;

	return(retval);
}
