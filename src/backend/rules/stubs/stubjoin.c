/*=======================================================================
 *
 * FILE: stubjoin.c
 *
 * IDENTIFICATION:
 * $Header$
 *
 * Routines used when the executor runs in 'rule lock set/remove' mode.
 *
 */
#include "rules/prs2.h"
#include "rules/prs2stub.h"
#include "nodes/plannodes.h"
#include "nodes/plannodes.a.h"

/*----------------------------------------------------------------
 *
 * prs2MakeStubForInnerRelation
 *
 * Create a `PrsOneStub'. This routine is called during the proccessing
 * of a Join node. For every tuple of the Outer relation, we have to
 * create (and insert in the inner relation) a rule stub.
 *
 */
Prs2OneStub
prs2MakeStubForInnerRelation(ruleInfo, tuple, buffer, relation)
JoinRuleInfo ruleInfo;
HeapTuple tuple;
Buffer buffer;
Relation relation;
{
    Prs2OneStub oneStub;
    ObjectId ruleId;
    Prs2StubId stubId;
    RuleLock lock;
    Prs2SimpleQual qual;
    AttributeNumber innerAttrNo, outerAttrNo;
    ObjectId operator;
    Datum value;
    Boolean isNull;


    operator = get_jri_operator(ruleInfo);
    innerAttrNo = get_jri_inattrno(ruleInfo);
    outerAttrNo = get_jri_outattrno(ruleInfo);
    ruleId = get_jri_ruleid(ruleInfo);
    stubId = get_jri_stubid(ruleInfo);
    lock = get_jri_lock(ruleInfo);

    /*
     * now form the 'Prs2SimpleQual'. This will correspond to
     * the qualification:
     *    ( <operator> <innerAttrNo> <constant> )
     * where <constant> is the value of the "outerAttrno" of the
     * outer relation tuple "tuple".
     */
    value = HeapTupleGetAttributeValue(
			tuple,
			buffer,
			outerAttrNo,
			RelationGetTupleDescriptor(relation),
			&isNull);
    if (isNull) {
	/*
	 * the outer tuple has a null attribute, so do not do
	 * anything.
	 */
	return((Prs2OneStub)NULL);
    }
    qual = prs2MakeSimpleQuals(1);
    qual->attrNo = innerAttrNo;
    qual->operator = operator;
    qual->constType = get_atttype(RelationGetRelationId(relation),outerAttrNo);
    qual->constByVal = get_typbyval(qual->constType);
    qual->constLength = get_typlen(qual->constType);
    qual->constData = value;

    /*
     * OK, now form the 'Prs2OneStub'
     */
    oneStub = prs2MakeOneStub(ruleId, stubId,
				1,	/* count */
				1,	/* numQuals */
				lock,
				qual);
    return(oneStub);
}

/*----------------------------------------------------------------
 *
 * prs2AddLocksAndReplaceTuple
 *
 * Add the given lock to a tuple, if the qualification associated
 * with the `oneStub' is satisfied.
 *
 */
void
prs2AddLocksAndReplaceTuple(tuple, buffer, relation, oneStub, lock)
HeapTuple tuple;
Buffer buffer;
Relation relation;
Prs2OneStub oneStub;
RuleLock lock;
{
    RuleLock oldLocks, newLocks;
    TupleDescriptor tupDesc;
    ItemPointer tupleId;
    HeapTuple newTuple;

    tupDesc = RelationGetTupleDescriptor(relation);

    if (prs2StubTestTuple(tuple, buffer, tupDesc, oneStub)) {
	oldLocks = prs2GetLocksFromTuple(tuple, buffer,tupDesc);
	newLocks = prs2LockUnion(lock, oldLocks);
	newTuple = prs2PutLocksInTuple(tuple, buffer, relation, newLocks);

	/*
	 * XXX: what happens if this tuple has already been replaced ?
	 * Should we check with: TupleUpdatedByCurXactAndCmd() ???
	 */
	tupleId = &(tuple->t_ctid);
	amreplace(relation, tupleId, newTuple);
    }
}
	
