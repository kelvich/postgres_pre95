/* ------------------------------------------------
 *   FILE
 *     functions.c
 *
 *   DESCRIPTION
 *	Routines to handle functions called from the executor
 *      Putting this stuff in fmgr makes the postmaster a mess....
 *
 *
 *   IDENTIFICATION
 *   	$Header$
 * ------------------------------------------------
 */
#include "tmp/postgres.h"

#include "parser/parsetree.h"
#include "parser/parse.h" /* for NOT used in macros in ExecEvalExpr */
#include "nodes/primnodes.h"
#include "nodes/relation.h"
#include "nodes/execnodes.h"
#include "planner/keys.h"
#include "tcop/pquery.h"
#include "utils/params.h"
#include "utils/fmgr.h"
#include "utils/fcache.h"
#include "utils/log.h"
#include "catalog/pg_proc.h"
#include "catalog/syscache.h"
#include "catalog/pg_language.h"
#include "access/heapam.h"
#include "executor/executor.h"

#undef new

typedef enum {F_EXEC_START, F_EXEC_RUN, F_EXEC_DONE} ExecStatus;

typedef struct local_es {
    List qd;
    EState estate;
    struct local_es *next;
    ExecStatus status;
} execution_state;

Datum
ProjectAttribute(TD, tlist, tup,isnullP)
     TupleDescriptor TD;
     List tlist;
     HeapTuple tup;
     Boolean *isnullP;
{
    Datum val,valueP;
    Var  attrVar = (Var)CADR(tlist);
    AttributeNumber attrno = get_varattno(attrVar);

    
    val = HeapTupleGetAttributeValue(tup,
				     InvalidBuffer,
				     attrno,
				     TD,
				     isnullP);
    valueP = datumCopy(val,
		       TD->data[0]->atttypid,
		       TD->data[0]->attbyval,
		       (Size) TD->data[0]->attlen);
    return valueP;
}

execution_state *
init_execution_state(fcache, args)
     FunctionCachePtr fcache;
     char *args[];
{
    execution_state       *newes;
    execution_state       *nextes;
    execution_state       *preves;
    LispValue             parseTree_list;
    LispValue             planTree_list;
    char                  *src;
    int nargs;

    nargs = fcache->nargs;

    newes = (execution_state *) palloc(sizeof(execution_state));
    nextes = newes;
    preves = (execution_state *)NULL;


    planTree_list = (LispValue)
	pg_plan(fcache->src, fcache->argOidVect, nargs, &parseTree_list, None);

    for ( ;
	 planTree_list;
	 planTree_list = CDR(planTree_list),parseTree_list = CDR(parseTree_list)
	)
    {
	EState    estate;
	LispValue parseTree = CAR(parseTree_list);
	LispValue planTree = CAR(planTree_list);

	if (!nextes)
	    nextes = (execution_state *) palloc(sizeof(execution_state));
	if (preves)
	    preves->next = nextes;

	nextes->next = NULL;
	nextes->status = F_EXEC_START;
	nextes->qd = CreateQueryDesc(parseTree,
				     planTree,
				     args,
				     fcache->argOidVect,
				     nargs,
				     None);
	estate = CreateExecutorState();

	if (nargs > 0)
	{
	    int           i;
	    ParamListInfo paramLI;

	    paramLI =
		(ParamListInfo)palloc((nargs+1)*sizeof(ParamListInfoData));

	    bzero(paramLI, nargs*sizeof(ParamListInfoData));
	    set_es_param_list_info(estate, paramLI);

	    for (i=0; i<nargs; paramLI++, i++)
	    {
	    	paramLI->kind = PARAM_NUM;
	    	paramLI->id = i+1;
	    	paramLI->isnull = false;
	    	paramLI->value = NULL;
	    }
	    paramLI->kind = PARAM_INVALID;
	}
	else
	    set_es_param_list_info(estate, (ParamListInfo)NULL);
        nextes->estate = estate;
	preves = nextes;
	nextes = (execution_state *)NULL;
    }

    return newes;
}

List
postquel_start(qd, estate)
     List qd;
     EState estate;
{
    return ExecMain(qd, estate,
		    lispCons(lispInteger(EXEC_START), LispNil));
}

TupleTableSlot
postquel_getnext(qd, estate)
     List qd;
     EState estate;
{
    return (TupleTableSlot)
	ExecMain(qd, estate, lispCons(lispInteger(EXEC_RETONE), LispNil));
}

List
postquel_end(qd, estate)
     List qd;
     EState estate;
{
    List res3;
    res3 = ExecMain(qd, estate,
		    lispCons(lispInteger(EXEC_END), LispNil));
    return res3;
}

void
postquel_sub_params(fcache, es, args)
    FunctionCachePtr fcache;
    execution_state  *es;
    char             *args[];
{
    execution_state *es;
    ParamListInfo paramLI;
    EState estate;
    int i;

    estate = es->estate;
    paramLI = get_es_param_list_info(estate);

    while (paramLI->kind != PARAM_INVALID)
    {
	if (paramLI->kind == PARAM_NUM)
	{
	    Assert(paramLI->id <= fcache->nargs);
	    paramLI->value = (Datum)args[(paramLI->id - 1)];
	    paramLI++;
	}
    }
}

Datum
postquel_function(funcNode, args, isNull, isDone)
     Func funcNode;
     char *args[];
     bool *isNull;
     bool *isDone;
{
    TupleTableSlot   slot;
    HeapTuple        tup;
    Datum            value;
    execution_state  *es;
    LispValue	     tlist;
    FunctionCachePtr fcache = get_func_fcache(funcNode);

    es = (execution_state *) fcache->func_state;
    if (es == NULL)
    {
	es = init_execution_state(fcache, args);
	fcache->func_state = (char *) es;
    }

    while (es && es->status == F_EXEC_DONE)
    	es = es->next;

    if (!es)
    {
	*isDone = true;
	return NULL;
    }
    if (es->status == F_EXEC_START)
    {
	(void) postquel_start(es->qd, es->estate);
	es->status = F_EXEC_RUN;
    }

    if (fcache->nargs > 0)
        postquel_sub_params(fcache, es, args);

    slot = postquel_getnext(es->qd, es->estate);

    if (TupIsNull(slot))
    {
	(void)postquel_end(es->qd, es->estate);
	es->status = F_EXEC_DONE;
	/*
	 * If there aren't anymore commands in this function then we
	 * are completely done.
	 */
	if (! es->next)
	{
	    es = (execution_state *)fcache->func_state;
	    while (es)
	    {
		es->status = F_EXEC_START;
		es = es->next;
	    }
	    *isDone = true;
	}
	return NULL;
    }
    *isDone = false;

    tup = (HeapTuple)ExecFetchTuple((Pointer)slot);
    if ((tlist = get_func_tlist(funcNode)) != (List)NULL)
    {
	List tle = CAR(tlist);

	value = ProjectAttribute(ExecSlotDescriptor(slot),tle,tup,isNull);
    }
    return value;
}
