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
#include "parser/parse.h"	/* for RETRIEVE, APPEND etc. */
#include "utils/palloc.h"

extern EState CreateExecutorState();
extern LispValue ExecMain();

/*-----------------------------------------------------------------------
 * prs2MakeRelationRuleInfo(relation)
 *
 * This initializes some information about the rules affecting a
 * relation.
 *
 * There are 2 places where this routine can be called:
 *
 * A) by 'ExecInitSeqScan' and 'ExecInitIndexScan'
 * when the 'ScanState' of a 'Scan' node is intialized.
 * In this case `operation' must be RETRIEVE
 *
 * B) by InitPlan when we have an update plan (i.e. a delete, append
 * or a replace command). In this case the 'operation' must be
 * APPEND, DELETE or REPLaCE.
 * 
 * This scan state contains among other things some information
 * needed by the rule manager. See commnets in the definition of the
 * 'RelationRuleInfo' struct for more details....
 *
 */
RelationRuleInfo
prs2MakeRelationRuleInfo(relation, operation)
Relation relation;
int operation;
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
    Prs2Stub stubs;
    RelationRuleInfo relationRuleInfo;


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
    if (operation == RETRIEVE) {
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
    }

    /*
     * now find the relation level rule stubs.
     */
    stubs = prs2GetRelationStubs(RelationGetRelationId(relation));

    /*
     * Create and return a 'RelationRuleInfo' structure..
     */
    size = sizeof(RelationRuleInfoData);
    relationRuleInfo = (RelationRuleInfo) palloc(size);
    if (relationRuleInfo == NULL) {
	elog(WARN,
	    "prs2MakeRelationRuleInfo: Out of memory! (%ld bytes requested)",
	    size);
    }

    relationRuleInfo->ruleList = ruleList;
    relationRuleInfo->insteadViewRuleFound = false;
    relationRuleInfo->relationLocks = locks;
    relationRuleInfo->relationStubs = stubs;
    relationRuleInfo->relationStubsHaveChanged = false;

    /*
     * if this is the "pg_class" then ignore all tuple level locks
     * found in tuples, because these locks are not "real" tuple level
     * locks of rules defined in "pg_class", but they are relation level
     * locks defined on various other relations.
     */
    if (!strcmp(relationName, Name_pg_relation))
	relationRuleInfo->ignoreTupleLocks = true;
    else
	relationRuleInfo->ignoreTupleLocks = false;

    return(relationRuleInfo);

}

/*-----------------------------------------------------------------------
 *
 * prs2GetOneTupleFromViewRules
 */
HeapTuple
prs2GetOneTupleFromViewRules(relationRuleInfo, prs2EStateInfo, relation,
			    explainRel)
RelationRuleInfo relationRuleInfo;
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
    while(relationRuleInfo->ruleList != NULL) {
	switch (relationRuleInfo->ruleList->type) {
	    case PRS2_RULELIST_RULEID:
		/*
		 * Fetch the plan from Prs2PlansRelation.
		 */
		plan = prs2GetRulePlanFromCatalog(
			    relationRuleInfo->ruleList->data.ruleId.ruleId,
			    relationRuleInfo->ruleList->data.ruleId.planNumber,
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
		    tempRuleList = relationRuleInfo->ruleList->next;
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
		    tempRuleListItem = relationRuleInfo->ruleList;
		    relationRuleInfo->ruleList = tempRuleList;
		    pfree((Pointer)tempRuleListItem);
		    /*
		     * check for 'instead' rules...
		     */
		    if (prs2IsRuleInsteadFromRuleInfo(ruleInfo)) {
			relationRuleInfo->insteadViewRuleFound = true;
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
			    relationRuleInfo->ruleList->data.rulePlan.plan);
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
		tempRuleList->data.queryDesc.estate = (Pointer) executorState;
		tempRuleList->next = relationRuleInfo->ruleList->next;
		tempRuleListItem = relationRuleInfo->ruleList;
		relationRuleInfo->ruleList = tempRuleList;
		pfree((Pointer)tempRuleListItem);
		break;
	    case PRS2_RULELIST_QUERYDESC:
		/*
		 * At last! a query desciptor (and an EState!)
		 * properly initialized...
		 * Call exec main to retrieve a tuple...
		 */
		res = ExecMain(
			relationRuleInfo->ruleList->data.queryDesc.queryDesc,
			relationRuleInfo->ruleList->data.queryDesc.estate,
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
		 * relationRuleInfo->ruleList.
		 * But before doing that, be nice and gracefully
		 * let the executor release whatever it has to...
		 */
		ExecMain(relationRuleInfo->ruleList->data.queryDesc.queryDesc,
			    relationRuleInfo->ruleList->data.queryDesc.estate,
			    lispCons(lispInteger(EXEC_END), LispNil));
		tempRuleList = relationRuleInfo->ruleList;
		relationRuleInfo->ruleList = relationRuleInfo->ruleList->next;
		pfree((Pointer)tempRuleList);
		break;
	} /* switch */
    } /* while */

    /*
     * No (more) rules found, return NULL
     */
    return(NULL);
}
