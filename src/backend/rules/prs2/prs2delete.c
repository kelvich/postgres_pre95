/*
 * FILE:
 *   prs2delete.c
 *
 * IDENTIFICATION:
 *   $Header$
 *
 * DESCRIPTION:
 *   The main routine is prs2Delete() which is called whenever
 *   a delete operation is performed by the executor.
 *
 */



#include "c.h"
#include "htup.h"
#include "buf.h"
#include "rel.h"
#include "prs2.h"

Prs2Status prs2Delete() { }

#ifdef NOT_YET
/*------------------------------------------------------------------
 *
 * prs2Delete()
 *
 * Activate any rules that apply to the deleted tuple
 *
 * Arguments:
 *      prs2EStateInfo: loop detection information stored in the EState
 *	tuple: the tuple to be deleted
 *	buffer:	the buffer for 'tuple'
 *	relation: the relation where 'tuple' belongs
 * Return value:
 *	PRS2MGR_INSTEAD: do not perform the delete
 *      PRS2MGR_TUPLE_UNCHANGED: Ok, go ahead & delete the tuple
 */
Prs2Status
prs2Delete(prs2EStateInfo, tuple, buffer, relation)
Prs2EStateInfo prs2EStateInfo;
HeapTuple tuple;
Buffer buffer;
Relation relation;
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

    nlocks = prs2GetNumberOfLocks(locks);
    for (i=0; i<nlocks; i++) {
	oneLock = prs2GetOneLockFromLocks(locks, i);
	if (prs2OneLockGetLockType == LockTypeDelete) {
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
