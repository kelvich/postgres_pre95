/*===================================================================
 *
 * FILE:
 *   prs2bkwd.c
 *
 * IDENTIFICATION:
 *   $Header$
 *
 * DESCRIPTION:
 *   These are routines that can be used to implement the
 *   'backward' chaining rules, i.e. rules that update the
 *   current tuple. Examples of these rules are:
 *    on retrieve to EMP.salary where EMP.name = "mike"
 *    do instead retrieve (salary=1000)
 * or
 *    on append to EMP where EMP.age<25
 *    do replace CURRENT(salary=2000)
 *
 *
 * There are two main routines in this module:
 * `prs2ActivateBackwardChainingRules' and `prs2ActivateForwardChainingRules'
 *
 * At any point the rule manager has 1 or 2 tuples to consider. The 'old'
 * tuple and/or the 'new' tuple.
 * In the case of a 'delete' or 'retrieve' event, it only has an 'old'
 * tuple. In the case of a 'append' it only has a 'new' tuple, and finally
 * in the case of a 'replace' it has both a 'new' and an 'old' tuple.
 * 
 * Any of these events can activate some rules. These rules can
 * either be 'backward chaining' rules (i.e. rules that have
 * a lock of type 'LockTypeXXXWrite' where XXX is the appropriate event)
 * or `forward chanining' (with a 'LockTypeXXXAction' lock).
 * In the first case, we must call 'prs2ActivateBackwardChainingRules' for
 * every attribute that has the 'LockTypeXXXWrite' lock.
 * This routine, finds these rules (if they exist) checks their
 * qualification and if true it activates them, retrieves the value
 * calculated by the rule and puts it in the tuple.
 * 
 * Explanation of parameters:
 * Most fo them are common in both 'prs2ActivateBackwardChainingRules'
 * and 'prs2ActivateForwardChainingRules'.
 *
 *   prs2EStateInfo: information (usually stored in the EState node)
 *       needed for loop detection.
 *   relation: the relation both 'old' & 'new' tuples belong to.
 *   attributeNumber: the number of the attribute that is 'touched'
 *       by the event.
 *   oldOrNewTuple: is this an attribute of the 'old' or of the
 *       'new' tuple?
 *   oldTupleOid: the oid of the 'old' tuple.
 *   oldAttributeValues: an array containing the values of the
 *       'old' tuple, plus some other info.
 *   oldTupleLocks: the rule locks of the old tuple.
 *   oldTupleLockType: the lock type (one of the 'LockTypeXXXWrite'
 *       locks that should be used to calculate the values of the
 *       'old' tuple's attributes. See discussion below.
 *   newTupleOid,
 *   newAttributeValues,
 *   newTupleLocks,
 *   newTupleLockType: similar to 'old' tuple stuff...
 *
 * The following arguments appear only in 'prs2ActivateForwardChainingRules'
 *   eventLockType: The lock type that a forward chaining rule must
 *        have in the specified attribute in order to be activated.
 *    
 * Both routines, might call 'prs2CalculateAttributesOfParamNodes'
 * (which in turn might also call prs2ActivateBackwardChainingRules')
 * The reason is that a rule plan might have some parameters (the NEW
 * & OLD stuff...), in which case we have first to calculate the
 * correct values for the correpsonding attributes (of either the
 * 'new' or 'old' tuple, depending on the type of paramterer)
 * before we execute the plan.
 *
 * 'prs2ActivateBackwardChainingRules' is (directly) called by the PRS2
 * routines that handle 'retrieve', 'append' and 'replace' events.
 * It is called to activate all the backward chaining rules that
 * might affect the value of a certain attribute.
 * 
 * In the 'retrieve' case, we only have an 'old' tuple, so all the
 * info about the 'new' tuple is ignored. The lock type that is 
 * used is 'LockTypeRetrieveWrite'.
 * In the case of 'append' event, we only have a new tuple, and
 * the lock type is 'LockTypeAppendWrite'
 * Finally in the case of replace, we have both a 'new' & 'old'
 * tuple, however we (directly) call 'prs2ActivateBackwardChainingRules'
 * only for the new tuple and the lock type is 'LockTypeReplaceWrite'.
 *
 * 'prs2ActivateForwardChainingRules' is called by
 * the prs2 routines thath handle 'retrieve', 'delete', 'append'
 * and 'replace' events.
 *
 * In the 'retrieve' case, we only have a 'old' tuple, the
 * `eventTypeLock' is 'LockTypeRetrieveAction' and the 'oldTupleLockType'
 * is 'LockTypeRetrieveWrite'
 *
 * In the 'delete' case, we only have an 'old' tuple, the 
 * `eventTypeLock' is 'LockTypeDeleteAction' and the 'oldTupleLockType'
 * is 'LockTypeRetrieveWrite'
 * 
 * In the 'append' case, we only have a 'new' tuple, the 
 * `eventTypeLock' is 'LockTypeAppendAction' and the 'newTupleLockType'
 * is 'LockTypeAppendWrite'
 * 
 * In the 'replace' case, we have both an 'old' & a 'new' tuple, the 
 * `eventTypeLock' is 'LockTypeReplaceAction', the 'oldTupleLockType'
 * is 'LockTypeRetrieveWrite' and the 'newTupleLockType' is
 * 'LockTypeReplaceWrite'
 *
 *===================================================================
 */

