/*====================================================================
 * FILE: prs2impexp.c
 *
 * $Header$
 *
 * DESCRIPTION:
 * routines that have to do with the import/export locking scheme.
 *====================================================================
 */
#include "tmp/c.h"
#include "utils/log.h"
#include "utils/palloc.h"
#include "rules/prs2.h"
#include "rules/prs2stub.h"
#include "parse.h"		/* for APPEND, DELETE */

/*----------------------------------------------------------------------
 * prs2FindNewExportLocksFromLocks
 *
 *----------------------------------------------------------------------
 */
RuleLock
prs2FindNewExportLocksFromLocks(locks)
RuleLock locks;
{
    RuleLock copylocks, expLocks, temp, thisLock;
    int nlocks;
    int i,j;
    Prs2OneLock oneLock, oneLock2;
    int npartial, partialindx;
    bool *table;

    /*
     * make a copy of the locks because we are going to
     * update them in place...
     */
    copylocks = prs2CopyLocks(locks);
    nlocks = prs2GetNumberOfLocks(copylocks);
    expLocks = prs2MakeLocks();

    for (i=0; i<nlocks; i++) {
	oneLock = prs2GetOneLockFromLocks(copylocks, i);
	if (prs2OneLockGetLockType(oneLock) != LockTypeImport)
	    continue;
	npartial = prs2OneLockGetNPartial(oneLock);
	Assert(npartial != 0);
	partialindx = prs2OneLockGetPartialIndx(oneLock);
	table = (bool *) palloc(sizeof(bool) * npartial);
	for (j=0; j<npartial; j++)
	    table[j] = false;
	table[partialindx] = true;
	/*
	 * now find all other import locks with the same ruleid and
	 * planid. Update `table' accordingly...
	 * NOTE: modify its 'locktype' to invalid, so that we don't
	 * check it again later... (hack, hack...)
	 */
	for (j=i+1; j<nlocks; j++) {
	    oneLock2 = prs2GetOneLockFromLocks(copylocks, j);
	    if (prs2OneLockGetLockType(oneLock2) != LockTypeImport)
		continue;
	    if (prs2OneLockGetRuleId(oneLock2) != 
		prs2OneLockGetRuleId(oneLock))
		continue;
	    if (prs2OneLockGetPlanNumber(oneLock2) != 
		prs2OneLockGetPlanNumber(oneLock))
		continue;
	    table[prs2OneLockGetPartialIndx(oneLock2)] = true;
	    prs2OneLockSetLockType(oneLock2, LockTypeInvalid);
	}
	/*
	 * are all the "partial" import locks of the rule there?
	 */
	for (j=0; j<npartial; j++) {
	    if (!table[j])
		break;
	}
	if (j==npartial) {
	    /*
	     * yes, there are! find the export lock and add it
	     * to the 'expLocks'.
	     * NOTE: we only add the lokcs that there were
	     * not already there!
	     *
	     * NOTE: prs2LockUnion & prs2LockDiffernece
	     * create copies, so get rid of the extra locks...
	     */
	    thisLock = prs2GetExportLockFromCatalog(
			    prs2OneLockGetRuleId(oneLock),
			    prs2OneLockGetPlanNumber(oneLock));
	    temp = prs2LockDifference(thisLock, locks);
	    prs2FreeLocks(thisLock);
	    thisLock = temp;
	    temp = prs2LockUnion(expLocks, thisLock);
	    prs2FreeLocks(thisLock);
	    prs2FreeLocks(expLocks);
	    expLocks = temp;
	}
	/*
	 * don't forget to free the table!
	 */
	pfree(table);
    } /* for i */
    
    /*
     * cleanup & return result locks...
     */
    prs2FreeLocks(copylocks);
    return(expLocks);
}

/*--------------------------------------------------------------------
 * prs2GetExportLockFromCatalog
 *
 * look into the system catalogs for all required info and
 * construct the export lock for the given `ruleid' and `planid'.
 *--------------------------------------------------------------------
 */
RuleLock
prs2GetExportLockFromCatalog(ruleId, planNo)
ObjectId ruleId;
Prs2PlanNumber planNo;
{

    LispValue type;
    LispValue plan;
    LispValue lockInfo;
    RuleLock lock;

    plan = prs2GetRulePlanFromCatalog(ruleId, planNo, (ParamListInfo *) NULL);

    /*
     * sanity check...
     */
    type = prs2GetTypeOfRulePlan(plan);
    if (strcmp(CString(type), Prs2RulePlanType_EXPORT)) {
	elog(WARN, "prs2GetExportLockFromCatalog: plan is not EXPORT!");
    }

    /*
     * extract the lock stored in the plan
     */
    lockInfo = prs2GetLockInfoFromExportPlan(plan);
    lock = StringToRuleLock(CString(lockInfo));

    return(lock);
}

/*--------------------------------------------------------------------
 * prs2ActivateExportLockRulePlan
 *--------------------------------------------------------------------
 */
void
prs2ActivateExportLockRulePlan(oneLock, value, typeid, operation)
Prs2OneLock oneLock;
Datum value;
ObjectId typeid;
int operation;
{
    LispValue plan;
    LispValue type;
    ObjectId ruleId;
    ParamListInfo params;
    Prs2PlanNumber planNo;
    Boolean isnull;
    int i;

    ruleId = prs2OneLockGetRuleId(oneLock);
    planNo = prs2OneLockGetPlanNumber(oneLock);

    plan = prs2GetRulePlanFromCatalog(ruleId, planNo, &params);

    /*
     * sanity check...
     */
    type = prs2GetTypeOfRulePlan(plan);
    if (strcmp(CString(type), Prs2RulePlanType_EXPORT)) {
	elog(WARN, "prs2GetExportLockFromCatalog: plan is not EXPORT!");
    }

    /*
     * `params' must contain only one parameter.
     * Make it have the given `value' and `type'.
     */
    if (params[0].kind != PARAM_OLD)
	elog(WARN, "prs2ActivateExportLockRulePlan: illegal param");
    if (params[1].kind != PARAM_INVALID)
	elog(WARN, "prs2ActivateExportLockRulePlan: more than 1 param");
    params[0].isnull = false;
    params[0].value = value;
    params[0].type = typeid;
    params[0].length = (Size) get_typlen(type);

    /*
     * OK, now run the plan.
     * XXX: should we use a non null value for Prs2EStateInfo ??
     */
    prs2RunActionPlans(prs2GetActionPlanFromExportPlan(plan),
		    params, (Prs2EStateInfo) NULL);

}

