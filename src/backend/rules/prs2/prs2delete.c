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
prs2Delete(prs2EStateInfo, explainRelation, tuple, buffer, relation)
Prs2EStateInfo prs2EStateInfo;
Relation explainRelation;
HeapTuple tuple;
Buffer buffer;
Relation relation;
{

    RuleLock locks;
    AttributeValues attrValues;
    bool insteadRuleFound;

    /*
     * Find the locks of the tuple.
     * NOTE: Do not search the 'RelationRelation' for any
     * locks! The reason is that the right locks have already
     * been put to this tuple by the rule manager when it was
     * called as part of a retrieve operation in the lower
     * scan nodes opf the plan.
     */
    locks = prs2GetLocksFromTuple (tuple, buffer,
			RelationGetTupleDescriptor(relation));

    /*
     * if there are no rules, then return immediatelly
     */
    if (locks == NULL || prs2GetNumberOfLocks(locks) == 0) {
	return(PRS2_STATUS_TUPLE_UNCHANGED);
    }

    /*
     * now extract from the tuple the array of the attribute values.
     */
    attrValues = attributeValuesCreate(tuple, buffer, relation);

    /*
     * activate all 'on delete' rules...
     * NOTE: all these rules must have an attribute no equal to
     * `InvalidAttributeNumber' because they do not refer to any
     * field in particular....
     */
    prs2ActivateForwardChainingRules(
	    prs2EStateInfo,
	    explainRelation,
	    relation,
	    InvalidAttributeNumber,
	    LockTypeDeleteAction,
	    PRS2_OLD_TUPLE,
	    tuple->t_oid,
	    attrValues,
	    locks,
	    LockTypeRetrieveWrite,
	    InvalidObjectId,
	    InvalidAttributeValues,
	    InvalidRuleLock,
	    LockTypeInvalid,
	    &insteadRuleFound,
	    (AttributeNumberPtr) NULL,
	    (AttributeNumber) 0);

    /*
     * free allocated stuff...
     */
    prs2FreeLocks(locks);
    attributeValuesFree(attrValues);

    /*
     * Was this rule an 'instead' rule ???
     */
    if (insteadRuleFound) {
	return(PRS2_STATUS_INSTEAD);
    } else {
	return(PRS2_STATUS_TUPLE_UNCHANGED);
    }
}
