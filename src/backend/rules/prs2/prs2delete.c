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

Prs2Status prs2Delete() { }

#ifdef NOT_YET
/*------------------------------------------------------------------
 *
 * prs2Delete()
 *
 * Activate any rules that apply to the deleted tuple
 *
 * Arguments:
 *	tuple: the tuple to be deleted
 *	buffer:	the buffer for 'tuple'
 *	relation: the relation where 'tuple' belongs
 * Return value:
 *	PRS2MGR_INSTEAD: do not perform the delete
 *      PRS2MGR_TUPLE_UNCHANGED: Ok, go ahead & delete the tuple
 */
prs2Delete(tuple, buffer, relation)
HeapTuple tuple;
Buffer buffer;
Relation relation;
{
    Prs2Locks locks;
    Prs2OneLocks oneLock;
    int nlocks;
    int i;
    int status;
    LispDisplay plan;
    LispDisplay planInfo;
    LispDisplay planQual;
    LispDisplay planAction;

    locks = prs2GetLocksFromTuple (tuple, buffer, &(relation->rd_att));

    nlocks = prs2GetNumberOfLocks(locks);
    for (i=0; i<nlocks; i++) {
	oneLock = prs2GetOneLockFromLocks(locks, i);
	if (prs2OneLockGetEventType != EventTypeDelete) {
	    /*
	     * Ignore this lock & go to the next one
	     */
	    continue;
	}
	/*
	 * We've found a lock. Go in the system catalogs and
	 * extract the appropriate plan info...
	 */
	plan = prs2GetRulePlanFromCatalog(
			    prs2OneLockGetRuleId(oneLock),
			    prs2OneLockGetPlanNumber(oneLock));
	planInfo = prs2GetRuleInfoFromRulePlan(plan);
	planQual = prs2GetQualFromRulePlan(plan);
	planAction = prs2GetActionsFromRulePlan(plan);


	
			
	
    prs2FreeLocks(locks);
    return(status);
}
#endif NOT_YET
