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
#include "nodes/plannodes.h"
#include "planner/keys.h"
#include "catalog/pg_proc.h"
#include "tcop/pquery.h"
#include "utils/params.h"
#include "utils/fmgr.h"
#include "utils/fcache.h"
#include "utils/log.h"
#include "catalog/syscache.h"
#include "catalog/pg_language.h"
#include "access/heapam.h"
#include "executor/executor.h"

#include "rules/prs2locks.h"	/*
				 * this because we have to worry about
				 * copying rule locks when we copy tuples
				 * d*mn you sp!! -mer 7 July 1992
				 */
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
    if (*isnullP)
	return NULL;

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
	Plan planTree = (Plan)CAR(planTree_list);

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
postquel_sub_params(es, nargs, args, nullV)
    execution_state  *es;
    int              nargs;
    char             *args[];
    bool             *nullV;
{
    ParamListInfo paramLI;
    EState estate;

    estate = es->estate;
    paramLI = get_es_param_list_info(estate);

    while (paramLI->kind != PARAM_INVALID)
    {
	if (paramLI->kind == PARAM_NUM)
	{
	    Assert(paramLI->id <= nargs);
	    paramLI->value = (Datum)args[(paramLI->id - 1)];
	    paramLI->isnull = nullV[(paramLI->id - 1)];
	}
	paramLI++;
    }
}

TupleTableSlot
copy_function_result(fcache, resultSlot)
    FunctionCachePtr fcache;
    TupleTableSlot   resultSlot;
{
    TupleTableSlot  funcSlot;
    TupleDescriptor resultTd;
    HeapTuple newTuple;
    HeapTuple oldTuple;

    Assert(! TupIsNull((Pointer)resultSlot));
    oldTuple = (HeapTuple)ExecFetchTuple(resultSlot);

    funcSlot = (TupleTableSlot)fcache->funcSlot;
    
    if (funcSlot == (TupleTableSlot)NULL)
	return resultSlot;

    resultTd = ExecSlotDescriptor(resultSlot);
    /*
     * When the funcSlot is NULL we have to initialize the funcSlot's
     * tuple descriptor.
     */
    if (TupIsNull((Pointer)funcSlot))
    {
	int             i      = 0;
	TupleDescriptor funcTd = ExecSlotDescriptor(funcSlot);

	while (i < oldTuple->t_natts)
	{
	    funcTd->data[i] =
		    (AttributeTupleForm)palloc(sizeof(FormData_pg_attribute));
	    bcopy(resultTd->data[i],
		  funcTd->data[i],
		  sizeof(FormData_pg_attribute));
	    i++;
	}
    }

    newTuple = (HeapTuple)heap_copysimple(oldTuple);

    return (TupleTableSlot)
	ExecStoreTuple((Pointer)newTuple,
		   (Pointer)funcSlot,
		   InvalidBuffer,
		   true);
}

Datum
postquel_execute(es, fcache, fTlist, args, isNull)
    execution_state  *es;
    FunctionCachePtr fcache;
    LispValue        fTlist;
    char             **args;
    bool             *isNull;
{
    TupleTableSlot slot;
    Datum          value;

    if (es->status == F_EXEC_START)
    {
	(void) postquel_start(es);
	es->status = F_EXEC_RUN;
    }

    if (fcache->nargs > 0)
        postquel_sub_params(es, fcache->nargs, args, fcache->nullVect);

    slot = postquel_getnext(es);

    if (TupIsNull((Pointer)slot))
    {
	postquel_end(es);
	es->status = F_EXEC_DONE;
	*isNull = true;
	/*
	 * If this isn't the last command for the function
	 * we have to increment the command
	 * counter so that subsequent commands can see changes made
	 * by previous ones.
	 */
	if (!LAST_POSTQUEL_COMMAND(es)) CommandCounterIncrement();
	return (Datum)NULL;
    }

    if (LAST_POSTQUEL_COMMAND(es))
    {
	TupleTableSlot resSlot;

	/*
	 * Copy the result.  copy_function_result is smart enough
	 * to do nothing when no action is called for.  This helps
	 * reduce the logic and code redundancy here.
	 */
	resSlot = copy_function_result(fcache, slot);
	if (fTlist != (LispValue)NULL)
	{
	    HeapTuple tup;
	    List      tle = CAR(fTlist);

	    tup = (HeapTuple)ExecFetchTuple((Pointer)resSlot);
	    value = ProjectAttribute(ExecSlotDescriptor(resSlot),
				     tle,
				     tup,
				     isNull);
	}
	else
	{
	    value = (Datum)resSlot;
	    *isNull = false;
	}

	/*
	 * If this is a single valued function we have to end the
	 * function execution now.
	 */
	if (fcache->oneResult)
	{
	    (void)postquel_end(es);
	    es->status = F_EXEC_DONE;
	}

	return value;
    }
    /*
     * If this isn't the last command for the function, we don't
     * return any results, but we have to increment the command
     * counter so that subsequent commands can see changes made
     * by previous ones.
     */
    CommandCounterIncrement();
    return (Datum)NULL;
}

Datum
postquel_function(funcNode, args, isNull, isDone)
     Func funcNode;
     char *args[];
     bool *isNull;
     bool *isDone;
{
    HeapTuple        tup;
    execution_state  *es;
    LispValue	     tlist;
    Datum            result;
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
     * Execute each command in the function one after another until we're
     * executing the final command and get a result or we run out of
     * commands.
     */
    while (es != (execution_state *)NULL)
    {
	result = postquel_execute(es,
				  fcache,
				  get_func_tlist(funcNode),
				  args,
				  isNull);
	if (es->status != F_EXEC_DONE)
	    break;
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
	return (fcache->oneResult) ? result : (Datum)NULL;
    }
    /*
     * If we got a result from a command within the function it has
     * to be the final command.  All others shouldn't be returing
     * anything.
     */
    Assert ( LAST_POSTQUEL_COMMAND(es) );
    *isDone = false;

    return result;
}
