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

#include "tmp/postgres.h"
#include "tmp/datum.h"
#include "catalog/syscache.h"
#include "utils/log.h"
#include "nodes/plannodes.h"		/* for EState */
#include "nodes/plannodes.a.h"		/* for EState */
#include "nodes/execnodes.h"		/* for EState */
#include "nodes/execnodes.a.h"		/* for EState */
#include "executor/execdefs.h"
#include "executor/x_execmain.h"
#include "parser/parsetree.h"
#include "utils/fmgr.h"
#include "tcop/dest.h"

#include "catalog/pg_prs2rule.h"
#include "catalog/pg_prs2plans.h"

extern EState CreateExecutorState();
RuleLock prs2SLOWGetLocksFromRelation();

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
	elog(DEBUG,
	"prs2GetRulePlanFromCatalog: plan NOT found (rulid=%ld,planno=%d)\n",
	ruleId, planNumber);
	return(LispNil);
    }

    /*
     * given the tuple extract the 'struct prs2plans' structure
     */
    planStruct = (struct prs2plans *) GETSTRUCT(planTuple);

    /*
     * This is a little bit tricky but it works!
     *
     * t = VARDATA(&planStruct->prs2code);
     * planString = PSIZESKIP(t);
     *
     * Unfortunately it assumes the wrong semantics, and this
     * causes problems when things change.
     */
     planString = VARDATA(&(planStruct->prs2code));

    if (paramListP == NULL)
	plan = StringToPlan(planString);
    else
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

    relationLocks = prs2GetLocksFromTuple(relationTuple, InvalidBuffer);

    return(relationLocks);
}

/*------------------------------------------------------------------
 * prs2SLOWGetLocksFromRelation
 *
 * get the locks but without using the sys cache.
 * Do a scan in the pg_class...
 * The only reason for the existence of this routine was a flaky system
 * cache... I kust keep it here in case things are broken again.
 *
 *------------------------------------------------------------------
 */
RuleLock
prs2SLOWGetLocksFromRelation(relationName)
Name relationName;
{
    Relation rel;
    TupleDescriptor tdesc;
    ScanKeyData scanKey;
    HeapScanDesc scanDesc;
    HeapTuple tuple;
    Buffer buffer;
    RuleLock locks;

    rel = RelationNameOpenHeapRelation(Name_pg_relation);
    tdesc = RelationGetTupleDescriptor(rel);
    /*----
     * Scan pg_relation
     */
	ScanKeyEntryInitialize(&scanKey.data[0], 0, Anum_pg_relation_relname,
						   F_CHAR16EQ, NameGetDatum(relationName));
    scanDesc = RelationBeginHeapScan(rel, false, NowTimeQual, 1, &scanKey);
    tuple = HeapScanGetNextTuple(scanDesc, false, &buffer);
    locks = prs2GetLocksFromTuple(tuple, buffer);
    RelationCloseHeapRelation(rel);
    HeapScanEnd(scanDesc);

    return(locks);
}
/*------------------------------------------------------------------
 *
 * prs2CheckQual
 *
 * Return 1 if the given rule qualification is true
 * (i.e. it returns at least one tuple).
 * Otherwise return 0.
 *
 * NOTE: A null qualification always evaluates to true.
 *
 */
int
prs2CheckQual(planQual, paramList, prs2EStateInfo)
LispValue planQual;
ParamListInfo paramList;
Prs2EStateInfo prs2EStateInfo;
{
    int status;

    if (planQual == LispNil)
	return(1);

    status = prs2RunOnePlanAndGetValue(
		    planQual,
		    paramList,
		    prs2EStateInfo,
		    NULL, NULL, NULL);
    
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
    int feature;
    LispValue onePlan;

    foreach(onePlan, plans) {
	/*
	 * XXX SOS XXX
	 * What kind of 'feature' should we use ??
	 */
	feature = EXEC_RUN;
	prs2RunOnePlan(CAR(onePlan), paramList, prs2EStateInfo, feature);
    }
	
}

