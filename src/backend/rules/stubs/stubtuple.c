
/*===================================================================
 *
 * FILE:
 * stubtuple.c
 *
 * IDENTIFICATION:
 * $Header$
 *
 *
 *===================================================================
 */


#include <stdio.h>
#include "datum.h"
#include "log.h"
#include "tupdesc.h"
#include "heapam.h"
#include "fmgr.h"
#include "prs2.h"
#include "prs2stub.h"

/*--------------------------------------------------------------------
 *
 * prs2StubGetLocksForTuple
 *
 * given a collection of stub records and a tuple, find all the locks
 * that the tuple must inherit.
 */
RuleLock
prs2StubGetLocksForTuple(tuple, buffer, tupDesc, stubs)
HeapTuple tuple;
Buffer buffer;
TupleDescriptor tupDesc;
Prs2Stub stubs;
{
    int i;
    RuleLock temp, resultLock;
    Prs2OneStub oneStub;

    /*
     * resultLock is initially an empty lock
     */
    resultLock = prs2MakeLocks();

    for (i=0; i<stubs->numOfStubs; i++) {
	oneStub = &(stubs->stubRecords[i]);
	if (prs2StubTestTuple(tuple, buffer, tupDesc, oneStub)) {
	    temp = prs2LockUnion(resultLock, oneStub->lock);
	    prs2FreeLocks(resultLock);
	    resultLock = temp;
	}
    }

    return(resultLock);

}

/*--------------------------------------------------------------------
 *
 * prs2StubTestTuple
 *
 * test if a tuple satisfies all the qualifications of a given
 * stub record.
 */
bool
prs2StubTestTuple(tuple, buffer, tupDesc, stub)
HeapTuple tuple;
Buffer buffer;
TupleDescriptor tupDesc;
Prs2OneStub stub;
{
    int i;
    Prs2SimpleQual oneQual;

    for(i=0; i<stub->numQuals; i++) {
	oneQual = & (stub->qualification[i]);
	if (!prs2SimpleQualTestTuple(tuple, buffer, tupDesc, oneQual)) {
	    return(false);
	}
    }

    return(true);
}

/*--------------------------------------------------------------------
 *
 * prs2SimpleQualTestTuple
 *
 * test if a tuple satisfies the given 'Prs2SimpleQual'
 */
bool
prs2SimpleQualTestTuple(tuple, buffer, tupDesc, qual)
HeapTuple tuple;
Buffer buffer;
TupleDescriptor tupDesc;
Prs2SimpleQual qual;
{
    Datum value;
    Boolean isNull;
    int result;

    /*
     * extract the value from the tuple
     */

    value = HeapTupleGetAttributeValue(
		tuple,
		buffer,
		qual->attrNo,
		tupDesc,
		&isNull);
    
    /*
     * NOTE: if the attribute is null, we assume that it does NOT
     * pass the qualification
     */
    if (isNull) {
	return(false);
    }

    /*
     * Now call the function manager...
     */
    
    result = (int) fmgr(qual->operator, value, qual->constData);

    if (result) {
	return(true);
    } else {
	return(false);
    }
}
