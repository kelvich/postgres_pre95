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

#include "log.h"
#include "palloc.h"
#include "prs2.h"

/*-----------------------------------------------------------------------
 *
 * prs2MakeLocks
 *
 * Create an empty prs2 locks, i.e. return a pointer to a 'Prs2LocksData'
 * with numberOfLocks = 0;
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
}


/*-----------------------------------------------------------------------
 *
 * prs2AddLock
 *
 * Add a new lock (filled in with the given data) to a 'prs2Locks'
 * Note that this frees the space occupied by 'prs2Locks' and reallocates
 * some other. So this routine should be used with caution!
 */
RuleLock
prs2AddLock(oldLocks, ruleId, lockType, attributeNumber, planNumber)
RuleLock	oldLocks;
ObjectId	ruleId;
Prs2LockType	lockType;
AttributeNumber attributeNumber;
Prs2PlanNumber	planNumber;
{
    long oldSize;
    long newSize;
    RuleLock newLocks;
    Prs2OneLock theNewLock;

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

    /*
     * Free the spcae occupied by oldLocks
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
 */
Prs2OneLock
prs2GetOneLockFromLocks(lock, n)
RuleLock lock;
int n;
{
    Prs2OneLock t;

    if (n >= lock->numberOfLocks || n<0) {
	elog(WARN, "prs2GetOneLockFromLocks: lock # out of range (%d/%d)",
	n, lock->numberOfLocks);
    } else {
	t = & ((lock->locks)[n]);
    }

    return(t);
}

/*------------------------------------------------------------------
 *
 * prs2GetLocksFromTuple
 *
 * Extract the locks from a tuple. It returns a 'RuleLock',
 * (NOTE:it will never return NULL! Even if the tuple has no
 * locks in it, it will return a 'RuleLock' with 'numberOfLocks'
 * equal to 0.
 *
 */

RuleLock
prs2GetLocksFromTuple(tuple, buffer, tupleDescriptor)
HeapTuple tuple;
Buffer buffer;
TupleDescriptor tupleDescriptor;
{
    
    Datum lockDatum;
    Boolean isNull;
    RuleLock locks;
    RuleLock t;
    Size size;

    lockDatum = HeapTupleGetAttributeValue(
                            tuple,
                            buffer,
                            RuleLockAttributeNumber,
			    tupleDescriptor,
                            &isNull);
    if (isNull) {
        /*
         * No lock was in there.. Create an empty lock
         */
        locks = prs2MakeLocks();
    } else {
	t = (RuleLock) DatumGetPointer(lockDatum);
	/*
	 * Make a copy of the locks
	 */
	size = prs2LockSize(prs2GetNumberOfLocks(t));
	locks = (RuleLock) palloc(size);
	if (locks == NULL) {
	    elog(WARN,"prs2MakeLocks: palloc(%d) failed",size);
	}
	bcopy((char *)t, (char *)locks, size);
    }

    return(locks);
}

/*------------------------------------------------------------------
 *
 * prs2PutLocksInTuple
 *
 * given a tuple, create a copy of it and put the given locks in its
 * t_lock field...
 *
 */

HeapTuple
prs2PutLocksInTuple(tuple, buffer, relation, newLocks)
HeapTuple   tuple;
Buffer      buffer;
Relation    relation;
RuleLock   newLocks;
{
    HeapTuple newTuple;
    HeapTuple palloctup();

    newTuple = palloctup(tuple, buffer, relation);
    /*
     * If there are no locks, then store a NULL!
     * (No need to store the empty lock header...)
     */
    if (prs2GetNumberOfLocks(newLocks) == 0) {
	newTuple->t_lock.l_lock = (RuleLock) NULL;
    } else {
	newTuple->t_lock.l_lock = (RuleLock) newLocks;
    }

    return(newTuple);
}

/*------------------------------------------------------------------
 *
 * prs2PrintLocks
 *
 * print the prs2 locks in stdout. Used for debugging...
 */

void
prs2PrintLocks(locks)
RuleLock   locks;
{
    int i;
    int nlocks;
    Prs2OneLock oneLock;

    if (locks == NULL) {
	elog(WARN, "prs2PrintLocks: NULL argument!");
    }

    nlocks = prs2GetNumberOfLocks(locks);
    printf("{%d locks", nlocks);

    for (i=0; i<nlocks; i++) {
	oneLock = prs2GetOneLockFromLocks(locks, i);
	printf(" (rulid=%ld,", prs2OneLockGetRuleId(oneLock));
	printf(" type=%c,", prs2OneLockGetLockType(oneLock));
	printf(" attrn=%d,", (int)prs2OneLockGetAttributeNumber(oneLock));
	printf(" plano=%d)", (int)prs2OneLockGetPlanNumber(oneLock));
    }

    printf(" }");
    fflush(stdout);
}


/*------------------------------------------------------------------
 *
 * prs2CopyLocks
 *
 * Make a copy of a prs2 lock..
 */
RuleLock
prs2CopyLocks(locks)
RuleLock locks;
{
    RuleLock newLocks;
    int numberOfLocks;
    int i;
    Prs2OneLock oneLock;

    numberOfLocks = prs2GetNumberOfLocks(locks);

    /*
     * create an emtpy lock
     */
    newLocks = prs2MakeLocks();

    /*
     * copy all the locks from locks to newLocks if their rule
     * id is different form the given one.
     */
    for (i=0; i < numberOfLocks; i++) {
	oneLock = prs2GetOneLockFromLocks(locks, i);
	prs2AddLock( newLocks,
	    prs2OneLockGetRuleId(oneLock),
	    prs2OneLockGetLockType(oneLock),
	    prs2OneLockGetAttributeNumber(oneLock),
	    prs2OneLockGetPlanNumber(oneLock));
    }

    return(newLocks);
}

/*------------------------------------------------------------------
 *
 * prs2RemoveOneLockInPlace
 *
 * This is a routine that removes one lock form a 'RuleLock'.
 * Note that the removal is done in place, i.e. no copy of the
 * original locks is made
 */
void
prs2RemoveOneLockInPlace(locks, n)
RuleLock locks;
int n;
{

    int i;
    Prs2OneLock lock1, lock2;

    if (n >= locks->numberOfLocks || n<0) {
	elog(WARN, "prs2RemoveOneLockInPlace: lock # out of range (%d/%d)",
	n, locks->numberOfLocks);
    }

    for (i=n+1; i<locks->numberOfLocks; i++) {
	lock1 = prs2GetOneLockFromLocks(locks, i-1);
	lock2 = prs2GetOneLockFromLocks(locks, i);
	prs2OneLockSetRuleId(lock1, prs2OneLockGetRuleId(lock2));
	prs2OneLockSetLockType(lock1, prs2OneLockGetLockType(lock2));
	prs2OneLockSetAttributeNumber(lock1,
		prs2OneLockGetAttributeNumber(lock2));
	prs2OneLockSetPlanNumber(lock1, prs2OneLockGetPlanNumber(lock2));
    }

    locks->numberOfLocks -= 1;
}


/*------------------------------------------------------------------
 *
 * prs2RemoveAllLocksOfRule
 *
 * remove all the locks that have a ruleId equal to the given.
 * the old `RuleLock' is destroyed and should never be
 * referenced again.
 * The new lock is returned.
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
			    prs2OneLockGetPlanNumber(oneLock));
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
		prs2OneLockGetPlanNumber(oneLock));
    }

    return(result);
}
