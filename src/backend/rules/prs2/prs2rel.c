/*=======================================================================
 * FILE:
 *   prs2rel.c
 *
 * IDENTIFICATION:
 *   $Header$
 *
 * DESCRIPTION:
 *
 *   Code for putting/removing relation level locks:
 *   Relation level locks are stored in pg_relation. All the
 *   tuples of the coresponding relation are assumed to be locked by
 *   these locks.
 *   Relation Level locks are used by both the tuple-level & the query
 *   rewrite rule systems. However the tuple-level system can also
 *   use tuple-level locks.
 *		
 *=======================================================================
 */

#include "tmp/postgres.h"
#include "nodes/primnodes.h"
#include "nodes/primnodes.a.h"
#include "nodes/pg_lisp.h"
#include "utils/log.h"
#include "utils/relcache.h"	/* RelationNameGetRelation() defined here...*/
#include "rules/prs2.h"
#include "rules/prs2stub.h"
#include "access/skey.h"	/* 'ScanKeyEntryData' defined here... */
#include "access/tqual.h"	/* 'NowTimeQual' defined here.. */
#include "access/heapam.h"	/* heap AM calls defined here */
#include "utils/lsyscache.h"	/* for get_attnum(), get_rel_name() ...*/
#include "parser/parse.h"	/* RETRIEVE, APPEND etc defined here.. */
#include "parser/parsetree.h"
#include "catalog/catname.h"	/* names of various system relations */
#include "utils/fmgr.h"	/* for F_CHAR16EQ, F_CHAR16IN etc. */
#include "access/ftup.h"	/* for FormHeapTuple() */
#include "utils/palloc.h"

#include "catalog/pg_proc.h"
#include "catalog/pg_prs2rule.h"
#include "catalog/pg_prs2plans.h"

Prs2Stub prs2FindStubsThatDependOnAttribute();

/*-----------------------------------------------------------------------
 * prs2DefRelationLevelLockRule
 *
 * Put the appropriate locks for a tuple-system rule that is implemented
 * with a relation level lock.
 *
 * We have to do two things:
 * a) put the relation level lock for this rule (easy!)
 * b) If this is a "on retrieve ... do retrieve" rule, i.e.
 * if it places a "write" lock to an attribute, then we must find all
 * the tuple-level-lock rules that reference this attribute in their
 * qualification and change them to relation-level-lock rules too.
 * (see the discussion in the comments in "prs2putlocks.c"...)
 *-----------------------------------------------------------------------
 */