#include "c.h"
#include "log.h"
#include "prs2.h"

/*-------------------------------------------------------------------
 * prs2ActivateBackwardChainingRules()
 *
 */
void
prs2ActivateBackwardChainingRules(
	    prs2EStateInfo,
	    relation,
	    attributeNumber,
	    oldOrNewTuple,
	    oldTupleOid,
	    oldAttributeValues,
	    oldTupleLocks,
	    oldTupleLockType,
	    newTupleOid,
	    newAttributeValues,
	    newTupleLocks,
	    newTupleLockType)
Prs2EStateInfo prs2EStateInfo;
Relation relation;
AttributeNumber attributeNumber;
int oldOrNewTuple;
ObjectId oldTupleOid;
AttributeValues oldAttributeValues;
RuleLock oldTupleLocks;
Prs2LockType oldTupleLockType;
ObjectId newTupleOid;
AttributeValues newAttributeValues;
RuleLock newTupleLocks;
Prs2LockType newTupleLockType;
{
    Prs2OneLock oneLock;
    int nlocks;
    int i;
    int finalStatus;
    LispValue plan;
    LispValue ruleInfo;
    LispValue planQual;
    LispValue planAction;
    ParamListInfo paramList;
    AttributeNumber lockAttrNo;
    AttributeValues attributeValues;
    Prs2LockType lockType;
    ObjectId ruleId;
    ObjectId tupleOid;
    RuleLock locks;
    Prs2LockType lockTypeToBeUsed;


    if (oldOrNewTuple == PRS2_OLD_TUPLE) {
	attributeValues = oldAttributeValues;
	lockTypeToBeUsed = oldTupleLockType;
	locks = oldTupleLocks;
	tupleOid = oldTupleOid;
    } else if (oldOrNewTuple == PRS2_NEW_TUPLE) {
	attributeValues = newAttributeValues;
	lockTypeToBeUsed = newTupleLockType;
	locks = newTupleLocks;
	tupleOid = newTupleOid;
    } else {
	elog(WARN,
	"prs2ActivateBackwardChainingRules: illegal tuple spec:%d",
	oldOrNewTuple);
    }
	
    if (attributeValues[attributeNumber-1].isCalculated) {
	/*
	 * we have already calculated the value for this attribute.
	 * Do nothing...
	 */
	return;
    }

    nlocks = prs2GetNumberOfLocks(locks);
    for (i=0; i<nlocks ; i++) {
	oneLock = prs2GetOneLockFromLocks(locks, i);
	lockType = prs2OneLockGetLockType(oneLock);
	lockAttrNo = prs2OneLockGetAttributeNumber(oneLock);
	ruleId = prs2OneLockGetRuleId(oneLock);
        if (lockType==lockTypeToBeUsed&& attributeNumber==lockAttrNo) {
	    /*
	     * We've found a 'LockTypeXXXWrite' lock, i.e. there
	     * is a rule that might calculate a new value
	     * for this attribute.
	     *
	     * Check for loops!
	     */
	    if (prs2RuleStackSearch(prs2EStateInfo,
				ruleId, tupleOid, attributeNumber)) {
		/*
		 * Ooops! a rule loop was found.
		 * Currently we decide just to ignore the rule.
		 */
		continue;	/* the for loop... */
	    }
	    /*
	     * now add the stack (loop detection) info...
	     */
	    prs2RuleStackPush(prs2EStateInfo,
				ruleId, tupleOid, attributeNumber);
	    /*
	     * Go in the system catalogs and extract the
	     * appropriate plan info...
	     */
	    plan = prs2GetRulePlanFromCatalog(
				ruleId,
				prs2OneLockGetPlanNumber(oneLock),
				&paramList);
	    ruleInfo = prs2GetRuleInfoFromRulePlan(plan);
	    planQual = prs2GetQualFromRulePlan(plan);
	    planAction = prs2GetActionsFromRulePlan(plan);

	    /*
	     * Now try to calculate all the parameters needed to fill in
	     * the plan
	     */
	    prs2CalculateAttributesOfParamNodes(
			prs2EStateInfo,
			relation,
			paramList,
			oldTupleOid,
			oldAttributeValues,
			oldTupleLocks,
			oldTupleLockType,
			newTupleOid,
			newAttributeValues,
			newTupleLocks,
			newTupleLockType);
	    if (prs2CheckQual(planQual, paramList, prs2EStateInfo)) {
		/*
		 * the qualification is true. Execute the action
		 * part in order to caclulate the new value
		 * for the attribute...
		 *
		 * Note that 'actionPlan' must only contain one
		 * item!
		 */
		if (CDR(planAction) != LispNil) {
		    elog(WARN, "planAction has more than 1 items!");
		}
		if (prs2RunOnePlanAndGetValue(CAR(planAction),
			    paramList,
			    prs2EStateInfo,
			    &(attributeValues[attributeNumber-1].value),
			    &(attributeValues[attributeNumber-1].isNull))) {
		    /*
		     * A value was calculated by the rule. 
		     * Store it in the 'attributeValues' array.
		     */
		    attributeValues[attributeNumber-1].isCalculated=(Boolean)1;
		    attributeValues[attributeNumber-1].isChanged=(Boolean)1;
		} 
	    /*
	     * OK, now pop the rule stack information
	     */
	    prs2RuleStackPop(prs2EStateInfo);
	    }
	}
    }/*for*/

    if (!attributeValues[attributeNumber-1].isCalculated) {
	/*
	 * no rule was applicable. Use the value stored in the tuple
	 * (NOTE: this value has already been put in the attributeValues,
	 * so we only need to change the flags.
	 */
	attributeValues[attributeNumber-1].isCalculated = (Boolean)1;
	attributeValues[attributeNumber-1].isChanged = (Boolean)0;
    }

    return;

}

