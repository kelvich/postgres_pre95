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
prs2Retrieve(prs2EStateInfo, explainRelation, tuple, buffer, attributeArray,
	    numberOfAttributes, relation, returnedTupleP, returnedBufferP)
Prs2EStateInfo prs2EStateInfo;
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
     * XXX: Hmm... for the time being only use the locks
     * that are on the RelationRelation...
     * XXX: note this only happens in a retrieve operation.
     * in delete/replce/append we only use the locks stored in
     * the tuple. So, this routine must put the right locks
     * back to the tuple returned.
     */
    /*
    locksInTuple = prs2GetLocksFromTuple(tuple, buffer,
				RelationGetTupleDescriptor(relation));
    */
    relName = & ((RelationGetRelationTupleForm(relation))->relname);
    locks = prs2GetLocksFromRelation(relName);

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
    attributeValuesFree(attrValues);

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
