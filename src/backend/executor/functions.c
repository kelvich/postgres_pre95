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

#define LAST_POSTQUEL_COMMAND(es) ((es)->next == (execution_state *)NULL)

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
		       TD->data[attrno-1]->atttypid,
		       TD->data[attrno-1]->attbyval,
		       (Size) TD->data[attrno-1]->attlen);
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
postquel_start(es)
    execution_state *es;
{
    return ExecMain(es->qd, es->estate,
		    lispCons(lispInteger(EXEC_START), LispNil));
}

TupleTableSlot
postquel_getnext(es)
    execution_state *es;
{
    LispValue feature;

    feature = (LAST_POSTQUEL_COMMAND(es)) ?
	lispCons(lispInteger(EXEC_RETONE), LispNil) :
	lispCons(lispInteger(EXEC_RUN), LispNil);

    return (TupleTableSlot) ExecMain(es->qd, es->estate, feature);
}

List
postquel_end(es)
    execution_state *es;
{
    List qd;
    EState estate;
    List res3;
    res3 = ExecMain(es->qd, es->estate,
		    lispCons(lispInteger(EXEC_END), LispNil));
    return res3;
}

void
postquel_sub_params(es, nargs, args)
    execution_state  *es;
    int              nargs;
    char             *args[];
{
    ParamListInfo paramLI;
    EState estate;
    int i;

    estate = es->estate;
    paramLI = get_es_param_list_info(estate);

    while (paramLI->kind != PARAM_INVALID)
    {
	if (paramLI->kind == PARAM_NUM)
	{
	    Assert(paramLI->id <= nargs);
	    paramLI->value = (Datum)args[(paramLI->id - 1)];
	    paramLI++;
	}
    }
}

TupleTableSlot
postquel_execute(es, fcache, args)
    execution_state *es;
    FunctionCachePtr fcache;
    char **args;
{
    TupleTableSlot slot;

    if (es->status == F_EXEC_START)
    {
	(void) postquel_start(es);
	es->status = F_EXEC_RUN;
    }

    if (fcache->nargs > 0)
        postquel_sub_params(es, fcache->nargs, args);

    slot = postquel_getnext(es);

    if (slot == (TupleTableSlot)NULL)
    {
	postquel_end(es);
	es->status = F_EXEC_DONE;
    }

    return slot;
}

Datum
postquel_function(funcNode, args, isNull, isDone)
     Func funcNode;
     char *args[];
     bool *isNull;
     bool *isDone;
{
    HeapTuple        tup;
    Datum            value;
    execution_state  *es;
    LispValue	     tlist;
    TupleTableSlot   slot = (TupleTableSlot)NULL;
    FunctionCachePtr fcache = get_func_fcache(funcNode);

    es = (execution_state *) fcache->func_state;
    if (es == NULL)
    {
	es = init_execution_state(fcache, args);
	fcache->func_state = (char *) es;
    }

    while (es && es->status == F_EXEC_DONE)
    	es = es->next;

    Assert(es);
    /*
     * Execute each command in the function one after another until we
     * either get a tuple back from one of them or we run out of commands.
     */
    while (slot == (TupleTableSlot)NULL && es != (execution_state *)NULL)
    {
	slot = postquel_execute(es, fcache, args);
	/*
	 * If the current command is done move to the next command.
	 */
	if (slot == (TupleTableSlot)NULL)
	    es = es->next;
    }

    /*
     * If we've gone through every command in this function, we are done.
     */
    if (es == (execution_state *)NULL)
    {
	/*
	 * Reset the execution states to start over again
	 */
	es = (execution_state *)fcache->func_state;
	while (es)
	{
	    es->status = F_EXEC_START;
	    es = es->next;
	}
	/*
	 * Let caller know we're finished.
	 */
	*isDone = true;
	return NULL;
    }
    *isDone = false;

    /*
     * We only return results when this is the final command in the
     * function.  Currently all functions must return something.
     */
    if (LAST_POSTQUEL_COMMAND(es))
    {
	tup = (HeapTuple)ExecFetchTuple((Pointer)slot);
	if ((tlist = get_func_tlist(funcNode)) != (List)NULL)
	{
	    List tle = CAR(tlist);

	    value = ProjectAttribute(ExecSlotDescriptor(slot),tle,tup,isNull);
	}
	else
	    value = (Datum) slot;
	return value;
    }
    *isNull = true;
    return NULL;
}
