/*===================================================================
 *
 * FILE:
 *   prs2replace.c
 *
 * IDENTIFICATION:
 *   $Header$
 *
 * DESCRIPTION:
 *
 *
 *===================================================================
 */

#include "tmp/c.h"
#include "utils/log.h"
#include "rules/prs2.h"

/*-------------------------------------------------------------------
 *
 * prs2Replace
 */
Prs2Status
prs2Replace(prs2EStateInfo, explainRelation, oldTuple, oldBuffer,
	    newTuple, newBuffer, rawTuple, rawBuffer,
	    attributeArray, numberOfAttributes, relation,
	    returnedTupleP, returnedBufferP)
Prs2EStateInfo prs2EStateInfo;
Relation explainRelation;
HeapTuple oldTuple;
Buffer oldBuffer;
HeapTuple newTuple;
Buffer newBuffer;
HeapTuple rawTuple;
Buffer rawBuffer;
AttributeNumberPtr attributeArray;
int numberOfAttributes;
Relation relation;
HeapTuple *returnedTupleP;
Buffer *returnedBufferP;
{
    RuleLock oldLocks, oldTupleLocks;
    RuleLock newLocks;
    RuleLock relLocks;
    int i;
    AttributeValues oldAttrValues, newAttrValues, rawAttrValues;
    AttributeNumber attr, maxattrs;
    int newTupleMade;
    Name relName;
    bool insteadRuleFound;
    bool insteadRuleFoundThisTime;


    /*
     * Find the locks of the tuple.
     *
     * To find the locks of the RAW(old) tuple, we have to check both
     * for relation level locks and for tuple level locks.
     * 
     * NOTE: 'old' tuple has no lock on it, because locks
     * are not propagated from node to node inside the executor.
     *
     * NOTE: 'on replace' rules have either a relation level
     * lock, or a tuple-level-lock on the old tuple.
     *
     * The rule stubs need only be checked at the end, and only
     * to find out what locks the "new" tuple should get.
     *
     */
    relName = & ((RelationGetRelationTupleForm(relation))->relname);

    relLocks = prs2GetLocksFromRelation(relName);
    oldTupleLocks = prs2GetLocksFromTuple(rawTuple, rawBuffer,
			    RelationGetTupleDescriptor(relation));
    oldLocks = prs2LockUnion(oldTupleLocks, relLocks);

    /*
     * XXX HACK! HACK! HACK!
     *
     * When `old' tuple was formed, we evaluated ALL backward
     * chaining rules defined on it.
     * So, get rid of the "write" locks of on retrieve-do retrieve
     * (otherwise, we might re-activate them unnecessarily)
     */
    prs2RemoveLocksOfTypeInPlace(oldLocks, LockTypeTupleRetrieveWrite);

    /*
     * now extract from the tuple the array of the attribute values.
     */
    newAttrValues = attributeValuesCreate(newTuple, newBuffer, relation);
    oldAttrValues = attributeValuesCreate(oldTuple, oldBuffer, relation);
    rawAttrValues = attributeValuesCreate(rawTuple, rawBuffer, relation);

    /*
     * First activate all the 'late evaluation' rules, i.e.
     * all the rules that try to update the fields of the 'new' tuple
     *
     * NOTE: WE use as locks of the new tuple the locks of the old
     * tuple! But in reality we are only interested in the 
     * 'LockTypeTupleReplaceWrite' locks...
     */
    maxattrs = RelationGetNumberOfAttributes(relation);
    for(attr=1; attr <= maxattrs; attr++) {
	prs2ActivateBackwardChainingRules(
		    prs2EStateInfo,
		    explainRelation,
		    relation,
		    attr,
		    PRS2_NEW_TUPLE,
		    oldTuple->t_oid,
		    oldAttrValues,
		    oldLocks,
		    LockTypeTupleRetrieveWrite,
		    newTuple->t_oid,
		    newAttrValues,
		    oldLocks,
		    LockTypeTupleReplaceWrite,
		    attributeArray,
		    numberOfAttributes);
    }

    /*
     * Then, activate all the 'forward chaining rules'
     * NOTE: Both old & new locks have the same "LockTypeTupleReplaceAction"
     * locks
     */
    insteadRuleFound = false;
    for(i=0; i<numberOfAttributes; i++) {
	prs2ActivateForwardChainingRules(
		    prs2EStateInfo,
		    explainRelation,
		    relation,
		    attributeArray[i],
		    LockTypeTupleReplaceAction,
		    PRS2_OLD_TUPLE,
		    oldTuple->t_oid,
		    oldAttrValues,
		    oldLocks,
		    LockTypeTupleRetrieveWrite,
		    newTuple->t_oid,
		    newAttrValues,
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
     * Now all the correct values 
     * Create a new tuple combining the 'raw' tuple, i.e. the
     * tuple without any rules activated and the 'new' tuple.
     */
    *returnedBufferP = InvalidBuffer;
    *returnedTupleP = attributeValuesCombineNewAndOldTuple(
				rawAttrValues,
				newAttrValues,
				relation,
				attributeArray,
				numberOfAttributes);

    /*
     * OK, now find out the rule locks that the new tuple
     * must get.
     */
    newLocks = prs2FindLocksForNewTupleFromStubs(
		    *returnedTupleP,
		    *returnedBufferP,
		    relation);
    HeapTupleSetRuleLock(*returnedTupleP, InvalidBuffer, newLocks);

    /*
     * clean up, sweep the floor and do the laundry....
     */
    prs2FreeLocks(oldLocks);
    prs2FreeLocks(relLocks);
    prs2FreeLocks(oldTupleLocks);
    attributeValuesFree(newAttrValues, relation);
    attributeValuesFree(oldAttrValues, relation);

    if (insteadRuleFound) {
	return(PRS2_STATUS_INSTEAD);
    } else {
	return(PRS2_STATUS_TUPLE_CHANGED);
    }

}
