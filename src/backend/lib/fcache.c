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
#include "nodes/primnodes.h"

/*-----------------------------------------------------------------
 *
 * Initialize the 'FunctionCache' given the PG_PROC oid.
 *
 *
 * NOTE:  This function can be called when the system cache is being
 *        initialized.  Therefore, use_syscache should ONLY be true
 *        when the function return type is interesting (ie: set_fcache).
 *-----------------------------------------------------------------
 */

FunctionCachePtr
init_fcache(foid, use_syscache)
ObjectId foid;
Boolean use_syscache;
{
    HeapTuple    procedureTuple;
    HeapTuple    typeTuple;
    struct proc    *procedureStruct;
    Form_pg_type    typeStruct;
    FunctionCachePtr retval;
    
    /* ----------------
     *   get the procedure tuple corresponding to the given
     *   functionOid.  If this fails, returnValue has been
     *   pre-initialized to "null" so we just return it.
     * ----------------
     */
    retval = (FunctionCachePtr) palloc(sizeof(FunctionCache));

    if (use_syscache)
    {
        procedureTuple = SearchSysCacheTuple(PROOID, (char *) foid,
                                             NULL, NULL, NULL);

        if (!HeapTupleIsValid(procedureTuple)) {
            elog(WARN, "init_fcache: %s %d",
                 "Cache lookup failed for procedure", foid);
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
        retval->typlen = (typeStruct)->typlen;
        retval->typbyval = (typeStruct)->typbyval ? true : false ;
	retval->foid = foid;
    }
    else
    {
        /* default to int4 */

        retval->typlen = sizeof(int4);
        retval->typbyval = true;
	retval->foid = foid;
    }

    fmgr_info(foid, &(retval->func), &(retval->nargs));

    return(retval);
}

void
set_fcache(node, foid)

Node node;
ObjectId foid;

{
    Func fnode;
    Oper onode;
    FunctionCachePtr fcache, init_fcache();

    fcache = init_fcache(foid, true);

    if (IsA(node,Oper))
    {
        onode = (Oper) node;
        onode->op_fcache = fcache;
    }
    else if (IsA(node,Func))
    {
        fnode = (Func) node;
        fnode->func_fcache = fcache;
    }
    else
    {
        elog(WARN, "init_fcache: node must be Oper or Func!");
    }
}
