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
#include "catalog/pg_relation.h"
#include "parser/parsetree.h"
#include "utils/fcache.h"
#include "utils/log.h"
#include "nodes/primnodes.h"
#include "nodes/execnodes.h"


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
#define FuncArgTypeIsDynamic(arg) \
    (ExactNodeType(arg,Var) && get_varattno((Var)arg) == InvalidAttributeNumber)

ObjectId
GetDynamicFuncArgType(arg, econtext)
    Var arg;
    ExprContext econtext;
{
    char *relname;
    int rtid;
    List rte;
    HeapTuple tup;
    Form_pg_relation pgc;
    Form_pg_type  pgt;

    Assert(ExactNodeType(arg,Var));

    rtid = CInteger(CAR(get_varid(arg)));
    relname = CString(getrelname(rtid, get_ecxt_range_table(econtext)));

    tup = SearchSysCacheTuple(TYPNAME, relname, NULL, NULL, NULL);
    if (!tup)
	elog(WARN,
	     "Lookup failed on type tuple for class %s",
	     &(pgc->relname.data[0]));

    return tup->t_oid;
}

FunctionCachePtr
init_fcache(foid, use_syscache, argList, econtext)
ObjectId foid;
Boolean use_syscache;
LispValue argList;
{
    HeapTuple        procedureTuple;
    HeapTuple        typeTuple;
    Form_pg_proc     procedureStruct;
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

    if (!use_syscache)
	elog(WARN, "what the ????, init the fcache without the catalogs?");

    procedureTuple = SearchSysCacheTuple(PROOID, (char *) foid,
                                             NULL, NULL, NULL);

    if (!HeapTupleIsValid(procedureTuple))
	elog(WARN,
	     "init_fcache: %s %d",
	     "Cache lookup failed for procedure", foid);

    /* ----------------
     *   get the return type from the procedure tuple
     * ----------------
     */
    procedureStruct = (Form_pg_proc) GETSTRUCT(procedureTuple);
    
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

    if (!HeapTupleIsValid(typeTuple))
	elog(WARN,
	     "init_fcache: %s %d",
	     "Cache lookup failed for type",
	     (procedureStruct)->prorettype);
    
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
    retval->oneResult  = ! procedureStruct->proretset;

    /*
     * If we are returning exactly one result then we have to copy
     * tuples and by reference results because we have to end the execution
     * before we return the results.  When you do this everything allocated
     * by the executor (i.e. slots and tuples) is freed.
     */
    if ((retval->language == POSTQUELlanguageId) &&
	(retval->oneResult) &&
	!(typeStruct->typbyval))
    {
	Form_pg_relation relationStruct;
	HeapTuple        relationTuple;
	TupleDescriptor  td;

	retval->funcSlot = (char *)MakeTupleTableSlot(true,
						      true,
						      NULL,
						      InvalidBuffer,
						      -1);
	relationTuple = (HeapTuple)
		SearchSysCacheTuple(RELNAME,
				    (char *)&typeStruct->typname,
				    (char *)NULL,
				    (char *)NULL,
				    (char *)NULL);

	if (relationTuple)
	{
	    relationStruct = (Form_pg_relation)GETSTRUCT(relationTuple);
	    td = CreateTemplateTupleDesc(relationStruct->relnatts);
	}
	else
	    td = CreateTemplateTupleDesc(1);

	set_ttc_tupleDescriptor(((TupleTableSlot)retval->funcSlot), td);
    }
    else
	retval->funcSlot = (char *)NULL;

    nargs = procedureStruct->pronargs;
    retval->nargs = nargs;

    if (nargs > 0)
    {
	ObjectId *argTypes;

	retval->nullVect = (bool *)palloc((retval->nargs)*sizeof(bool));

	if (retval->language == POSTQUELlanguageId)
	{
	    int  i;
	    List oneArg;
	    
	    retval->argOidVect =
		(ObjectId *)palloc(retval->nargs*sizeof(ObjectId));
	    argTypes =
		funcname_get_funcargtypes(&procedureStruct->proname.data[0]);
	    bcopy(argTypes,
		  retval->argOidVect,
		  (retval->nargs)*sizeof(ObjectId));

	    for (i=0, oneArg = CAR(argList);
		 argList;
		 i++, argList = CDR(argList))
	    {
		if (FuncArgTypeIsDynamic(oneArg))
		    retval->argOidVect[i] = GetDynamicFuncArgType(oneArg,
								  econtext);
	    }
	}
	else
	    retval->argOidVect = (ObjectId *)NULL;
    }
    else
    {
	retval->argOidVect = (ObjectId *)NULL;
	retval->nullVect = (BoolPtr)NULL;
    }

    /*
     * XXX this is the first varlena in the struct.  If the order
     *     changes for some reason this will fail.
     */
    if (procedureStruct->prolang == POSTQUELlanguageId)
    {
	retval->src = (char *) textout(&(procedureStruct->prosrc));
	retval->bin = (char *) NULL;
    }
    else
    {
#if 0
	/*
	 * I'm not sure that we even need to do this at all.
	 */
        tmp = (char *)
		SearchSysCacheGetAttribute(PROOID,
					   Anum_pg_proc_probin,
					   foid);
        retval->bin = (char *) textout(tmp);
#endif 
        retval->bin = (char *) NULL;
	retval->src = (char *) NULL;
    }

    if (retval->language != POSTQUELlanguageId)
	fmgr_info(foid, &(retval->func), &(retval->nargs));
    else
	retval->func = (func_ptr)NULL;


    return(retval);
}

void
set_fcache(node, foid, argList, econtext)

Node node;
ObjectId foid;
LispValue argList;
ExprContext econtext;

{
    Func fnode;
    Oper onode;
    FunctionCachePtr fcache, init_fcache();

    fcache = init_fcache(foid, true, argList, econtext);

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
