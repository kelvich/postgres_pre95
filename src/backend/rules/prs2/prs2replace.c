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
    RuleLock oldLocks, newLocks;
    int i;
    AttributeValues oldAttrValues, newAttrValues, rawAttrValues;
    AttributeNumber attr;
    int newTupleMade;
    Name relName;
    bool insteadRuleFound;
    bool insteadRuleFoundThisTime;


    /*
     * Find the locks of the tuple.
     *
     * XXX:
     * The "old" tuple's lock are stored in the tuple (that
     * happenned at a lower level of the plan, when prs2Retrieve
     * was called as part of the scan operation.
     * To find the locks of the "new" tuple we have to look at
     * the RelationRelation.
     */
    relName = & ((RelationGetRelationTupleForm(relation))->relname);
    newLocks = prs2GetLocksFromRelation(relName);
#ifdef NO
    oldLocks = prs2GetLocksFromTuple(oldTuple, oldBuffer,
			    RelationGetTupleDescriptor(relation));
#else
    /*
     * XXX
     * in the current implementation, "oldTuple" is the same as
     * "rawTuple", i.e. as all locks are currently "relation level"
     * locks, no locks are actually stored in the tuple!
     */
    oldLocks = prs2CopyLocks(newLocks);
#endif

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
    for(attr=1; attr <= RelationGetNumberOfAttributes(relation); attr++) {
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
