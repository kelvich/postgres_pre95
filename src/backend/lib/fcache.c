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
#include "catalog/pg_language.h"
#include "utils/fcache.h"
#include "utils/log.h"
#include "nodes/primnodes.h"




static OID
typeid_get_relid(type_id)
        int type_id;
{
        HeapTuple     typeTuple;
        TypeTupleForm   type;
        OID             infunc;
        typeTuple = SearchSysCacheTuple(TYPOID, (char *) type_id,
                  (char *) NULL, (char *) NULL, (char *) NULL);
        type = (TypeTupleForm) GETSTRUCT(typeTuple);
        infunc = type->typrelid;
        return(infunc);
}


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
    HeapTuple        procedureTuple;
    HeapTuple        typeTuple;
    struct proc      *procedureStruct;
    Form_pg_type     typeStruct;
    FunctionCachePtr retval;
    char             *tmp;
    ObjectId         *funcname_get_funcargtypes();
    int              nargs;

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
        retval->language = procedureStruct->prolang;
        retval->func_state = (char *)NULL;
	retval->setArg     = NULL;
	retval->hasSetArg  = false;
	retval->oneResult   = false;

	/*
	 * might want to change this to just deref the pointer, there are
	 * no varlenas (currently) before this attribute.
	 */
        tmp = (char *) SearchSysCacheGetAttribute(PROOID,Anum_pg_proc_prosrc,
                                         foid);
        retval->src =  (char *)  textout(tmp);

        tmp = (char *) SearchSysCacheGetAttribute(PROOID,Anum_pg_proc_probin,
                                         foid);
        retval->bin = ( char *) textout(tmp);

	nargs = procedureStruct->pronargs;
	retval->nargs = nargs;

	if (nargs > 0)
	{
	    ObjectId *argTypes;

	    retval->nullVect = (bool *)palloc((retval->nargs)*sizeof(bool));

	    retval->argOidVect =
		(ObjectId *)palloc(retval->nargs*sizeof(ObjectId));
	    argTypes = funcname_get_funcargtypes(procedureStruct->proname);
	    bcopy(argTypes,
		  retval->argOidVect,
		  (retval->nargs)*sizeof(ObjectId));
	}
	else
	{
	    retval->argOidVect = (ObjectId *)NULL;
	    retval->nullVect = (BoolPtr)NULL;
	}
    }
    else
	elog(WARN, "what the ????, init the fcache without the catalogs?");

    if (retval->language != POSTQUELlanguageId)
	fmgr_info(foid, &(retval->func), &(retval->nargs));
    else
	retval->func = (func_ptr)NULL;


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
