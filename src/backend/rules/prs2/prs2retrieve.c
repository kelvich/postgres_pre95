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
#include "access/rulescan.h"

/*-------------------------------------------------------------------
 *
 * prs2Retrieve
 */
Prs2Status
prs2Retrieve(prs2EStateInfo, scanStateRuleInfo,
	    explainRelation, tuple, buffer, attributeArray,
	    numberOfAttributes, relation, returnedTupleP, returnedBufferP)
Prs2EStateInfo prs2EStateInfo;
ScanStateRuleInfo scanStateRuleInfo;
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
     * have also copied them (for efficiency) in 'scanStateRuleInfo'
     * when initializing the plan.
     *
     * XXX: note this only happens in a retrieve operation.
     * in delete/replce/append we only use the locks stored in
     * the tuple. So, this routine must put the right locks
     * back to the tuple returned.
     */
    locksInTuple = prs2GetLocksFromTuple(tuple, buffer,
				RelationGetTupleDescriptor(relation));
    if (scanStateRuleInfo == NULL) {
	/*
	 * this should not happen!
	 */
	elog(WARN,"prs2Retrieve: scanStateRuleInfop==NULL!");
    }
    locksInRelation = scanStateRuleInfo->relationLocks;
    locks = prs2LockUnion(locksInRelation, locksInTuple);

    /*
     * If there are no rules, then return immediatelly...
     */
    if (locks == NULL || prs2GetNumberOfLocks(locks)==0) {
	prs2FreeLocks(locksInTuple);
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
		    LockTypeTupleRetrieveWrite,
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
		    LockTypeTupleRetrieveAction,
		    PRS2_OLD_TUPLE,
		    tuple->t_oid,
		    attrValues,
		    locks,
		    LockTypeTupleRetrieveWrite,
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
				attrValues, locks, LockTypeTupleRetrieveWrite,
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
	HeapTupleSetRuleLock(*returnedTupleP, InvalidBuffer, locksInTuple);
	return(PRS2_STATUS_TUPLE_CHANGED);
    } else {

	/*
	 * This is a copy - don't need if we didn't form tuple.
	 */

	prs2FreeLocks(locksInTuple);
	return(PRS2_STATUS_TUPLE_UNCHANGED);
    }
}
