/*=======================================================================
 *
 * FILE:
 *   prs2locks.c
 *
 * IDENTIFICATION:
 *  $Header$
 *
 * Routines to manipulate PRS2 locks...
 * (there are also some useful #define's in lib/H/prs2.h
 *=======================================================================
 */

#include <ctype.h>
#include "utils/log.h"
#include "utils/palloc.h"
#include "rules/prs2.h"
#include "rules/rac.h"

/*-----------------------------------------------------------------------
 *
 * prs2FreeLocks
 *
 *-----------------------------------------------------------------------
 */
void
prs2FreeLocks(lock)
RuleLock lock;
{
    if (!RuleLockIsValid(lock))
	elog(WARN,"prs2FreeLocks: Invalid rule lock");

#ifdef PRS2_DEBUG
    {
    /*
     * yes, yes, it might help discover a couple of these
     * tedious, stupid, mean, "carefull what you pfree" memory bugs...
     */
    long i, size;

    size = prs2LockSize(lock->numberOfLocks);
    for (i=0; i<size; i++)
	*( ((char *)lock) + i) = (char) 255;
    }
#endif PRS2_DEBUG

    pfree((Pointer)lock);
}

/*-----------------------------------------------------------------------
 *
 * prs2MakeLocks
 *
 * Create an empty prs2 locks, i.e. return a pointer to a 'Prs2LocksData'
 * with numberOfLocks = 0;
 *-----------------------------------------------------------------------
 */
RuleLock
prs2MakeLocks()
{
    RuleLock t;

    t = (RuleLock) palloc(sizeof(Prs2LocksData));

    if (t==NULL) {
	elog(WARN, "prs2MakeLocks: palloc failed.");
    }

    t->numberOfLocks = 0;
    return(t);
}


/*-----------------------------------------------------------------------
 *
 * prs2AddLock
 *
 * Add a new lock (filled in with the given data) to a 'prs2Locks'
 * Note that this frees the space occupied by 'prs2Locks' and reallocates
 * some other. So this routine should be used with caution!
 *-----------------------------------------------------------------------
 */
RuleLock
prs2AddLock(oldLocks, ruleId, lockType, attributeNumber, planNumber,
		partialindx, npartial)
RuleLock	oldLocks;
ObjectId	ruleId;
Prs2LockType	lockType;
AttributeNumber attributeNumber;
Prs2PlanNumber	planNumber;
int		partialindx;
int		npartial;
{
    long oldSize;
    long newSize;
    RuleLock newLocks;
    Prs2OneLock theNewLock;

    if (!RuleLockIsValid(oldLocks))
	elog(WARN,"prs2AddLock: Invalid rule lock");

    /*
     * allocate enough space to hold the old locks + the new one..
     */
    newSize = prs2LockSize(oldLocks->numberOfLocks + 1);
    newLocks = (RuleLock) palloc(newSize);
    if (newLocks == NULL) {
	elog(WARN,"prs2AddLock: palloc failed");
    }

    /*
     * Now copy the old lock data to the new lock..
     */
    oldSize = prs2LockSize(oldLocks->numberOfLocks);
    bcopy(oldLocks, newLocks, oldSize);

    /*
     * update the information in newLocks..
     */
    newLocks->numberOfLocks = oldLocks->numberOfLocks + 1;

    theNewLock = prs2GetOneLockFromLocks(newLocks, newLocks->numberOfLocks-1);
    prs2OneLockSetRuleId(theNewLock, ruleId);
    prs2OneLockSetLockType(theNewLock, lockType);
    prs2OneLockSetAttributeNumber(theNewLock, attributeNumber);
    prs2OneLockSetPlanNumber(theNewLock, planNumber);
    prs2OneLockSetPartialIndx(theNewLock, partialindx);
    prs2OneLockSetNPartial(theNewLock, npartial);

    /*
     * Free the space occupied by oldLocks
     */
    prs2FreeLocks(oldLocks);
    return(newLocks);
}

/*-----------------------------------------------------------------------
 *
 * prs2GetOneLockFromLocks
 *
 * Given a 'RuleLock' return a pointer to its Nth lock..
 * (the first locks is lock number 0).
 *-----------------------------------------------------------------------
 */
Prs2OneLock
prs2GetOneLockFromLocks(lock, n)
RuleLock lock;
int n;
{
    Prs2OneLock t;

    if (!RuleLockIsValid(lock))
	elog(WARN,"prs2GetOneLockFromLocks: Invalid rule lock");

    if (n >= lock->numberOfLocks || n<0) {
	elog(WARN, "prs2GetOneLockFromLocks: lock # out of range (%d/%d)",
	n, lock->numberOfLocks);
    } else {
	t = & ((lock->locks)[n]);
    }

    return(t);
}