/*----------------------------------------------------------------------
 *
 * prs2CalculateAttributesOfParamNodes
 *
 */

void
prs2CalculateAttributesOfParamNodes(
		prs2EStateInfo,
		relation,
		paramList,
		oldTupleOid,
		oldTupleAttributeValues,
		oldTupleRuleLock,
		oldTupleLockType,
		newTupleOid,
		newTupleAttributeValues,
		newTupleRuleLock,
		newTupleLockType)
Prs2EStateInfo prs2EStateInfo;
Relation relation;
ParamListInfo paramList;
ObjectId oldTupleOid;
AttributeValues oldTupleAttributeValues;
RuleLock oldTupleRuleLock;
Prs2LockType oldTupleLockType;
ObjectId newTupleOid;
AttributeValues newTupleAttributeValues;
RuleLock newTupleRuleLock;
Prs2LockType newTupleLockType;
{

    Name attributeName;
    AttributeNumber attributeNumber;
    LispValue attributesNeededInPlan;
    LispValue lispAttributeName;
    int i;
    Boolean isNull;

    if (paramList == NULL) {
	return;
    }

    i = 0;
    while (paramList[i].kind != PARAM_INVALID) {
	attributeName = paramList[i].name;
	attributeNumber = get_attnum(relation->rd_id, attributeName);
	if (paramList[i].kind == PARAM_OLD) {
	    /*
	     * make sure that the value stored in the old tuple
	     * is the correct one
	     */
	    prs2ActivateBackwardChainingRules(
			prs2EStateInfo,
			relation,
			attributeNumber,
			PRS2_OLD_TUPLE,
			oldTupleOid,
			oldTupleAttributeValues,
			oldTupleRuleLock,
			oldTupleLockType,
			newTupleOid,
			newTupleAttributeValues,
			newTupleRuleLock,
			newTupleLockType);
	    /*
	     * store information about this attribute (type, value etc)
	     * in the paramList
	     */
	    paramList[i].type = get_atttype(relation->rd_id, attributeNumber);
	    paramList[i].length = (Size) get_typlen(paramList[i].type);
	    paramList[i].value =
			oldTupleAttributeValues[attributeNumber-1].value;
	    if (oldTupleAttributeValues[attributeNumber-1].isNull)
		paramList[i].isnull = true;
	    else
		paramList[i].isnull = false;
	} else if (paramList[i].kind == PARAM_NEW) {
	    /*
	     * make sure that the value stored in the new tuple
	     * is the correct one
	     */
	    prs2ActivateBackwardChainingRules(
			prs2EStateInfo,
			relation,
			attributeNumber,
			PRS2_NEW_TUPLE,
			oldTupleOid,
			oldTupleAttributeValues,
			oldTupleRuleLock,
			oldTupleLockType,
			newTupleOid,
			newTupleAttributeValues,
			newTupleRuleLock,
			newTupleLockType);
	    /*
	     * store information about this attribute (type, value etc)
	     * in the paramList
	     */
	    paramList[i].type = get_atttype(relation->rd_id, attributeNumber);
	    paramList[i].length = (Size) get_typlen(paramList[i].type);
	    paramList[i].value =
			newTupleAttributeValues[attributeNumber-1].value;
	    if (newTupleAttributeValues[attributeNumber-1].isNull)
		paramList[i].isnull = true;
	    else
		paramList[i].isnull = false;
	} else {
	    elog(WARN,"Illegal param kind = (%d), name = %s",
	    paramList[i].kind, paramList[i].name);
	}
	/*
	 * Continue with the next parameter
	 */
	i += 1;
    } /*while*/
}