void
prs2DefRelationLevelLockRule(r)
Prs2RuleData r;
{
    RuleLock oldLocks, newLocks;
    LispValue actionPlans;
    Name relationName;
    Prs2Stub relstubs, stubs, newstubs;
    ObjectId *ruleoids;
    int nrules;
    int i;
    AttributeNumber attributeNo;
    Prs2LockType lockType;


    /*
     * insert the appropriate info in the system catalogs.
     * (and find the rule oid)
     */
    r->ruleId = prs2InsertRuleInfoInCatalog(r);

    elog(DEBUG,
	"Rule %s (id=%ld) will be implemented with a relation level lock",
	r->ruleName, r->ruleId);

    actionPlans = prs2GenerateActionPlans(r);
    prs2InsertRulePlanInCatalog(r->ruleId, ActionPlanNumber, actionPlans);

    /*
     * find the loc type and the attribute that must be locked
     */
    prs2FindLockTypeAndAttrNo(r, &lockType, &attributeNo);


    /*
     * Find the relation level locks of the relation
     */
    relationName = get_rel_name(r->eventRelationOid);
    oldLocks = prs2GetLocksFromRelation(relationName);

    /*
     * now claculate the new locks
     */
    newLocks = prs2CopyLocks(oldLocks);
    newLocks = prs2AddLock(newLocks,
		    r->ruleId,
		    lockType,
		    attributeNo,
		    ActionPlanNumber, /* 'ActionPlanNumber' is a constant */
		    0,			/* partial indx - UNUSED */
		    0);			/* npartial - UNUSED  */

    /*
     * if this is a "write" lock, we might have to change some other
     * rules too (from tuple-level-lock to relation-level-lock).
     */
    if (lockType == LockTypeRetrieveWrite) {
	/*
	 * find all the tuple-level-lock rules that must be changed
	 * to relation-level-lock...
	 * Look at the (relation level) stubs of this relation.
	 * and find all the stubs that "depend" on "attributeNo"
	 * (the attribute to be locked by a "write" lock).
	 */
	relstubs = prs2GetRelationStubs(r->eventRelationOid);
	stubs = prs2FindStubsThatDependOnAttribute(relstubs, attributeNo);
	/*
	 * now get the rule oids from the stubs & store them in an
	 * array.
	 */
	nrules = stubs->numOfStubs;
	if (nrules>0) {
	    ruleoids = (ObjectId *) palloc(nrules * sizeof(ObjectId));
	    if (ruleoids==NULL) {
		elog(WARN,"prs2DefRelationLevelLockRule: out of memory!");
	    }
	}
	for (i=0; i<nrules; i++) {
	    ruleoids[i] = stubs->stubRecords[i]->ruleId;
	}

#ifdef PRS2_DEBUG
	{
	    int k;

	    printf(
	    "PRS2: the following rules will change from TupLev to RelLev\n");
	    printf("PRS2:");
	    for (k=0; k<nrules; k++) {
		printf(" %ld", ruleoids[k]);
	    }
	    printf("\n");
	}
#endif PRS2_DEBUG

	/*
	 * now remove their tuple level locks and stubs..
	 */
	prs2RemoveTupleLeveLocksAndStubsOfManyRules(r->eventRelationOid,
						    ruleoids,
						    nrules);
	/*
	 * finally, these rules must be implemented with a relation
	 * level lock, so add the appropriate locks to 'newLocks'
	 */
	for (i=0; i<nrules; i++) {
	    newLocks = prs2LockUnion(newLocks, stubs->stubRecords[i]->lock);
	}
    }

    /*
     * OK, now update the relation-level-locks of the relation.
     */
    prs2SetRelationLevelLocks(r->eventRelationOid, newLocks);

}

/*--------------------------------------------------------------
 *
 * prs2UndefRelationLevelLockRule
 *
 * remove a rule that uses relation level locks...
 * delete its relation level lock & all the info from the
 * system catalogs.
 *--------------------------------------------------------------
 */
void
prs2UndefRelationLevelLockRule(ruleId, relationId)
ObjectId ruleId;
ObjectId relationId;
{

    elog(DEBUG, "Removing relation-level-lock rule %ld", ruleId);

    /*
     * first remove the relation level lock
     */
    prs2RemoveRelationLevelLocksOfRule(ruleId, relationId);

    /*
     * Finally remove the system catalog info...
     */
    prs2DeleteRulePlanFromCatalog(ruleId);
    prs2DeleteRuleInfoFromCatalog(ruleId);

}

/*-----------------------------------------------------------------------
 * prs2RemoveRelationLevelLocksOfRule
 *
 * remove the relation level locks of the given rule
 *
 *-----------------------------------------------------------------------
 */
void
prs2RemoveRelationLevelLocksOfRule(ruleId, relationId)
ObjectId ruleId;
ObjectId relationId;
{
    RuleLock currentLocks;
    RuleLock newLocks;
    Name relationName;

    /*
     * first find the current locks of the relation
     */
    relationName = get_rel_name(relationId);
    if (relationName == NULL) {
	/*
	 * complain, but do not abort the transaction
	 * so that we can remove a rule, even if we have
	 * accidentally destroyed the corresponding relation...
	 */
	elog(NOTICE, "prs2RemoveRelationLevelLocksOfRule: can not find rel %d",
	    relationId);
	return;
    }

    currentLocks = prs2GetLocksFromRelation(relationName);
#ifdef PRS2_DEBUG
    printf("PRS2: Remove RelationLevelLock of rule %d from rel=%s\n",
	ruleId, relationName);
#endif PRS2_DEBUG

    /*
     * Now calculate the new locks
     */
    newLocks = prs2RemoveAllLocksOfRule(currentLocks, ruleId);

    /*
     * Now, update the locks of the relation
     */
    prs2SetRelationLevelLocks(relationId, newLocks);
}

