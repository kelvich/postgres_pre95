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


/*-------------------------------------------------------------------
 *
 * prs2Append
 */
Prs2Status
prs2Append(prs2EStateInfo, explainRelation, tuple, buffer, relation,
	    returnedTupleP, returnedBufferP)
Prs2EStateInfo prs2EStateInfo;
Relation explainRelation;
HeapTuple tuple;
Buffer buffer;
Relation relation;
HeapTuple *returnedTupleP;
Buffer *returnedBufferP;
{
    RuleLock locks, locksInTuple, locksInRelation;
    AttributeValues attrValues;
    AttributeNumber numberOfAttributes, attrNo;
    int newTupleMade;
    bool insteadRuleFound;
    Name relName;

    /*
     * Find the locks of the tuple.
     * Use both the locks stored in the tuple
     * and the locks found in the RelationRelation
     */
    locksInTuple = prs2GetLocksFromTuple (tuple, buffer, 
                                RelationGetTupleDescriptor(relation));
    relName = & ((RelationGetRelationTupleForm(relation))->relname);
    locksInRelation = prs2GetLocksFromRelation(relName);
    locks = prs2LockUnion(locksInTuple, locksInRelation);
    prs2FreeLocks(locksInTuple);
    prs2FreeLocks(locksInRelation);

    if (locks == NULL || prs2GetNumberOfLocks(locks)==0) {
	return(PRS2_STATUS_TUPLE_UNCHANGED);
    }

    /*
     * now extract from the tuple the array of the attribute values.
     */
    attrValues = attributeValuesCreate(tuple, buffer, relation);

    /*
     * first calculate all the attributes of the tuple.
     * I.e. see if there is any rule that will calculate a new
     * value for a tuple's attribute (i.e. a backward chaining rule).
     */
    numberOfAttributes = RelationGetNumberOfAttributes(relation);
    for (attrNo=1; attrNo <= numberOfAttributes; attrNo++) {
	prs2ActivateBackwardChainingRules(
		prs2EStateInfo,
		explainRelation,
		relation,
		attrNo,
		PRS2_NEW_TUPLE,
		InvalidObjectId,
		InvalidAttributeValues,
		InvalidRuleLock,
		LockTypeInvalid,
		tuple->t_oid,
		attrValues,
		locks,
		LockTypeAppendWrite,
		(AttributeNumberPtr) NULL,
		(AttributeNumber) 0);
    }

    /*
     * now, try to activate all other 'on append' rules.
     * (these ones will have a LockTypeAppendAction lock ....)
     * NOTE: the attribute number storde in the locks for these
     * rules must be `InvalidAttributeNumber' because they
     * do not refer to any attribute in particular.
     */
    prs2ActivateForwardChainingRules(
	    prs2EStateInfo,
	    explainRelation,
	    relation,
	    InvalidAttributeNumber,
	    LockTypeAppendAction,
	    PRS2_NEW_TUPLE,
	    InvalidObjectId,
	    InvalidAttributeValues,
	    InvalidRuleLock,
	    LockTypeInvalid,
	    tuple->t_oid,
	    attrValues,
	    locks,
	    LockTypeAppendWrite,
	    &insteadRuleFound,
	    (AttributeNumberPtr) NULL,
	    (AttributeNumber) 0);

    /*
     * Now all the correct values for the new tuple
     * (the ones proposed by the user + the ones calculated
     * by rule with 'LockTypeAppendWrite' locks,
     * are in the attrValues array.
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