/*-----------------------------------------------------------------------
 * prs2ActivateForwardChainingRules
 */
void
prs2ActivateForwardChainingRules(
	    prs2EStateInfo,
	    relation,
	    attributeNumber,
	    actionLockType,
	    oldOrNewTuple,
	    oldTupleOid,
	    oldAttributeValues,
	    oldTupleLocks,
	    oldTupleLockType,
	    newTupleOid,
	    newAttributeValues,
	    newTupleLocks,
	    newTupleLockType,
	    insteadRuleFoundP)
Prs2EStateInfo prs2EStateInfo;
Relation relation;
AttributeNumber attributeNumber;
int oldOrNewTuple;
Prs2LockType actionLockType;
ObjectId oldTupleOid;
AttributeValues oldAttributeValues;
RuleLock oldTupleLocks;
Prs2LockType oldTupleLockType;
ObjectId newTupleOid;
AttributeValues newAttributeValues;
RuleLock newTupleLocks;
Prs2LockType newTupleLockType;
bool *insteadRuleFoundP;
{
    Prs2OneLock oneLock;
    int nlocks;
    int i;
    int finalStatus;
    LispValue plan;
    LispValue ruleInfo;
    LispValue planQual;
    LispValue planActions;
    ParamListInfo paramList;
    AttributeNumber lockAttrNo;
    Prs2LockType lockType;
    ObjectId ruleId;
    RuleLock locks;
    AttributeValues attributeValues;
    ObjectId tupleOid;

    /*
     * Have we encountered an 'instead' rule ???
     */
    *insteadRuleFoundP = false;

    if (oldOrNewTuple == PRS2_OLD_TUPLE) {
	attributeValues = oldAttributeValues;
	locks = oldTupleLocks;
	tupleOid = oldTupleOid;
    } else if (oldOrNewTuple == PRS2_NEW_TUPLE) {
	attributeValues = newAttributeValues;
	locks = newTupleLocks;
	tupleOid = newTupleOid;
    } else {
	elog(WARN,
	"prs2ActivateForwardChainingRules: illegal tuple spec:%d",
	oldOrNewTuple);
    }
	
    nlocks = prs2GetNumberOfLocks(locks);
    for (i=0; i<nlocks ; i++) {
	oneLock = prs2GetOneLockFromLocks(locks, i);
	lockType = prs2OneLockGetLockType(oneLock);
	lockAttrNo = prs2OneLockGetAttributeNumber(oneLock);
	ruleId = prs2OneLockGetRuleId(oneLock);
        if (lockType==actionLockType && attributeNumber==lockAttrNo) {
	    /*
	     * We've found a 'LockTypeXXXAction' lock, i.e. there
	     * is a rule that might be activated by this event.
	     * for this attribute.
	     *
	     * Go in the system catalogs and extract the
	     * appropriate plan info...
	     */
	    plan = prs2GetRulePlanFromCatalog(
				ruleId,
				prs2OneLockGetPlanNumber(oneLock),
				&paramList);
	    ruleInfo = prs2GetRuleInfoFromRulePlan(plan);
	    planQual = prs2GetQualFromRulePlan(plan);
	    planActions = prs2GetActionsFromRulePlan(plan);

	    /*
	     * Now try to calculate all the parameters needed to fill in
	     * the plan
	     */
	    prs2CalculateAttributesOfParamNodes(
			prs2EStateInfo,
			relation,
			paramList,
			oldTupleOid,
			oldAttributeValues,
			oldTupleLocks,
			oldTupleLockType,
			newTupleOid,
			newAttributeValues,
			newTupleLocks,
			newTupleLockType);
	    if (prs2CheckQual(planQual, paramList, prs2EStateInfo)) {
		/*
		 * the qualification is true. Execute the action
		 * part of the rule.
		 */
		prs2RunActionPlans(planActions, paramList, prs2EStateInfo);
		if (prs2IsRuleInsteadFromRuleInfo(ruleInfo)) {
		    *insteadRuleFoundP = true;
		}
	    }
	}
    }/*for*/
}
