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
    RuleLock newLocks, newTupleLocks;
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
     * To find the locks of the old tuple, we have to check both
     * for relation level locks and for tuple level locks.
     *
     * For the "new" tuple, we have to check the relation level locks
     * and the locks from the rule stubs which are put in the tuple
     * itself by the executor.
     */
    relName = & ((RelationGetRelationTupleForm(relation))->relname);

    relLocks = prs2GetLocksFromRelation(relName);
    newTupleLocks = prs2GetLocksFromTuple(newTuple, newBuffer,
			    RelationGetTupleDescriptor(relation));
    oldTupleLocks = prs2GetLocksFromTuple(oldTuple, oldBuffer,
			    RelationGetTupleDescriptor(relation));
    oldLocks = prs2LockUnion(oldTupleLocks, relLocks);
    newLocks = prs2LockUnion(newTupleLocks, relLocks);

    /*
     * now extract from the tuple the array of the attribute values.
     */
    newAttrValues = attributeValuesCreate(newTuple, newBuffer, relation);
    oldAttrValues = attributeValuesCreate(oldTuple, oldBuffer, relation);
    rawAttrValues = attributeValuesCreate(rawTuple, rawBuffer, relation);

    /*
     * First activate all the 'late evaluation' rules, i.e.
     * all the rules that try to update the fields of the 'new' tuple
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
		    newLocks,
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
		    newLocks,
		    LockTypeTupleRetrieveWrite,
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
    HeapTupleSetRuleLock(*returnedTupleP, InvalidBuffer, newTupleLocks);

    prs2FreeLocks(newLocks);
    prs2FreeLocks(oldLocks);
    attributeValuesFree(newAttrValues, relation);
    attributeValuesFree(oldAttrValues, relation);

    if (insteadRuleFound) {
	return(PRS2_STATUS_INSTEAD);
    } else {
	return(PRS2_STATUS_TUPLE_CHANGED);
    }

}