/*------------------------------------------------------------------
 * prs2OneLocksAreTheSame
 *
 * return true iff the given two 'Prs2OneLock' are the same...
 *------------------------------------------------------------------
 */
bool
prs2OneLocksAreTheSame(l1, l2)
Prs2OneLock l1;
Prs2OneLock l2;
{
    if (prs2OneLockGetRuleId(l1)==prs2OneLockGetRuleId(l2) &&
	prs2OneLockGetLockType(l1)==prs2OneLockGetLockType(l2) &&
	prs2OneLockGetAttributeNumber(l1)== prs2OneLockGetAttributeNumber(l2) &&
	prs2OneLockGetPlanNumber(l1)==prs2OneLockGetPlanNumber(l2) &&
	prs2OneLockGetPartialIndx(l1)==prs2OneLockGetPartialIndx(l2) &&
	prs2OneLockGetNPartial(l1)==prs2OneLockGetNPartial(l2))
	    return(true);
    else
	    return(false);
}

/*------------------------------------------------------------------
 * prs2OneLockIsMemberOfLocks
 *
 * return true iff the given `oneLock' is one of the locks in `locks'.
 *------------------------------------------------------------------
 */
bool
prs2OneLockIsMemberOfLocks(oneLock, locks)
Prs2OneLock oneLock;
RuleLock locks;
{
    int i, nlocks;
    Prs2OneLock l;

    nlocks = prs2GetNumberOfLocks(locks);
    for (i=0; i<nlocks; i++) {
	l = prs2GetOneLockFromLocks(locks, i);
	if (prs2OneLocksAreTheSame(oneLock, l))
	    return(true);
    }
    return(false);
}

/*------------------------------------------------------------------
 *
 * prs2GetLocksFromTuple
 *
 * Extract the locks from a tuple. It returns a 'RuleLock',
 *
 * NOTE 1:it will never return NULL! Even if the tuple has no
 * locks in it, it will return a 'RuleLock' with 'numberOfLocks'
 * equal to 0.)
 *
 * NOTE 2: The lock returned is a COPY, so it can and must be pfreed!
 * (otherwise we will have memory leaks!)
 *
 */

RuleLock
prs2GetLocksFromTuple(tuple, buffer)
HeapTuple tuple;
Buffer buffer;
{
    return(HeapTupleGetRuleLock(tuple, buffer));
}
 
/*------------------------------------------------------------------
 *
 * prs2PutLocksInTuple
 *
 * given a tuple, replace its current locks with the given new ones.
 *
 * NOTE: the old locks are pfreed!!!!!!
 *
 *-----------------------------------------------------------------------
 */

void
prs2PutLocksInTuple(tuple, buffer, relation, newLocks)
HeapTuple   tuple;
Buffer      buffer;
Relation    relation;
RuleLock   newLocks;
{
    HeapTupleSetRuleLock(tuple, InvalidBuffer, newLocks);
}

/*------------------------------------------------------------------
 *
 * prs2PrintLocks
 *
 * print the prs2 locks in stdout. Used for debugging...
 *-----------------------------------------------------------------------
 */

void
prs2PrintLocks(locks)
RuleLock   locks;
{
    int i;
    unsigned int type_int;
    int nlocks;
    Prs2OneLock oneLock;

    if (!RuleLockIsValid(locks)) {
	printf("{Invalidlock}");
	fflush(stdout);
	return;
    }

    nlocks = prs2GetNumberOfLocks(locks);
    printf("{%d locks", nlocks);

    for (i=0; i<nlocks; i++) {
	oneLock = prs2GetOneLockFromLocks(locks, i);
	printf(" (rulid=%ld,", prs2OneLockGetRuleId(oneLock));

	/*
	 * print the 'type' as a char only if it is a printable
	 * character, otherwise print it as a hex number.
	 */
	type_int = (unsigned int) prs2OneLockGetLockType(oneLock);
	if (type_int >= 32 && type_int < 127)
	    printf(" type='%c'", prs2OneLockGetLockType(oneLock));
	else
	    printf(" type=0x%x,", type_int);

	printf(" attrn=%d,", (int)prs2OneLockGetAttributeNumber(oneLock));
	printf(" plano=%d,", (int)prs2OneLockGetPlanNumber(oneLock));
	printf(" partial=%d/%ds)",
		(int)prs2OneLockGetPartialIndx(oneLock),
		(int)prs2OneLockGetNPartial(oneLock));
    }

    printf(" }");
    fflush(stdout);
}


