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

#include "tmp/c.h"
#include "access/htup.h"
#include "storage/buf.h"
#include "utils/rel.h"
#include "utils/log.h"
#include "utils/palloc.h"
#include "rules/prs2.h"
#include "rules/prs2stub.h"
#include "nodes/execnodes.h"		/* which includes access/rulescan.h */
#include "parser/parse.h"		/* for the APPEND */

extern HeapTuple palloctup();


/*-------------------------------------------------------------------
 *
 * prs2Append
 */
Prs2Status
prs2Append(prs2EStateInfo, relationRuleInfo, explainRelation,
		tuple, buffer, relation, returnedTupleP, returnedBufferP)
Prs2EStateInfo prs2EStateInfo;
RelationRuleInfo relationRuleInfo;
Relation explainRelation;
HeapTuple tuple;
Buffer buffer;
Relation relation;
HeapTuple *returnedTupleP;
Buffer *returnedBufferP;
{
    RuleLock locks, newLocks, explocks, temp;
    Prs2Stub stubs;
    AttributeValues attrValues;
    AttributeNumber numberOfAttributes, attrNo;
    bool insteadRuleFound;
    Name relName;
    Prs2Status status;
    int i;
    TupleDescriptor tupDesc;

    /*
     * NOTE: currently we only have relation-level-locks for
     * "on append ...." rules.
     * So we do not check for any tuple level locks.
     * (after all this is a brand new tuple with no locks on it)
     */
    relName = RelationGetRelationName(relation);
    tupDesc = RelationGetTupleDescriptor(relation);

    locks = relationRuleInfo->relationLocks;
    stubs = relationRuleInfo->relationStubs;

    /*
     * now extract from the tuple the array of the attribute values.
     */
    attrValues = attributeValuesCreate(tuple, buffer, relation);

    /*
     * activate 'on append' rules...
     */
    if (locks == NULL || prs2GetNumberOfLocks(locks)==0) {
	/*
	 * the trivial case: no "on append" rules!
	 */
	status = PRS2_STATUS_TUPLE_UNCHANGED;
	insteadRuleFound = false;
	*returnedTupleP = tuple;
	*returnedBufferP = buffer;
    } else {
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
	 * NOTE: the attribute number stored in the locks for these
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
	if (attributeValuesMakeNewTuple(
				    tuple, buffer,
				    attrValues, locks,
				    LockTypeRetrieveWrite,
				    relation, returnedTupleP)) {
	    status = PRS2_STATUS_TUPLE_CHANGED;
	    *returnedBufferP = InvalidBuffer;
	} else {
	    status = PRS2_STATUS_TUPLE_UNCHANGED;
	    *returnedTupleP = tuple;
	    *returnedBufferP = buffer;
	}
    }


    /*
     * OK, if there was an 'instead' rule, then we must not
     * append the tuple.
     */
    if (insteadRuleFound) {
	return(PRS2_STATUS_INSTEAD);
    }

    /*
     * well, there was no instead rule.
     * Check all the stubs to see if this tuple should
     * inherit some new locks.
     */
    newLocks = prs2FindLocksForNewTupleFromStubs(
		    *returnedTupleP,
		    *returnedBufferP,
		    stubs,
		    relation);
    /*
     * Check if any "export" lock should be added to the
     * tuple. If yes, then activate the appropriate
     * rule plans...
     */
    explocks = prs2FindNewExportLocksFromLocks(newLocks);
    for (i=0; i<prs2GetNumberOfLocks(explocks); i++) {
	Prs2OneLock oneLock;
	AttributeNumber attrno;
	oneLock = prs2GetOneLockFromLocks(explocks);
	attrno = prs2OneLockGetAttributeNumber(oneLock);
	if (!attrValues[attrno-1].isNull)
	    prs2ActivateExportLockRulePlan(oneLock,
					attrValues[attrno-1].value,
					tupDesc->data[attrno-1]->atttypid,
					APPEND);
    }

    /*
     * the locks that this tuple must get is the union of 'newLocks'
     * and 'explocks'.
     */
    temp = prs2LockUnion(newLocks, explocks);
    prs2FreeLocks(newLocks);
    prs2FreeLocks(explocks);
    newLocks = temp;

    attributeValuesFree(attrValues, relation);

    /*
     * Did the locks or an attribute of the tuple change?
     * If yes, whistle to the executor...
     */
    if (status == PRS2_STATUS_TUPLE_CHANGED) {
	if (prs2GetNumberOfLocks(newLocks) == 0) {
	    prs2FreeLocks(newLocks);
	    return(PRS2_STATUS_TUPLE_CHANGED);
	} else {
	    HeapTupleSetRuleLock(*returnedTupleP, InvalidBuffer, newLocks);
	    return(PRS2_STATUS_TUPLE_CHANGED);
	}
    } else {	/* status = PRS2_STATUS_TUPLE_UNCHANGED */
	if (prs2GetNumberOfLocks(newLocks) == 0) {
	    prs2FreeLocks(newLocks);
	    return(PRS2_STATUS_TUPLE_UNCHANGED);
	} else {
	    /*
	     * we must create a COPY of the tuple!
	     */
	    *returnedTupleP = palloctup(tuple,buffer,relation);
	    HeapTupleSetRuleLock(*returnedTupleP, InvalidBuffer, newLocks);
	    *returnedBufferP = InvalidBuffer;
	    return(PRS2_STATUS_TUPLE_CHANGED);
	}
    }
}

