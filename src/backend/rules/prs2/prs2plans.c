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
#include "ruleutil.h"		/* for StringToPlan(), MakeQueryDesc()
				CreateExecutorState() */
#include "executor.h"		/* for EState */

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
Prs2Locks
prs2GetLocksFromRelation(relationName)
Name relationName;
{
    HeapTuple relationTuple;
    Prs2Locks relationLocks;

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
 * prs2RunOnePlanAndGetValue
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
    EState executorState;
    LispValue queryDescriptor;
    LispValue res1, res2, res3;
    AttributeNumber numberOfAttributes;
    TupleDescriptor resultTupleDescriptor;
    HeapTuple resultTuple;
    LispValue parseTree;
    LispValue plan;


    /*
     * the actionPlan must be a list containing the parsetree & the
     * plan that has to be executed.
     */
    parseTree = prs2GetParseTreeFromOneActionPlan(actionPlan);
    plan = prs2GetPlanFromOneActionPlan(actionPlan);

    if (root_command_type(parse_root(parseTree)) != RETRIEVE) {
	elog(WARN,"prs2RunPlanAndGetValue: plan is not a retrieve");
    }
    queryDescriptor = MakeQueryDesc(
			    lispAtom("retrieve"),
			    parseTree,
			    plan,
			    LispNil,
			    LispNil);

    executorState = CreateExecutorState();
    set_es_param_list_info(executorState, paramList);
    set_es_prs2_info(executorState, prs2EStateInfo);

    res1 = ExecMain(queryDescriptor, executorState,
		    lispCons(lispInteger(EXEC_START), LispNil));
    
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

    res2 = ExecMain(queryDescriptor, executorState,
		    lispCons(lispInteger(EXEC_RETONE), LispNil));

    res3 = ExecMain(queryDescriptor, executorState,
		    lispCons(lispInteger(EXEC_END), LispNil));

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
 * prs2RunActionPlans
 *
 */

void
prs2RunActionPlans(plan, paramList, prs2EStateInfo)
LispValue plan;
ParamListInfo paramList;
Prs2EStateInfo prs2EStateInfo;
{
}
