/*===================================================================
 *
 * FILE:
 *   prs2append.c
 *
 * IDENTIFICATION:
 *   $Header$
 *
 * DESCRIPTION:
 *
 *
 *===================================================================
 */

#include "c.h"
#include "htup.h"
#include "buf.h"
#include "rel.h"
#include "log.h"
#include "prs2.h"


Prs2Status prs2Append() { };

#ifdef NOT_YET
/*-------------------------------------------------------------------
 *
 * prs2Append
 */
Prs2Status
prs2Append(prs2EStateInfo, tuple, buffer, relation,
	    returnedTupleP, returnedBufferP)
Prs2EStateInfo prs2EStateInfo;
HeapTuple tuple;
Buffer buffer;
Relation relation;
HeapTuple *returnedTupleP;
Buffer *returnedBufferP;
{
    RuleLock locks;
    Prs2OneLocks oneLock;
    int nlocks;
    int i;
    int status;
    LispDisplay plan;
    LispDisplay ruleInfo;
    LispDisplay planQual;
    LispDisplay planAction;
    ParamListInfo paramList;
    AttributeValues attrValues;
    AttributeNumber numberOfAttributes;
    AttributeNumber attrNo;

    /*
     * Find the locks of the tuple.
     * NOTE: Do not search the 'RelationRelation' for any
     * locks! The reason is that the right locks have already
     * been put to this tuple by the rule manager when it was
     * called as part of a retrieve operation in the lower
     * scan nodes opf the plan.
     */
    locks = prs2GetLocksFromTuple (tuple, buffer, &(relation->rd_att));

    /*
     * now extract from the tuple the array of the attribute values.
     */
    attrValues = attributeValuesCreate(tuple, buffer, relation);

    /*
     * first calculate all the attributes of the tuple.
     * I.e. see if there is any rule that will calculate a new
     * value for a tuple's attribute.
     * NOTE: these rules will have a LockTypeWrite.
     */
    numberOfAttributes = RelationGetNumberOfAttributes(relation);
    for (attrNo=1; attrNo <= numberOfAttributes; attrNo++) {
	prs2CalculateOneAttribute(
	    prs2EStateInfo,
	    relation,
	    tuple->t_oid,
	    attrNo,
	    locks,
	    attrValues);
    }

    /*
     * now, try to activate all other 'on append' rules.
     * (these ones will have a LockTypeOnAppend lock ....)
     */
    nlocks = prs2GetNumberOfLocks(locks);
    for (i=0; i<nlocks; i++) {
	oneLock = prs2GetOneLockFromLocks(locks, i);
	if (prs2OneLockGetLockType(oneLock) == LockTypeOnAppend) {
	    /*
	     * We've found a lock. Go in the system catalogs and
	     * extract the appropriate plan info...
	     */
	    plan = prs2GetRulePlanFromCatalog(
				prs2OneLockGetRuleId(oneLock),
				prs2OneLockGetPlanNumber(oneLock),
				&paramList);
	    ruleInfo = prs2GetRuleInfoFromRulePlan(plan);
	    planQual = prs2GetQualFromRulePlan(plan);
	    planAction = prs2GetActionsFromRulePlan(plan);

	    /*
	     * Now calculate all the parameters needed to fill the plan
	     */
            prs2CalculateAttributesOfParamNodes(
                        prs2EStateInfo,
                        relation, tuple-t_oid,
                        locks,
                        paramList,
                        attributeValues);
	    /*
	     * Check for the plan qualification
	     */
	    if (prs2CheckQual(planQual, paramList, prs2EStateInfo)) {
		/*
		 * the qualifcation is true. Activate the rule.
		 */
		prs2RunActionPlans(planAction, paramList, prs2EStateInfo);
	    }
	}
    }/*for*/
	
    prs2FreeLocks(locks);
    attributeValuesFree(attrValues);

    /*
     * Was this rule an 'instead' rule ???
     */
    if (prs2IsRuleInsteadFromRuleInfo(ruleInfo)) {
	return(PRS2_STATUS_INSTEAD);
    } else {
	return(PRS2_STATUS_TUPLE_UNCHANGED);
    }
}

#endif NOT_YET