/*-----------------------------------------------------------------------
 * prs2FindLocksForNewTupleFromStubs
 *
 * Given a brand new tuple that will be inserted in a relation,
 * check all the rule stubs to find what tuple-level locks this tuple
 * should get.
 *
 * NOTE: the following code can be hazardus to your mental health...
 * (but if you are hacking it you have already reached the point of no
 * return, so...)
 *
 * The idea is that we have a new tuple, some stubs and we want to see
 * which of the stubs' qualifications are satisfied by the tuple.
 * So, all we need is to test if the tuple satisfies the stub's
 * qualification, eh? Trivial! Easy! 5 lines of code!
 * NOOOOOOO! This is NOT enough! The problem is that a stub's
 * qualification must also be considered "satisfied" if it depends on
 * an attribute with a "write" lock.
 * So, for every new "write" lock added, we must re-check all the
 * previous stubs to see if they "depend" on this lock. Bliah!
 * 
 *-----------------------------------------------------------------------
 */
RuleLock
prs2FindLocksForNewTupleFromStubs(tuple, buffer, stubs, relation)
HeapTuple tuple;
Buffer buffer;
Prs2Stub stubs;
Relation relation;
{
    TupleDescriptor tdesc;
    Prs2OneStub thisStub;
    LispValue thisQual;
    Prs2OneLock thisLock;
    Prs2LockType locktype;
    bool done;
    RuleLock res, t;
    bool *used;
    register int i, j;

    /*
     * check the trival case...
     */
    if (stubs->numOfStubs == 0) {
	res = prs2MakeLocks();
	return(res);
    }

    /*
     * keep an array of the stubs that "succeeded" in adding
     * locks to the tuple.
     */
    used = (bool *) palloc(stubs->numOfStubs * sizeof(bool));
    if (used == NULL) {
	elog(WARN,"prs2FindLocksForNewTupleFromStubs: out of memory");
    }
    for (i=0; i<stubs->numOfStubs; i++)
	used[i] = false;

    tdesc = RelationGetTupleDescriptor(relation);
    res = prs2MakeLocks();

    /*
     * Yes, I know, this code sure looks ugly, but it must work
     * by tonight! When (if!) I have some more time I'll clean it
     * & speed it up a little...
     */
    do {
	done = true;
	/*
	 * check one by one all stubs...
	 */
	for (i=0; i<stubs->numOfStubs; i++) {
	    if (used[i]) {
		/*
		 * no need to check this stub again
		 */
		continue;
	    }
	    thisStub = stubs->stubRecords[i];
	    thisQual = thisStub->qualification;
	    /*
	     * should we use the lock of this stub?
	     * The answer is yes iff the tuple satisfies
	     * the stub's qual or if the qual depends
	     * on an attribute which has a "write" lock on it.
	     */
	    if (prs2StubQualTestTuple(tuple, buffer, tdesc, thisQual)) {
		used[i] = true;
	    } else {
		for (j=0; j<prs2GetNumberOfLocks(res); j++) {
		    thisLock = prs2GetOneLockFromLocks(res, j);
		    locktype = prs2OneLockGetLockType(thisLock);
		    if (locktype == LockTypeRetrieveWrite) {
			/*
			 * does this stub depend on the attribute
			 * "calculated" by this "write" lock?
			 */
			if (prs2DoesStubDependsOnAttribute(thisStub,
				prs2OneLockGetAttributeNumber(thisLock))){
			    used[i] = true;
			    break;
			}
		    }
		}
	    }
	    if (used[i]) {
		/*
		 * OK, we must use this attribute.
		 * Add its lock to the 'res'
		 */
		t = prs2LockUnion(res, thisStub->lock);
		prs2FreeLocks(res);
		res = t;
		/*
		 * if this stub had a "write" lock, then
		 * 'done' is false because we must check again
		 * all the stubs.
		 */
		for (j=0; j<prs2GetNumberOfLocks(thisStub->lock); j++) {
		    thisLock = prs2GetOneLockFromLocks(thisStub->lock, j);
		    locktype = prs2OneLockGetLockType(thisLock);
		    if (locktype == LockTypeRetrieveWrite) {
			done = false;
			break;
		    }
		}
	    }
	}
    } while (!done);

    return(res);
}
