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

#include "tmp/c.h"
#include "utils/log.h"
#include "rules/prs2.h"
#include "nodes/execnodes.h"	/* which includes access/rulescan.h */

/*-------------------------------------------------------------------
 *
 * prs2Retrieve
 */
Prs2Status
prs2Retrieve(prs2EStateInfo, relationRuleInfo,
	    explainRelation, tuple, buffer, attributeArray,
	    numberOfAttributes, relation, returnedTupleP, returnedBufferP)
Prs2EStateInfo prs2EStateInfo;
RelationRuleInfo relationRuleInfo;
Relation explainRelation;
HeapTuple tuple;
Buffer buffer;
AttributeNumberPtr attributeArray;
int numberOfAttributes;
Relation relation;
HeapTuple *returnedTupleP;
Buffer *returnedBufferP;
{
    RuleLock locks, locksInTuple, locksInRelation;
    int i;
    AttributeValues attrValues;
    int newTupleMade;
    Name relName;
    bool insteadRuleFound;
    bool insteadRuleFoundThisTime;


    /*
     * Find the locks of the tuple.
     *
     * There are 2 places to look for locks. The "tuple-level"
     * locks are stored in the tuple, and the "relation-level"
     * locks are stored in the PG_RELATION relation, but we
     * have also copied them (for efficiency) in 'relationRuleInfo'
     * when initializing the plan.
     *
     * BTW, ignore the tuple locks if the appropriate flag
     * is set in 'relationRuleInfo'. This happens when we scan
     * "pg_class".
     */
    locksInRelation = relationRuleInfo->relationLocks;
    if (relationRuleInfo->ignoreTupleLocks) {
	locksInTuple = prs2MakeLocks();
    } else {
	locksInTuple = prs2GetLocksFromTuple(tuple, buffer);
    }
    locks = prs2LockUnion(locksInRelation, locksInTuple);
    prs2FreeLocks(locksInTuple);

    /*
     * If there are no rules, then return immediatelly...
     */
    if (prs2GetNumberOfLocks(locks)==0) {
	prs2FreeLocks(locks);
	return(PRS2_STATUS_TUPLE_UNCHANGED);
    }

    /*
     * now extract from the tuple the array of the attribute values.
     */
    attrValues = attributeValuesCreate(tuple, buffer, relation);

    /*
     * First activate all the 'late evaluation' rules, i.e.
     * all the rules that try to update the fields to be retrieved.
     */
    for(i=0; i<numberOfAttributes; i++) {
	prs2ActivateBackwardChainingRules(
		    prs2EStateInfo,
		    explainRelation,
		    relation,
		    attributeArray[i],
		    PRS2_OLD_TUPLE,
		    tuple->t_oid,
		    attrValues,
		    locks,
		    LockTypeRetrieveWrite,
		    InvalidObjectId,
		    InvalidAttributeValues,
		    InvalidRuleLock,
		    LockTypeInvalid,
		    attributeArray,
		    numberOfAttributes);
    }

    /*
     * Then, activate all the 'forward chaining rules'
     */
    insteadRuleFound = false;
    for(i=0; i<numberOfAttributes; i++) {
	prs2ActivateForwardChainingRules(
		    prs2EStateInfo,
		    explainRelation,
		    relation,
		    attributeArray[i],
		    LockTypeRetrieveAction,
		    PRS2_OLD_TUPLE,
		    tuple->t_oid,
		    attrValues,
		    locks,
		    LockTypeRetrieveWrite,
		    InvalidObjectId,
		    InvalidAttributeValues,
		    InvalidRuleLock,
		    LockTypeInvalid,
		    &insteadRuleFoundThisTime,
		    attributeArray,
		    numberOfAttributes);
	if (insteadRuleFoundThisTime) {
	    insteadRuleFound = true;
	}
    }

    /*
     * Now all the correct values are in the attrValues array.
     * Create a new tuple with the correct values.
     */
    newTupleMade = attributeValuesMakeNewTuple(
				tuple, buffer,
				attrValues, locks, LockTypeRetrieveWrite,
				relation, returnedTupleP);

    prs2FreeLocks(locks);

    attributeValuesFree(attrValues, relation);

    if (insteadRuleFound) {
	return(PRS2_STATUS_INSTEAD);
    }

    if (newTupleMade) {
	*returnedBufferP = InvalidBuffer;
	/*
	 * The new tuple must have the same locks as the old
	 * tuple (in case we are retrieving the "lock" attribute.
	 */
	locksInTuple = prs2GetLocksFromTuple(tuple, buffer);
	HeapTupleSetRuleLock(*returnedTupleP, InvalidBuffer, locksInTuple);
	return(PRS2_STATUS_TUPLE_CHANGED);
    } else {
	return(PRS2_STATUS_TUPLE_UNCHANGED);
    }
}