/*------------------------------------------------------------------
 *
 * prs2RunOnePlanAndGetValue
 *
 * Run a retrieve plan and return 1 if a tuple was found or 0 if there
 * was no tuple.
 * 
 * Sometimes when we call this routine, we are only interested in
 * whether there is a tuple returned or not, but we are *not*
 * interested in the tuple itself (for instance, when we test
 * a rule qualification).
 * In this case, "valueP" and "isNullP" are NULL
 *
 * On the other hand, sometimes (when we execute the action part of an 
 * "on retrieve ... do instead retrieve ..." rule) we want the value of
 * the tuple's attribute too. This tuple has always one attribute.
 * NOTE: We make a copy of the attribute!
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
    
    AttributeNumber numberOfAttributes;
    TupleDescriptor resultTupleDescriptor;
    LispValue queryDescriptor;
    LispValue res1, res2, res3;
    EState executorState;
    HeapTuple resultTuple;
    TupleTableSlot slot;
    bool oldPolicy;
    int status;

    queryDescriptor = prs2MakeQueryDescriptorFromPlan(actionPlan);

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

    /*
     * XXX - 
     * Unfortunately, by now the executor has freed (with pfree) the tuple
     * descriptor!!!  You will have to either copy it or work out something
     * with Cim - I hacked the executor to avoid freeing this for now,
     * but it *should* be freeing these things to prevent memory leaks.
     *
     * -- Greg
     */
    numberOfAttributes = (AttributeNumber) CInteger(CAR(res1));
    resultTupleDescriptor = (TupleDescriptor) CADR(res1);


    /*
     * now retrieve one tuple
     */
    res2 = ExecMain(queryDescriptor, executorState,
		    lispCons(lispInteger(EXEC_RETONE), LispNil));
    /*
     * "res2" is a 'TupleTableSlot', containing a tuple that will
     * might be freed by the next call to the executor (with operation
     * EXEC_END). However, we might want to use the values of this
     * tuple, in which case we must not free it.
     * To do that, we copy the given slot to another one
     * and change the "freeing policy" of the original slot
     * to "false"
     */
    slot = (TupleTableSlot) res2;
    resultTuple = (HeapTuple) ExecFetchTuple(slot);
    if (resultTuple == NULL) { 
	/*
	 * No tuple was returned...
	 * XXX What shall we do??
	 * Options:
	 *    1) bomb!
	 *    2) continue with the next rule
	 *    3) assume a nil value
	 * Currently option (2) is implemented.
	 */
	status = 0;	/* this means no value found */
    } else {
	/*
	 * A tuple was found. Find the value of its attribute.
	 * NOTE: for the time being we assume that this tuple
	 * has only one attribute!
	 */
	Datum val;

	if (valueP != NULL && isNullP != NULL) {
	    val = HeapTupleGetAttributeValue(
			resultTuple,
			InvalidBuffer,
			(AttributeNumber) 1,
			resultTupleDescriptor,
			isNullP);
	    *valueP = datumCopy(val,
			resultTupleDescriptor->data[0]->atttypid,
			resultTupleDescriptor->data[0]->attbyval,
			(Size) resultTupleDescriptor->data[0]->attlen);

	}
	status = 1;
    }

    /*
     * let the executor do the cleaning up...
     */
    res3 = ExecMain(queryDescriptor, executorState,
		    lispCons(lispInteger(EXEC_END), LispNil));
    
    return(status);
}

/*------------------------------------------------------------------
 *
 * prs2RunOnePlan
 *
 * Run one plan. Ignore the return values from the executor.
 *
 */

void
prs2RunOnePlan(actionPlan, paramList, prs2EStateInfo, operation)
LispValue actionPlan;
ParamListInfo paramList;
Prs2EStateInfo prs2EStateInfo;
int operation;
{
    LispValue queryDescriptor;
    LispValue res1, res2, res3;
    EState executorState;
	TupleDescriptor foo; /* XXX Hack for making copy of tuple descriptor */

    queryDescriptor = prs2MakeQueryDescriptorFromPlan(actionPlan);

    executorState = CreateExecutorState();
    set_es_param_list_info(executorState, paramList);
    set_es_prs2_info(executorState, prs2EStateInfo);

    res1 = ExecMain(queryDescriptor, executorState,
		    lispCons(lispInteger(EXEC_START), LispNil));

    res2 = ExecMain(queryDescriptor, executorState,
		    lispCons(lispInteger(operation), LispNil));
    res3 = ExecMain(queryDescriptor, executorState,
		    lispCons(lispInteger(EXEC_END), LispNil));
}

/*------------------------------------------------------------------
 *
 * prs2IsRuleInsteadFromRuleInfo
 *
 * Given the 'rule info' stored in pg_prs2plans return true iff
 * a rule is an 'instead' rule, otherwise return false
 */
Boolean
prs2IsRuleInsteadFromRuleInfo(ruleInfo)
LispValue ruleInfo;
{
    LispValue t;
    char *s;

    t = CAR(ruleInfo);
    s = CString(t);

    if (!strcmp("instead", s)) {
	return((Boolean) 1);
    } else {
	return((Boolean) 0);
    }
}

/*------------------------------------------------------------------
 *
 * prs2GetEventAttributeNumberFromRuleInfo
 *
 * Given the 'rule info' as stored in pg_prs2plans, 
 * return the attribute number of the attribute specified in the
 * 'ON EVENT to REL.attr' clause
 */
AttributeNumber
prs2GetEventAttributeNumberFromRuleInfo(ruleInfo)
LispValue ruleInfo;
{
    LispValue t;
    int n;

    t = CAR(CDR(ruleInfo));
    n = CInteger(t);

    return(n);
}


/*------------------------------------------------------------------
 *
 * prs2GetUpdatedAttributeNumberFromRuleInfo
 *
 * Given the 'rule info' as stored in pg_prs2plans, 
 * return the attribute number of the attribute of the CURRENT tuple
 * which is updated by the rule. This only applies to rules of the
 * form 'ON RETRIEVE TO REL.X DO RETRIEVE INSTEAD (X = ...)' or
 * 'ON REPLACE TO REL.Y DO REPLACE CURRENT(X = ....)'.
 */
AttributeNumber
prs2GetUpdatedAttributeNumberFromRuleInfo(ruleInfo)
LispValue ruleInfo;
{
    LispValue t;
    int n;

    t = CAR(CDR(CDR(ruleInfo)));
    n = CInteger(t);

    return(n);
}


/*------------------------------------------------------------------
 *
 * prs2MakeQueryDescriptorFromPlan
 *
 * Given the actionPlan, the paramList and the prs2EStateInfo,
 * create a query descriptor
 */

LispValue
prs2MakeQueryDescriptorFromPlan(actionPlan)
LispValue actionPlan;
{
    LispValue queryDescriptor;
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

    queryDescriptor = (LispValue)
			MakeQueryDesc(
			    command,
			    parseTree,
			    plan,
			    LispNil,
			    LispNil,
			    None);

    return(queryDescriptor);
}
