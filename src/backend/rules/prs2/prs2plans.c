/*===================================================================
 *
 * FILE:
 *   prs2plans.c
 *
 * IDENTIFICATION:
 *   $Header$
 *
 * DESCRIPTION:
 *   Various useful routines called by the PRS2 code...
 *
 *
 *===================================================================
 */

#include "c.h"
#include "postgres.h"
#include "syscache.h"
#include "log.h"
#include "prs2.h"
#include "executor.h"		/* for EState */

extern EState CreateExecutorState();

/*-------------------------------------------------------------------
 * prs2GetRulePlanFromCatalog
 * Given a rule id and a plan number get the appropriate plan
 * from the system catalogs. At the same time construct a list
 * with all the Param nodes contained in this plan.
 */
LispValue
prs2GetRulePlanFromCatalog(ruleId, planNumber, paramListP)
ObjectId ruleId;
Prs2PlanNumber planNumber;
ParamListInfo *paramListP;
{
    

    HeapTuple planTuple;
    struct prs2plans *planStruct;
    char *planString;
    LispValue plan;
    char *getstruct();
    char *t;

    /*
     * get the tuple form the system cache
     */
    planTuple = SearchSysCacheTuple(PRS2PLANCODE, ruleId, planNumber);
    if (planTuple == NULL) {
	/*
	 * No such rule exist! Complain!
	 */
	elog(WARN,
	"prs2GetRulePlanFromCatalog: plan NOT found (rulid=%ld,planno=%d)\n",
	ruleId, planNumber);
    }

    /*
     * given the tuple extract the 'struct prs2plans' structure
     */
    planStruct = (struct prs2plans *) GETSTRUCT(planTuple);

    /*
     * This is a little bit tricky but it works!
     */
    t = VARDATA(&planStruct->prs2code);
    planString = PSIZESKIP(t);

    plan = StringToPlanWithParams(planString, paramListP);

    return(plan);
}

/*------------------------------------------------------------------
 *
 * prs2GetLocksFromRelation
 *
 * Given a relation, find all its relation level locks.
 * We have to go to the RelationRelation, get the tuple that corresponds
 * to the given relation and extract its locks field.
 *
 * NOTE: we are returning a pointer to a *copy* of the locks!
 */
RuleLock
prs2GetLocksFromRelation(relationName)
Name relationName;
{
    HeapTuple relationTuple;
    RuleLock relationLocks;

    /*
     * Search the system cache for the relation tuple
     */
    relationTuple = SearchSysCacheTuple(RELNAME,
			    relationName,
			    (char *) NULL,
			    (char *) NULL,
			    (char *) NULL);
    if (!HeapTupleIsValid(relationTuple)) {
	elog(WARN, "prs2GetLocksFromRelation: no rel with name '%s'\n",
	relationName);
    }

    /*
     * I hope that this is gonna work! (tupleDescriptor == NULL !!!!)
     */
    relationLocks = prs2GetLocksFromTuple(
			    relationTuple,
			    InvalidBuffer,
			    (TupleDescriptor) NULL);
    return(relationLocks);
}

/*------------------------------------------------------------------
 *
 * prs2CheckQual
 *
 */
int
prs2CheckQual(planQual, paramList, prs2EStateInfo)
LispValue planQual;
ParamListInfo paramList;
Prs2EStateInfo prs2EStateInfo;
{
    Datum dummyDatum;
    Boolean dummyIsNull;
    int status;

    if (planQual == LispNil)
	return(1);

    status = prs2RunOnePlanAndGetValue(
		    planQual,
		    paramList,
		    &dummyDatum,
		    &dummyIsNull);
    
    return(status);
}

/*------------------------------------------------------------------
 *
 * prs2RunActionPlans
 *
 */

void
prs2RunActionPlans(plans, paramList, prs2EStateInfo)
LispValue plans;
ParamListInfo paramList;
Prs2EStateInfo prs2EStateInfo;
{
    LispValue onePlan;

    foreach(onePlan, plans) {
	prs2RunOnePlan(CAR(onePlan), paramList, prs2EStateInfo);
    }
	
}

