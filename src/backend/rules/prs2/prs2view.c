/*========================================================================
 *
 * IDENTIFICATION:
 * 	$Header$
 *
 * DESCRIPTION:
 * This file contains (among other things!) the code for 'view-like' rules,
 * i.e. rules that return many tuples at a time like:
 *	ON retrieve to <relation>
 *      DO [ instead ] retrieve ......
 *
 * (if we specify a 'instead' then this is a virtual view,
 * otherwise it might be partially materialized)
 *
 *
 *========================================================================
 */

#include <stdio.h>
#include "utils/log.h"
#include "executor/execdefs.h"
#include "rules/prs2locks.h"
#include "rules/prs2.h"
#include "rules/prs2stub.h"
#include "nodes/execnodes.h"
#include "utils/palloc.h"

extern EState CreateExecutorState();
extern LispValue ExecMain();
extern HeapTuple palloctup();

/*-----------------------------------------------------------------------
 * prs2MakeScanStateRuleInfo(relation)
 *
 * This routine is called by 'ExecInitSeqScan' and 'ExecInitIndexScan'
 * when the 'ScanState' of a 'Scan' node is intialized.
 * This scan state contains among other things some information
 * needed by the rule manager. See commnets in the definition of the
 * 'ScanStateRuleInfo' struct for more details....
 *
 */
ScanStateRuleInfo
prs2MakeScanStateRuleInfo(relation)
Relation relation;
{
    RuleLock locks;
    Name relationName;
    int i, j, nlocks;
    Prs2OneLock oneLock;
    Prs2LockType lockType;
    ObjectId ruleId;
    Prs2PlanNumber planNo;
    long size;
    int nrules;
    Prs2RuleList  ruleList, ruleListItem;
    ScanStateRuleInfo scanStateRuleInfo;


    /*
     * find the locks of the relation...
     */
    relationName = RelationGetRelationName(relation);
    locks = prs2GetLocksFromRelation(relationName);

    /*
     * create a linked list of all the rule ids that hold a
     * 'LockTypeRetrieveRelation'
     */
    ruleList = NULL;
    nlocks = prs2GetNumberOfLocks(locks);
    for (i=0; i<nlocks ; i++) {
        oneLock = prs2GetOneLockFromLocks(locks, i);
        lockType = prs2OneLockGetLockType(oneLock);
        ruleId = prs2OneLockGetRuleId(oneLock);
	planNo = prs2OneLockGetPlanNumber(oneLock);
        if (lockType==LockTypeRetrieveRelation) {
	    ruleListItem = (Prs2RuleList) palloc(sizeof(Prs2RuleListItem));
	    if (ruleListItem == NULL) {
		elog(WARN,"prs2MakeRuleList: Out of memory");
	    }
	    ruleListItem->type = PRS2_RULELIST_RULEID;
	    ruleListItem->data.ruleId.ruleId = ruleId;
	    ruleListItem->data.ruleId.planNumber = planNo;
	    ruleListItem->next = ruleList;
	    ruleList = ruleListItem;
	}
    }

    /*
     * Create and return a 'ScanStateRuleInfo' structure..
     */
    size = sizeof(ScanStateRuleInfoData);
    scanStateRuleInfo = (ScanStateRuleInfo) palloc(size);
    if (scanStateRuleInfo == NULL) {
	elog(WARN,
	    "prs2MakeScanStateRuleInfo: Out of memory! (%ld bytes requested)",
	    size);
    }

    scanStateRuleInfo->ruleList = ruleList;
    scanStateRuleInfo->insteadRuleFound = false;
    scanStateRuleInfo->relationLocks = locks;
    scanStateRuleInfo->relationStubs =
	prs2GetRelationStubs(RelationGetRelationId(relation));
    scanStateRuleInfo->relationStubsHaveChanged = false;

    return(scanStateRuleInfo);

}

/*-----------------------------------------------------------------------
 *
 * prs2GetOneTupleFromViewRules
 */
HeapTuple
prs2GetOneTupleFromViewRules(scanStateRuleInfo, prs2EStateInfo, relation,
			    explainRel)