/*------------------------------------------------------------------
 *
 * prs2CopyLocks
 *
 * Make a copy of a prs2 lock..
 *-----------------------------------------------------------------------
 */
RuleLock
prs2CopyLocks(locks)
RuleLock locks;
{
    RuleLock newLocks;
    int numberOfLocks;
    Size size;

    if (!RuleLockIsValid(locks))
	elog(WARN,"prs2CopyLocks: Invalid rule lock");

    numberOfLocks = prs2GetNumberOfLocks(locks);

    size = prs2LockSize(numberOfLocks);
    newLocks = (RuleLock) palloc(size);
    if (newLocks == NULL) {
	elog(WARN,"prs2MakeLocks: palloc(%d) failed",size);
    }
    bcopy((char *)locks, (char *)newLocks, size);

    return(newLocks);

}

/*------------------------------------------------------------------
 *
 * prs2RemoveOneLockInPlace
 *
 * This is a routine that removes one lock form a 'RuleLock'.
 * Note that the removal is done in place, i.e. no copy of the
 * original locks is made
 *-----------------------------------------------------------------------
 */
void
prs2RemoveOneLockInPlace(locks, n)
RuleLock locks;
int n;
{

    int i;
    Prs2OneLock lock1, lock2;

    if (!RuleLockIsValid(locks)) {
	elog(WARN, "prs2RemoveOneLockInPlace: called with NULL lock");
    }

    if (n >= locks->numberOfLocks || n<0) {
	elog(WARN, "prs2RemoveOneLockInPlace: lock # out of range (%d/%d)",
	n, locks->numberOfLocks);
    }

    /*
     * Copy the last lock to the lock to be deleted
     */
    lock1 = prs2GetOneLockFromLocks(locks, n);
    lock2 = prs2GetOneLockFromLocks(locks, prs2GetNumberOfLocks(locks)-1);

    prs2OneLockSetRuleId(lock1, prs2OneLockGetRuleId(lock2));
    prs2OneLockSetLockType(lock1, prs2OneLockGetLockType(lock2));
    prs2OneLockSetAttributeNumber(lock1,
	    prs2OneLockGetAttributeNumber(lock2));
    prs2OneLockSetPlanNumber(lock1, prs2OneLockGetPlanNumber(lock2));
    prs2OneLockSetPartialIndx(lock1, prs2OneLockGetPartialIndx(lock2));
    prs2OneLockSetNPartial(lock1, prs2OneLockGetNPartial(lock2));

    /*
     * decrease the number of locks
     */
    locks->numberOfLocks -= 1;
}


/*------------------------------------------------------------------
 * prs2RemoveAllLocksOfRuleInPlace
 *
 * remove all the locks of the given rule, in place (i.e. no copies).
 * Return true if there were locks actually removed, or false
 * if no locks for this rule were found.
 *-----------------------------------------------------------------------
 */
bool
prs2RemoveAllLocksOfRuleInPlace(locks, ruleId)
RuleLock locks;
ObjectId ruleId;
{
    int i;
    Prs2OneLock onelock;
    bool result;

    result = false;
    i = 0;
    while (i<prs2GetNumberOfLocks(locks)) {
	onelock = prs2GetOneLockFromLocks(locks, i);
	if (prs2OneLockGetRuleId(onelock) == ruleId) {
	    prs2RemoveOneLockInPlace(locks, i);
	    result = true;
	} else {
	    i++;
	}
    }

    return(result);
}

/*------------------------------------------------------------------
 *
 * prs2RemoveAllLocksOfRule
 *
 * remove all the locks that have a ruleId equal to the given.
 * the old `RuleLock' is destroyed and should never be
 * referenced again.
 * The new lock is returned.
 *-----------------------------------------------------------------------
 */
RuleLock
prs2RemoveAllLocksOfRule(oldLocks, ruleId)
RuleLock oldLocks;
ObjectId ruleId;
{
    RuleLock newLocks;
    int numberOfLocks;
    int i;
    Prs2OneLock oneLock;

    if (!RuleLockIsValid(oldLocks)) {
	elog(WARN,"prs2RemoveAllLocksOfRule: Invalid rule lock");
    }

    numberOfLocks = prs2GetNumberOfLocks(oldLocks);

    /*
     * create an emtpy lock
     */
    newLocks = prs2MakeLocks();

    /*
     * copy all the locks from oldLocks to newLocks if their rule
     * id is different form the given one.
     */
    for (i=0; i < numberOfLocks; i++) {
	oneLock = prs2GetOneLockFromLocks(oldLocks, i);
	if (prs2OneLockGetRuleId(oneLock) != ruleId) {
	    newLocks = prs2AddLock(
			    newLocks,
			    prs2OneLockGetRuleId(oneLock),
			    prs2OneLockGetLockType(oneLock),
			    prs2OneLockGetAttributeNumber(oneLock),
			    prs2OneLockGetPlanNumber(oneLock),
			    prs2OneLockGetPartialIndx(oneLock),
			    prs2OneLockGetNPartial(oneLock));
	}
    }

    prs2FreeLocks(oldLocks);
    return(newLocks);
}