/*------------------------------------------------------------------
 *
 * prs2RunOnePlanAndGetValue
 *
 * Run a retrieve plan and return 1 if a tuple was found or 0 if there
 * was no tuple.
 * In the first case, the tuple must have only one attribute,
 * and output arguments *valueP and *isNullP, contain the value and the
 * isNull flag for this attribute.
 *
 */

int
prs2RunOnePlanAndGetValue(actionPlan, paramList, prs2EStateInfo,
                        valueP, isNullP)
LispValue actionPlan;
ParamListInfo paramList;
Prs2EStateInfo prs2EStateInfo;
Datum *valueP;
Boolean *isNullP;
{
    
    LispValue res1, res2, res3, result;
    AttributeNumber numberOfAttributes;
    TupleDescriptor resultTupleDescriptor;
    HeapTuple resultTuple;

    result = prs2RunOnePlan(actionPlan, paramList, prs2EStateInfo, EXEC_START);

    res1 = nth(0, result);
    res2 = nth(1, result);
    res3 = nth(2, result);

    /*
     * When the executor is called with EXEC_START it returns
     * "lispCons(LispInteger(n), lispCons(t, lispNil))"
     * where 'n' is the number of attributes in the result tuple
     * and 't' is an 'AttributePtr'.
     * XXX Note: it appears that 'AttributePtr' and 'TupleDescriptor'
     * are the same thing!
     */
    numberOfAttributes = (AttributeNumber) CInteger(CAR(res1));
    resultTupleDescriptor = (TupleDescriptor) CADR(res1);

    if (res2 == LispNil) { 
	/*
	 * No tuple was returned...
	 * XXX What shall we do??
	 * Options:
	 *    1) bomb!
	 *    2) continue with the next rule
	 *    3) assume a nil value
	 * Currently option (2) is implemented.
	 */
	return(0);	/* this means no value found */
    } else {
	/*
	 * A tuple was found. Find the value of its attribute.
	 * NOTE: for the time being we assume that this tuple
	 * has only one attribute!
	 */
	resultTuple = (HeapTuple) CAR(res2);
	*valueP = HeapTupleGetAttributeValue(
		    resultTuple,
		    InvalidBuffer,
		    (AttributeNumber) 1,
		    resultTupleDescriptor,
		    isNullP);
	return(1);
    }
}

/*------------------------------------------------------------------
 *
 * prs2RunOnePlan
 *
 * Run one plan and return the executor's return values.
 *
 */

LispValue
prs2RunOnePlan(actionPlan, paramList, prs2EStateInfo, operation)
LispValue actionPlan;
ParamListInfo paramList;
Prs2EStateInfo prs2EStateInfo;
int operation;
{
    EState executorState;
    LispValue queryDescriptor;
    LispValue res1, res2, res3, result;
    LispValue parseTree;
    LispValue plan;
    LispValue command;


    /*
     * the actionPlan must be a list containing the parsetree & the
     * plan that has to be executed.
     */
    parseTree = prs2GetParseTreeFromOneActionPlan(actionPlan);
    plan = prs2GetPlanFromOneActionPlan(actionPlan);

    command = root_command_type_atom(parse_root(parseTree));

    queryDescriptor = MakeQueryDesc(
			    command,
			    parseTree,
			    plan,
			    LispNil,
			    LispNil);

    executorState = CreateExecutorState();
    set_es_param_list_info(executorState, paramList);
    set_es_prs2_info(executorState, prs2EStateInfo);

    res1 = ExecMain(queryDescriptor, executorState,
		    lispCons(lispInteger(EXEC_START), LispNil));

    res2 = ExecMain(queryDescriptor, executorState,
		    lispCons(lispInteger(operation), LispNil));

    res3 = ExecMain(queryDescriptor, executorState,
		    lispCons(lispInteger(EXEC_END), LispNil));

    
    result = lispCons(res3, LispNil);
    result = lispCons(res2, result);
    result = lispCons(res1, result);

    return(result);
}