ScanStateRuleInfo scanStateRuleInfo;
Prs2EStateInfo prs2EStateInfo;
Relation relation;
Relation explainRel;
{

    long size;
    Prs2RuleList tempRuleList, tempRuleListItem;
    ParamListInfo paramList;
    LispValue plan, ruleInfo, planQual, planActions, onePlan;
    LispValue queryDesc;
    EState executorState;
    LispValue res;
    HeapTuple tuple;


    /*
     * Process the first record of 'ruleList',
     * until either the lsit becomes empty, or
     * a new tuple is created....
     */
    while(scanStateRuleInfo->ruleList != NULL) {
	switch (scanStateRuleInfo->ruleList->type) {
	    case PRS2_RULELIST_RULEID:
		/*
		 * Fetch the plan from Prs2PlansRelation.
		 */
		plan = prs2GetRulePlanFromCatalog(
			    scanStateRuleInfo->ruleList->data.ruleId.ruleId,
			    scanStateRuleInfo->ruleList->data.ruleId.planNumber,
			    &paramList);
		/*
		 * It is possible that plan = nil (an obsolete rule?)
		 * In this case ignore the rule.
		 */
		if (null(plan)) {
		    break;
		}
		/*
		 * NOTE: the `paramList' must be empty!
		 * because we do not allow 'NEW' & 'OLD' in 'view-like' rules.
		 * Note though, that the rule qualification might not
		 * be empty. E.g. it might be something like:
		 *       where user() == "Spyros the Great"
		 */
		if (paramList != NULL && paramList[0].kind != PARAM_INVALID) {
		    elog(WARN, "View Rules must not have parameters!\n");
		}
		ruleInfo = prs2GetRuleInfoFromActionPlan(plan);
		planQual = prs2GetQualFromActionPlan(plan);
		planActions = prs2GetActionsFromActionPlan(plan);
		/*
		 * test the rule qualification and if true, then
		 * for every plan specified in the action part of the
		 * rule, create a Prs2RuleListItem and insert it
		 * in a linked list.
		 */
		if (prs2CheckQual(planQual, paramList, prs2EStateInfo)) {
		    tempRuleList = scanStateRuleInfo->ruleList->next;
		    foreach(onePlan, planActions) {
			size = sizeof(Prs2RuleListItem);
			tempRuleListItem = (Prs2RuleList) palloc(size);
			if (tempRuleListItem == NULL) {
			    elog(WARN,
			    "prs2GetOneTupleFromViewRules: Out of memory");
			}
			tempRuleListItem->type = PRS2_RULELIST_PLAN;
			tempRuleListItem->data.rulePlan.plan = CAR(onePlan);
			tempRuleListItem->next = tempRuleList;
			tempRuleList = tempRuleListItem;
		    }
		    /*
		     * no more action plans for this rule.
		     * replace the Prs2RuleListItem of theis rule with
		     * the chain of Prs2RuleListItem(s) we have just
		     * created.
		     */
		    tempRuleListItem = scanStateRuleInfo->ruleList;
		    scanStateRuleInfo->ruleList = tempRuleList;
		    pfree(tempRuleListItem);
		    /*
		     * check for 'instead' rules...
		     */
		    if (prs2IsRuleInsteadFromRuleInfo(ruleInfo)) {
			scanStateRuleInfo->insteadRuleFound = true;
		    }
		}
		break;
	    case PRS2_RULELIST_PLAN:
		/*
		 * this is a rule plan. COnstruct the query descriptor,
		 * the EState, and call the executor to perform the
		 * EXEC_START operation...
		 */
		queryDesc = prs2MakeQueryDescriptorFromPlan(
			    scanStateRuleInfo->ruleList->data.rulePlan.plan);
		executorState = CreateExecutorState();
		set_es_param_list_info(executorState, paramList);
		set_es_prs2_info(executorState, prs2EStateInfo);
		ExecMain(queryDesc, executorState,
			lispCons(lispInteger(EXEC_START), LispNil));
		size = sizeof(Prs2RuleListItem);
		tempRuleList = (Prs2RuleList) palloc(size);
		if (tempRuleList == NULL) {
		    elog(WARN,
		    "prs2GetOneTupleFromViewRules: Out of memory");
		}
		tempRuleList->type = PRS2_RULELIST_QUERYDESC;
		tempRuleList->data.queryDesc.queryDesc = queryDesc;
		/*
		 * NOTE: the following typecast is due to a stupid
		 * circular dependency in the definitions of `EState' and
		 * `Prs2RuleList' which obliged me to declare 
		 * `Prs2RuleList->data.queryDesc.estate' not as an `EState'
		 * (which is the correct) but as a (struct *). Argh!
		 */
		tempRuleList->data.queryDesc.estate =
			(struct EState *)executorState;
		tempRuleList->next = scanStateRuleInfo->ruleList->next;
		tempRuleListItem = scanStateRuleInfo->ruleList;
		scanStateRuleInfo->ruleList = tempRuleList;
		pfree(tempRuleListItem);
		break;
	    case PRS2_RULELIST_QUERYDESC:
		/*
		 * At last! a query desciptor (and an EState!)
		 * properly initialized...
		 * Call exec main to retrieve a tuple...
		 */
		res = ExecMain(
			scanStateRuleInfo->ruleList->data.queryDesc.queryDesc,
			scanStateRuleInfo->ruleList->data.queryDesc.estate,
			lispCons(lispInteger(EXEC_RETONE), LispNil));
		tuple = (HeapTuple) ExecFetchTuple((TupleTableSlot)res);
		if (tuple != NULL) {
		    /*
		     * Yeahhh! the executor returned a tuple!
		     *
		     * Make a COPY of this tuple
		     * we need to make a copy for 2 reasons:
		     * a) the (low level) executor might release it when
		     *    called with EXEC_END
		     * b) the top level executor might want to pfree it
		     *    and pfree seems to complain (I guess something
		     *    to do with the fact that this palloc happenned
		     *    in the lower executor's context and not in
		     *    the current one...
		     */
		    tuple = palloctup(tuple, InvalidBuffer, relation);
		    return(tuple);
		}
		/*
		 * No more tuples! Remove this record from the
		 * scanStateRuleInfo->ruleList.
		 * But before doing that, be nice and gracefully
		 * let the executor release whatever it has to...
		 */
		ExecMain(scanStateRuleInfo->ruleList->data.queryDesc.queryDesc,
			    scanStateRuleInfo->ruleList->data.queryDesc.estate,
			    lispCons(lispInteger(EXEC_END), LispNil));
		tempRuleList = scanStateRuleInfo->ruleList;
		scanStateRuleInfo->ruleList = scanStateRuleInfo->ruleList->next;
		pfree(tempRuleList);
		break;
	} /* switch */
    } /* while */

    /*
     * No (more) rules found, return NULL
     */
    return(NULL);
}
