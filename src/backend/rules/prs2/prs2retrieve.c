/*===================================================================
 *
 * FILE:
 *   prs2retrieve.c
 *
 * IDENTIFICATION:
 *   $Header$
 *
 * DESCRIPTION:
 *   The main routine is prs2Retrieve() which is called whenever
 *   a retrieve operation is performed by the executor.
 *
 *
 *===================================================================
 */

#include "c.h"
#include "log.h"
#include "prs2.h"


/*-------------------------------------------------------------------
 *
 * prs2Retrieve
 */
Prs2Status
prs2Retrieve(prs2EStateInfo, tuple, buffer, attributeArray,
	    numberOfAttributes, relation, returnedTupleP, returnedBufferP)
Prs2EStateInfo prs2EStateInfo;
HeapTuple tuple;
Buffer buffer;
AttributeNumberPtr attributeArray;
int numberOfAttributes;
Relation relation;
HeapTuple *returnedTupleP;
Buffer *returnedBufferP;
{
    Prs2Locks locks, locksInTuple, locksInRelation;
    int i;
    AttributeValues attrValues;
    int newTupleMade;
    Name relName;

    /*
     * Find the locks of the tuple.
     * Use both the locks stored in the tuple
     * and the locks found in the RelationRelation
     * XXX: note this only happens in a retrieve operation.
     * in delete/replce/append we only use the locks stored in
     * the tuple. So, this routine must put the right locks
     * back to the tuple returned.
     */
    locksInTuple = prs2GetLocksFromTuple(tuple, buffer,
				RelationGetTupleDescriptor(relation));
    relName = & ((RelationGetRelationTupleForm(relation))->relname);
    locksInRelation = prs2GetLocksFromRelation(relName);
    locks = prs2LockUnion(locksInTuple, locksInRelation);
    prs2FreeLocks(locksInTuple);
    prs2FreeLocks(locksInRelation);

    /*
     * now extract from the tuple the array of the attribute values.
     */
    attrValues = attributeValuesCreate(tuple, buffer, relation);

    /*
     * First activate all the 'late evaluation' rules, i.e.
     * all the rules that try to update the fields to be retrieved.
     */
    for(i=0; i<numberOfAttributes; i++) {
	prs2CalculateOneAttribute(
			prs2EStateInfo,
			relation,
			tuple->t_oid,
			attributeArray[i],
			locks,
			attrValues);
    }

    /*
     * Now all the correct values are in the attrValues array.
     * Create a new tuple with the correct values.
     */
    newTupleMade = attributeValuesMakeNewTuple(
				tuple, buffer,
				attrValues,
				locks, relation, returnedTupleP);

    prs2FreeLocks(locks);
    attributeValuesFree(attrValues);

    if (newTupleMade) {
	*returnedBufferP = InvalidBuffer;
	return(PRS2_STATUS_TUPLE_CHANGED);
    } else {
	return(PRS2_STATUS_TUPLE_UNCHANGED);
    }
}


/*-------------------------------------------------------------------
 * prs2CalculateOneAttribute()
 *
 */
void
prs2CalculateOneAttribute(
	    prs2EStateInfo,
	    relation, tupleOid,
	    attributeNumber, locks,
	    attributeValues)
Prs2EStateInfo prs2EStateInfo;
Relation relation;
ObjectId tupleOid;
AttributeNumber attributeNumber;
Prs2Locks locks;
AttributeValues attributeValues;
{
    Prs2OneLock oneLock;
    int nlocks;
    int i;
    int status;
    int finalStatus;
    LispValue plan;
    LispValue planInfo;
    LispValue planQual;
    LispValue planAction;
    ParamListInfo paramList;
    AttributeNumber lockAttrNo;
    Prs2LockType lockType;
    ObjectId ruleId;

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
        if (lockType==LockTypeWrite && attributeNumber==lockAttrNo) {
	    /*
	     * We've found a 'LockTypeWrite' lock, i.e. there
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
	    planInfo = prs2GetRuleInfoFromRulePlan(plan);
	    planQual = prs2GetQualFromRulePlan(plan);
	    planAction = prs2GetActionsFromRulePlan(plan);

	    /*
	     * Now try to calculate all the parameters needed to fill in
	     * the plan
	     */
	    prs2CalculateAttributesOfParamNodes(
			prs2EStateInfo,
			relation, tupleOid,
			locks,
			paramList,
			attributeValues);
		    
	    if (planQual == LispNil ||
			prs2CheckQual(planQual, paramList, prs2EStateInfo)) {
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
	}else if (lockType==LockTypeOnRetrieve && attributeNumber==lockAttrNo){
	    /*
	     * A 'on retrieve' rule was found, which does not update
	     * the returned tuple. Execute it.
	     */
	    plan = prs2GetRulePlanFromCatalog(
				prs2OneLockGetRuleId(oneLock),
				prs2OneLockGetPlanNumber(oneLock),
				&paramList);
	    planInfo = prs2GetRuleInfoFromRulePlan(plan);
	    planQual = prs2GetQualFromRulePlan(plan);
	    planAction = prs2GetActionsFromRulePlan(plan);

	    /*
	     * Now try to calculate all the parameters needed to fill in
	     * the plan
	     */
	    prs2CalculateAttributesOfParamNodes(
			prs2EStateInfo,
			relation, tupleOid,
			locks,
			paramList,
			attributeValues);
		    
	    if (planQual == LispNil || prs2CheckQual(planQual, paramList)) {
		/*
		 * the qualification is true. Execute the action
		 * part of the rule.
		 */
		prs2RunActionPlans(planAction, paramList, prs2EStateInfo);
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

}

/*----------------------------------------------------------------------
 *
 * prs2CalculateAttributesOfParamNodes
 *
 */

void
prs2CalculateAttributesOfParamNodes(prs2EStateInfo, relation, tupleOid,
				    ruleLocks, paramList, attributeValues)
Prs2EStateInfo prs2EStateInfo;
Relation relation;
ObjectId tupleOid;
Prs2Locks ruleLocks;
ParamListInfo paramList;
AttributeValues attributeValues;
{

    int status;
    int finalStatus;
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
	/*
	 * XXX Hm...
	 */
	if (paramList[i].kind != PARAM_OLD) {
	    elog(WARN,"param kind = (%d) != PARAM_OLD, name = %s",
	    paramList[i].kind, paramList[i].name);
	}
	/*
	 * make sure that the value stored in the tuple
	 * is the correct one
	 */
	prs2CalculateOneAttribute(
		    prs2EStateInfo,
		    relation, tupleOid,
		    attributeNumber,
		    ruleLocks, 
		    attributeValues);
	/*
	 * store information about this attribute (type, value etc)
	 * in the paramList
	 */
	paramList[i].type = get_atttype(relation->rd_id, attributeNumber);
	paramList[i].length = (Size) get_typlen(paramList[i].type);
	paramList[i].value = attributeValues[attributeNumber-1].value;
	if (attributeValues[attributeNumber].isNull)
	    paramList[i].isnull = true;
	else
	    paramList[i].isnull = false;
	
	/*
	 * Continue with the next parameter
	 */
	i += 1;
    } /*while*/
}