/*------------------------------------------------------------------
 *
 * prs2LockUnion
 *
 * Create the union of two locks.
 * NOTE: we make COPIES of the locks. The two original locks
 * are NOT modified.
 *------------------------------------------------------------------
 */
RuleLock
prs2LockUnion(lock1, lock2)
RuleLock lock1;
RuleLock lock2;
{
    RuleLock result;
    Prs2OneLock oneLock;
    int nlocks;
    int i;

    result = prs2CopyLocks(lock1);

    nlocks = prs2GetNumberOfLocks(lock2);

    for (i=0; i<nlocks; i++) {
	oneLock = prs2GetOneLockFromLocks(lock2, i);
	result = prs2AddLock(result,
		prs2OneLockGetRuleId(oneLock),
		prs2OneLockGetLockType(oneLock),
		prs2OneLockGetAttributeNumber(oneLock),
		prs2OneLockGetPlanNumber(oneLock),
		prs2OneLockGetPartialIndx(oneLock),
		prs2OneLockGetNPartial(oneLock));
    }

    return(result);

}

/*------------------------------------------------------------------
 *
 * prs2LockDifference
 *
 * Create the differenc of the two locks, i.e. all Prs2OneLocks
 * that exist in `lock1' and do not exist in `lock2'.
 *
 * NOTE: we make COPIES of the locks. The two original locks
 * are NOT modified.
 *------------------------------------------------------------------
 */
RuleLock
prs2LockDifference(lock1, lock2)
RuleLock lock1;
RuleLock lock2;
{
    RuleLock result;
    Prs2OneLock oneLock;
    int nlocks;
    int i;

    result = prs2MakeLocks();

    nlocks = prs2GetNumberOfLocks(lock1);

    for (i=0; i<nlocks; i++) {
	oneLock = prs2GetOneLockFromLocks(lock1, i);
	if (!prs2OneLockIsMemberOfLocks(oneLock, lock2)) {
	    result = prs2AddLock(result,
		    prs2OneLockGetRuleId(oneLock),
		    prs2OneLockGetLockType(oneLock),
		    prs2OneLockGetAttributeNumber(oneLock),
		    prs2OneLockGetPlanNumber(oneLock),
		    prs2OneLockGetPartialIndx(oneLock),
		    prs2OneLockGetNPartial(oneLock));
	}
    }
    return(result);
}

/*------------------------------------------------------------------
 * prs2FindLocksOfType
 *
 * return a copy of all the locks contained in 'locks' that
 * match the given lock type.
 *------------------------------------------------------------------
 */
RuleLock
prs2FindLocksOfType(locks, lockType)
RuleLock locks;
Prs2LockType lockType;
{
    RuleLock result;
    int nlocks;
    Prs2OneLock oneLock;
    int i;

    result = prs2MakeLocks();

    nlocks = prs2GetNumberOfLocks(locks);

    for (i=0; i<nlocks; i++) {
	oneLock = prs2GetOneLockFromLocks(locks, i);
	if (prs2OneLockGetLockType(oneLock) == lockType) {
	    result = prs2AddLock(result,
			    prs2OneLockGetRuleId(oneLock),
			    prs2OneLockGetLockType(oneLock),
			    prs2OneLockGetAttributeNumber(oneLock),
			    prs2OneLockGetPlanNumber(oneLock),
			    prs2OneLockGetPartialIndx(oneLock),
			    prs2OneLockGetNPartial(oneLock));
	}
    }

    return(result);

}

/*------------------------------------------------------------------
 * prs2RemoveLocksOfTypeInPlace
 *
 * remove all the locks of the given type in place.
 *------------------------------------------------------------------
 */
void
prs2RemoveLocksOfTypeInPlace(locks, lockType)
RuleLock locks;
Prs2LockType lockType;
{
    int i;
    Prs2OneLock onelock;

    i = 0;
    while (i<prs2GetNumberOfLocks(locks)) {
	onelock = prs2GetOneLockFromLocks(locks, i);
	if (prs2OneLockGetLockType(onelock) == lockType) {
	    prs2RemoveOneLockInPlace(locks, i);
	} else {
	    i++;
	}
    }

}

