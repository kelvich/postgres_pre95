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
#include "rules/prs2stub.h"
#include "nodes/execnodes.h"	/* which includes access/rulescan.h */
#include "parser/parse.h"	/* for the APPEND/DELETE */

/*-------------------------------------------------------------------
 *
 * prs2Replace
 */
Prs2Status
prs2Replace(prs2EStateInfo, relationRuleInfo, explainRelation,
	    oldTuple, oldBuffer,
	    newTuple, newBuffer, rawTuple, rawBuffer,
	    attributeArray, numberOfAttributes, relation,
	    returnedTupleP, returnedBufferP)
Prs2EStateInfo prs2EStateInfo;
RelationRuleInfo relationRuleInfo;
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
    RuleLock oldLocks, oldTupleLocks, explocks;
    RuleLock newLocks;
    RuleLock relLocks;
    Prs2OneLock oneLock;
    int i,j;
    AttributeValues oldAttrValues, newAttrValues, rawAttrValues;
    AttributeNumber attr, maxattrs, attrno;
    int newTupleMade;
    Name relName;
    bool insteadRuleFound;
    bool insteadRuleFoundThisTime;
    TupleDescriptor tupDesc;

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
    tupDesc = RelationGetTupleDescriptor(relation);

    relLocks = relationRuleInfo->relationLocks;
    oldTupleLocks = prs2GetLocksFromTuple(rawTuple, rawBuffer,
			    RelationGetTupleDescriptor(relation));
    oldLocks = prs2LockUnion(oldTupleLocks, relLocks);

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
     * 'LockTypeReplaceWrite' locks...
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
		    LockTypeRetrieveWrite,
		    newTuple->t_oid,
		    newAttrValues,
		    oldLocks,
		    LockTypeReplaceWrite,
		    attributeArray,
		    numberOfAttributes);
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
		    explainRelation,
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
     * Look also for 'on replace' rules with
     * an 'InvalidAttributeNumber' locked attrno.
     * (I.e. rules like: 'on replace to EMP where... do...'
     */
    prs2ActivateForwardChainingRules(
		prs2EStateInfo,
		explainRelation,
		relation,
		InvalidAttributeNumber,
		LockTypeReplaceAction,
		PRS2_OLD_TUPLE,
		oldTuple->t_oid,
		oldAttrValues,
		oldLocks,
		LockTypeRetrieveWrite,
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
     * are there any export locks "broken" ???
     * That includes
     * a) new export locks that didn't exist before
     * b) removal of old export locks
     * c) value of an attribute locked by an export lock has changed.
     */
    explocks = prs2FindNewExportLocksFromLocks(newLocks);
    for (i=0; i<prs2GetNumberOfLocks(explocks); i++) {
	oneLock = prs2GetOneLockFromLocks(explocks, i);
	if (!prs2OneLockIsMemberOfLocks(oneLock, oldLocks)) {
	    /*
	     * a brand new export lock !
	     */
	    attrno = prs2OneLockGetAttributeNumber(oneLock);
	    if (!newAttrValues[attrno-1].isNull)
		prs2ActivateExportLockRulePlan(oneLock,
				newAttrValues[attrno-1].value,
				tupDesc->data[attrno-1]->atttypid,
				APPEND);

	}
    }
    for (i=0; i<prs2GetNumberOfLocks(oldLocks); i++) {
	oneLock = prs2GetOneLockFromLocks(oldLocks, i);
	if (prs2OneLockGetLockType(oneLock) != LockTypeExport)
	    continue;
	if (!prs2OneLockIsMemberOfLocks(oneLock, oldLocks)) {
	    /*
	     * an old lock that has been deleted...
	     */
	    attrno = prs2OneLockGetAttributeNumber(oneLock);
	    if (!oldAttrValues[attrno-1].isNull)
		prs2ActivateExportLockRulePlan(oneLock,
				oldAttrValues[attrno-1].value,
				tupDesc->data[attrno-1]->atttypid,
				DELETE);
	}
    }
    for (i=0; i<prs2GetNumberOfLocks(explocks); i++) {
	oneLock = prs2GetOneLockFromLocks(explocks, i);
	if (prs2OneLockIsMemberOfLocks(oneLock, oldLocks)) {
	    /*
	     * this lock existed before & still exists now...
	     * has the locked atribute changed in any way ??
	     * (either by the user or by the rule ?)
	     */
	    attrno = prs2OneLockGetAttributeNumber(oneLock);
	    for (j=0; j<numberOfAttributes; j++) {
		    if (attributeArray[j] == attrno)
			break;
	    }
	    if (newAttrValues[attrno-1].isChanged || j <numberOfAttributes) {
		/*
		 * yes, this attribute values has been changed...
		 */
		if (!oldAttrValues[attrno-1].isNull)
		    prs2ActivateExportLockRulePlan(oneLock,
				    oldAttrValues[attrno-1].value, DELETE);
		if (!newAttrValues[attrno-1].isNull)
		    prs2ActivateExportLockRulePlan(oneLock,
				    newAttrValues[attrno-1].value,
				    tupDesc->data[attrno-1]->atttypid,
				    APPEND);
	    }
	}
    }
	    
    
    /*
     * clean up, sweep the floor and do the laundry....
     */
    prs2FreeLocks(oldLocks);
    prs2FreeLocks(oldTupleLocks);
    attributeValuesFree(newAttrValues, relation);
    attributeValuesFree(oldAttrValues, relation);

    if (insteadRuleFound) {
	return(PRS2_STATUS_INSTEAD);
    } else {
	return(PRS2_STATUS_TUPLE_CHANGED);
    }

}
