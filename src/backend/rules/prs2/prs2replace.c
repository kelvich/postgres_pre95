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

#include "c.h"
#include "log.h"
#include "prs2.h"

/*-------------------------------------------------------------------
 *
 * prs2Replace
 */
Prs2Status
prs2Replace(prs2EStateInfo, oldTuple, oldBuffer, newTuple, newBuffer,
	    attributeArray, numberOfAttributes, relation,
	    returnedTupleP, returnedBufferP)
Prs2EStateInfo prs2EStateInfo;
HeapTuple oldTuple;
Buffer oldBuffer;
HeapTuple newTuple;
Buffer newBuffer;
AttributeNumberPtr attributeArray;
int numberOfAttributes;
Relation relation;
HeapTuple *returnedTupleP;
Buffer *returnedBufferP;
{
    return(PRS2_STATUS_TUPLE_UNCHANGED);
}


#ifdef NOT_YET
/*-------------------------------------------------------------------
 *
 * prs2Replace
 */
Prs2Status
prs2Replace(prs2EStateInfo, oldTuple, oldBuffer, newTuple, newBuffer,
	    attributeArray, numberOfAttributes, relation,
	    returnedTupleP, returnedBufferP)
Prs2EStateInfo prs2EStateInfo;
HeapTuple oldTuple;
Buffer oldBuffer;
HeapTuple newTuple;
Buffer newBuffer;
AttributeNumberPtr attributeArray;
int numberOfAttributes;
Relation relation;
HeapTuple *returnedTupleP;
Buffer *returnedBufferP;
{
    RuleLock oldLocks, newLocks;
    int i;
    AttributeValues oldAttrValues, newAttrValues;
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
    oldLocks = prs2GetLocksFromTuple(oldTuple, oldBuffer,
			    RelationGetTupleDescriptor(relation));

    /*
     * now extract from the tuple the array of the attribute values.
     */
    newAttrValues = attributeValuesCreate(newTuple, newBuffer, relation);
    oldAttrValues = attributeValuesCreate(oldTuple, oldBuffer, relation);

    /*
     * First activate all the 'late evaluation' rules, i.e.
     * all the rules that try to update the fields of the 'new' tuple
     */
    for(i=0; i<numberOfAttributes; i++) {
	prs2ActivateBackwardChainingRules(
		    prs2EStateInfo,
		    relation,
		    attributeArray[i],
		    PRS2_NEW_TUPLE,
		    newTuple->t_oid,
		    newAttrValues,
		    newLocks,
		    LockTypeReplaceWrite,
		    oldTuple->t_oid,
		    oldAttrValues,
		    oldLocks,
		    LockTypeRetrieveWrite);
    }

    /*
     * Then, activate all the 'forward chaining rules'
     * NOTE: Both old & new locks have the same "LockTypeReplaceAction"
     * locks
     */
    insteadRuleFound = false;
    for(i=0; i<numberOfAttributes; i++) {
	prs2ActivateForwardChainingRules(
		    prs2EStateInfo,
		    relation,
		    attributeArray[i],
		    LockTypeReplaceAction,
		    PRS2_OLD_TUPLE,
		    oldTuple->t_oid,
		    oldAttrValues,
		    oldLocks,
		    LockTypeRetrieveWrite,
		    newTuple->t_oid,
		    newAttrValues,
		    newLocks,
		    LockTypeRetrieveWrite,
		    LockTypeInvalid,
		    &insteadRuleFoundThisTime);
	if (insteadRuleFoundThisTime) {
	    insteadRuleFound = true;
	}
    }

    /*
     * Now all the correct values 
     * Create a new tuple with the correct values.
     */
    newTupleMade = attributeValuesMakeNewTuple(
				newTuple, newBuffer,
				newAttrValues, newLocks, LockTypeInvalid,
				relation, returnedTupleP);

    prs2FreeLocks(newLocks);
    prs2FreeLocks(oldLocks);
    attributeValuesFree(newAttrValues);
    attributeValuesFree(oldAttrValues);

    if (insteadRuleFound) {
	return(PRS2_STATUS_INSTEAD);
    }

    if (newTupleMade) {
	*returnedBufferP = InvalidBuffer;
	return(PRS2_STATUS_TUPLE_CHANGED);
    } else {
	return(PRS2_STATUS_TUPLE_UNCHANGED);
    }
}
#endif NOT_YET
