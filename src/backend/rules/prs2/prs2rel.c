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
 *	prs2PutRelationLevelLocks
 *	prs2RemoveRelationLevelLocks
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
#include "access/skey.h"	/* 'ScanKeyEntryData' defined here... */
#include "access/tqual.h"	/* 'NowTimeQual' defined here.. */
#include "access/heapam.h"	/* heap AM calls defined here */
#include "utils/lsyscache.h"	/* get_attnum()  defined here...*/
#include "parser/parse.h"	/* RETRIEVE, APPEND etc defined here.. */
#include "parser/parsetree.h"
#include "catalog/catname.h"	/* names of various system relations */
#include "utils/fmgr.h"	/* for F_CHAR16EQ, F_CHAR16IN etc. */
#include "access/ftup.h"	/* for FormHeapTuple() */

#include "catalog/pg_proc.h"
#include "catalog/pg_prs2rule.h"
#include "catalog/pg_prs2plans.h"

extern HeapTuple palloctup();

/*-----------------------------------------------------------------------
 * prs2PutRelationLevelLocks
 *
 * Put the appropriate relation level locks. 
 * These locks are stored in the appropriate tuple of pg_relation.
 * All the tuples of the target relation are assumed to be locked
 *
 * Relation level locks are used by both the tuple-level system and
 * the query rewrite system.
 *-----------------------------------------------------------------------
 */

void
prs2PutRelationLevelLocks(ruleId, lockType, eventRelationOid,
				    lockedAttrNo)
ObjectId ruleId;
Prs2LockType lockType;
ObjectId eventRelationOid;
AttributeNumber lockedAttrNo;
{

    Relation relationRelation;
    HeapScanDesc scanDesc;
    ScanKeyData scanKey;
    HeapTuple tuple;
    Buffer buffer;
    HeapTuple newTuple;
    RuleLock currentLock;

#ifdef PRS2_DEBUG
	printf(
	    "Putting relation level locks (ruleId=%ld, relId=%ld, attr=%hd)\n",
	    ruleId, eventRelationOid, lockedAttrNo);
#endif PRS2_DEBUG


    /*
     * Lock a relation given its ObjectId.
     * Go to the RelationRelation (i.e. pg_relation), find the
     * appropriate tuple, and add the specified lock to it.
     */
    relationRelation = RelationNameOpenHeapRelation(RelationRelationName);

    scanKey.data[0].flags = 0;
    scanKey.data[0].attributeNumber = ObjectIdAttributeNumber;
    scanKey.data[0].procedure = ObjectIdEqualRegProcedure;
    scanKey.data[0].argument = ObjectIdGetDatum(eventRelationOid);
    scanDesc = RelationBeginHeapScan(relationRelation,
					0, NowTimeQual,
					1, &scanKey);
    
    tuple = HeapScanGetNextTuple(scanDesc, 0, &buffer);
    if (!HeapTupleIsValid(tuple)) {
	elog(WARN, "Invalid rel OID %ld", eventRelationOid);
    }

    /*
     * We have found the appropriate tuple of the RelationRelation.
     * Now find its old locks, and add the new one
     */
    currentLock = prs2GetLocksFromTuple(tuple, buffer,
					relationRelation->rd_att);

#ifdef PRS2_DEBUG
    /*-- DEBUG --*/
    printf("previous Relation Level Lock:");
    prs2PrintLocks(currentLock);
    printf("\n");
#endif /* PRS2_DEBUG */

    currentLock = prs2AddLock(currentLock,
			ruleId,
			lockType,
			lockedAttrNo,
			ActionPlanNumber);	/* ActionPlanNumber is
						   a constant.  */

#ifdef PRS2_DEBUG
    /*-- DEBUG --*/
    printf("new Relation Level Lock:");
    prs2PrintLocks(currentLock);
    printf("\n");
#endif /* PRS2_DEBUG */

    /*
     * Create a new tuple (i.e. a copy of the old tuple
     * with its rule lock field changed and replace the old
     * tuple in the RelationRelation
     * NOTE: XXX ??? do we really need to make that copy ????
     */
    newTuple = palloctup(tuple, buffer, relationRelation);
    prs2PutLocksInTuple(newTuple, buffer, relationRelation, currentLock);

    RelationReplaceHeapTuple(relationRelation, &(tuple->t_ctid),
			    newTuple, (double *)NULL);
    
    RelationCloseHeapRelation(relationRelation);

}


/*--------------------------------------------------------------
 *
 * prs2RemoveRelationLevelLocks
 *
 * Unlock a relation given its ObjectId and the Objectid of the rule
 * Go to the RelationRelation (i.e. pg_relation), find the
 * appropriate tuple, and remove all locks of this rule from it.
 *
 * Relation level locks are used by both the tuple-level system and
 * the query rewrite system.
 *--------------------------------------------------------------
 */

void
prs2RemoveRelationLevelLocks(ruleId, eventRelationOid)
ObjectId ruleId;
ObjectId eventRelationOid;
{

    Relation relationRelation;
    HeapScanDesc scanDesc;
    ScanKeyData scanKey;
    HeapTuple tuple;
    Buffer buffer;
    HeapTuple newTuple;
    HeapTuple newTuple2;
    RuleLock currentLocks;
    Boolean isNull;
    int i;
    int numberOfLocks;
    Prs2OneLock oneLock;

    relationRelation = RelationNameOpenHeapRelation(RelationRelationName);

    scanKey.data[0].flags = 0;
    scanKey.data[0].attributeNumber = ObjectIdAttributeNumber;
    scanKey.data[0].procedure = ObjectIdEqualRegProcedure;
    scanKey.data[0].argument = ObjectIdGetDatum(eventRelationOid);
    scanDesc = RelationBeginHeapScan(relationRelation,
					0, NowTimeQual,
					1, &scanKey);
    
    tuple = HeapScanGetNextTuple(scanDesc, 0, &buffer);
    if (!HeapTupleIsValid(tuple)) {
	elog(NOTICE, "Could not find relation with id %ld\n", eventRelationOid);
    } else {
	/*
	 * "calculate" the new locks...
	 */
	currentLocks = prs2GetLocksFromTuple(
				tuple,
				buffer,
				&(relationRelation->rd_att));
#ifdef PRS2_DEBUG
	printf("previous Lock:");
	prs2PrintLocks(currentLocks);
	printf("\n");
#endif PRS2_DEBUG

	/*
	 * find all the locks with the given rule id and remove them
	 */
	currentLocks = prs2RemoveAllLocksOfRule(currentLocks, ruleId);

#ifdef PRS2_DEBUG
	printf("new Lock:");
	prs2PrintLocks(currentLocks);
	printf("\n");
#endif PRS2_DEBUG

	/*
	 * Create a new tuple (i.e. a copy of the old tuple
	 * with its rule lock field changed and replace the old
	 * tuple in the RelationRelation
	 * NOTE: XXX ??? do we really need to make that copy ????
	 */
	newTuple = palloctup(tuple, buffer, relationRelation);
	prs2PutLocksInTuple(tuple, buffer, relationRelation, currentLocks);
	RelationReplaceHeapTuple(relationRelation, &(tuple->t_ctid),
				newTuple, (double *)NULL);
	
    }

    /*
     * we are done, close the RelationRelation...
     */
    RelationCloseHeapRelation(relationRelation);

}