/*-----------------------------------------------------------------------
 * prs2SetRelationLevelLocks
 *
 * Set the relation level locks for a relation to the given ones.
 * These locks are stored in the appropriate tuple of pg_relation.
 * All the tuples of the target relation are assumed to be locked
 *
 * Relation level locks are used by both the tuple-level system and
 * the query rewrite system.
 *
 *-----------------------------------------------------------------------
 */

void
prs2SetRelationLevelLocks(relationId, newLocks)
ObjectId relationId;
RuleLock newLocks;
{

    Relation relationRelation;
    HeapScanDesc scanDesc;
    ScanKeyData scanKey;
    HeapTuple tuple;
    Buffer buffer;
    HeapTuple newTuple;

    /*
     * Lock a relation given its ObjectId.
     * Go to the RelationRelation (i.e. pg_relation), find the
     * appropriate tuple, and add the specified lock to it.
     */
    relationRelation = RelationNameOpenHeapRelation(RelationRelationName);

	ScanKeyEntryInitialize(&scanKey.data[0], 0, ObjectIdAttributeNumber,
						   ObjectIdEqualRegProcedure, 
						   ObjectIdGetDatum(relationId));

    scanDesc = RelationBeginHeapScan(relationRelation,
					false, NowTimeQual,
					1, &scanKey);
    
    tuple = HeapScanGetNextTuple(scanDesc, false, &buffer);
    if (!HeapTupleIsValid(tuple)) {
	elog(WARN, "prs2SetRelationLevelLocks: Invalid rel OID %ld",
		relationId);
    }

#ifdef PRS2_DEBUG
    {
	RuleLock l;

	l = prs2GetLocksFromTuple(tuple, buffer);
	printf(
	    "PRS2:prs2SetRelationLevelLocks: Updating locks of relation %d\n",
	    relationId);
	printf("PRS2: Old RelLev Locks = ");
	prs2PrintLocks(l);
	printf("\n");
	printf("PRS2: New RelLev Locks = ");
	prs2PrintLocks(newLocks);
	printf("\n");
    }
#endif PRS2_DEBUG

    /*
     * Create a new tuple (i.e. a copy of the old tuple
     * with its rule lock field changed and replace the old
     * tuple in the RelationRelation
     * NOTE: XXX ??? do we really need to make that copy ????
     */
    newTuple = palloctup(tuple, buffer, relationRelation);
    prs2PutLocksInTuple(newTuple, InvalidBuffer, relationRelation, newLocks);

    RelationReplaceHeapTuple(relationRelation, &(tuple->t_ctid),
			    newTuple, (double *)NULL);
    
    RelationCloseHeapRelation(relationRelation);
    HeapScanEnd(scanDesc);

}


/*-------------------------------------------------------------------
 * prs2AddRelationLevelLock
 *
 * create & add a new relation level lock to the given relation.
 *
 *-------------------------------------------------------------------
 */
void
prs2AddRelationLevelLock(ruleId, lockType, relationId, attrNo)
ObjectId ruleId;
Prs2LockType lockType;
ObjectId relationId;
AttributeNumber attrNo;
{
    RuleLock currentLocks;
    RuleLock newLocks;
    Name relationName;

    /*
     * first find the current locks of the relation
     */
    relationName = get_rel_name(relationId);
    currentLocks = prs2GetLocksFromRelation(relationName);

    /*
     * Now calculate the new locks
     */
    newLocks = prs2AddLock(currentLocks,
			ruleId,
			lockType,
			attrNo,
			ActionPlanNumber, /* ActionPlanNumber is a constant.*/
			0,		  /* partialindx - UNUSED */
			0);		  /* npartial - UNUSED */

    /*
     * Now, update the locks of the relation
     */
    prs2SetRelationLevelLocks(relationId, newLocks);

}

