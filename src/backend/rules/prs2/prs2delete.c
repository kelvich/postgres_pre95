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



#include "tmp/c.h"
#include "access/htup.h"
#include "storage/buf.h"
#include "utils/rel.h"
#include "rules/prs2.h"
#include "parser/parse.h"	/* for the DELETE */


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
 *------------------------------------------------------------------
 */
Prs2Status
prs2Delete(prs2EStateInfo, explainRelation, tuple, buffer,
			rawTuple, rawBuffer, relation)
Prs2EStateInfo prs2EStateInfo;
Relation explainRelation;
HeapTuple tuple;
Buffer buffer;
HeapTuple rawTuple;
Buffer rawBuffer;
Relation relation;
{

    RuleLock locks, l1, l2;
    AttributeValues attrValues;
    bool insteadRuleFound;
    Name relName;
    TupleDescriptor tupDesc;
    int i;

    /*
     * Find the locks of the tuple.
     *
     * NOTE: the 'rawTuple' is the tuple as retrieved
     * from the disk, so it has all required lock information in
     * it (as opposed to tuples formed when executing the target
     * lists of the various executor nodes).
     * On the other hand, 'tuple' is a result of projections,
     * etc, identical to 'rawTuple' with only 2 differences:
     * a) no rule locks
     * b) all "on retrieve" backward chaining rules have been activated
     */
    relName = RelationGetRelationName(relation);
    tupDesc = RelationGetTupleDescriptor(relation);

    l1 = prs2GetLocksFromTuple (rawTuple, rawBuffer,
			RelationGetTupleDescriptor(relation));
    l2 = prs2GetLocksFromRelation(relName);
    locks = prs2LockUnion(l1, l2);
    prs2FreeLocks(l1);
    prs2FreeLocks(l2);

    /*
     * if there are no rules, then return immediatelly
     */
    if (locks == NULL || prs2GetNumberOfLocks(locks) == 0) {
	return(PRS2_STATUS_TUPLE_UNCHANGED);
    }

    /*
     * Ooops! There is a problem here... 'tuple' is set to 'nil'
     * by the executor, so for the time being use the 'rawTuple'
     * (the result is that we might have to re-evaluate some backward
     * chaining rules...)
     */
    tuple = rawTuple;
    buffer = rawBuffer;

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
     * if this tuple contains any 'export' locks, then
     * activate the appropriate rule plans...
     */
    for (i=0; i<prs2GetNumberOfLocks(locks); i++) {
	Prs2OneLock oneLock;
	AttributeNumber attrno;

	oneLock = prs2GetOneLockFromLocks(locks, i);
	if (prs2OneLockGetLockType(oneLock)  == LockTypeExport) {
	    attrno = prs2OneLockGetAttributeNumber(oneLock);
	    if (!attrValues[attrno-1].isNull)
		prs2ActivateExportLockRulePlan(oneLock,
				attrValues[attrno-1].value,
				tupDesc->data[attrno-1]->atttypid,
				DELETE);
	}
    }
    
    /*
     * free allocated stuff...
     */
    prs2FreeLocks(locks);
    attributeValuesFree(attrValues, relation);

    /*
     * Was this rule an 'instead' rule ???
     */
    if (insteadRuleFound) {
	return(PRS2_STATUS_INSTEAD);
    } else {
	return(PRS2_STATUS_TUPLE_UNCHANGED);
    }
}
