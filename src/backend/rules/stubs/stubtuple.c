
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
#include "tmp/postgres.h"

#include "utils/log.h"
#include "access/tupdesc.h"
#include "access/heapam.h"
#include "utils/fmgr.h"
#include "rules/prs2.h"
#include "rules/prs2stub.h"
#include "parser/parse.h"       /* for the AND, NOT, OR */

/*==================== ROUTINES LOCAL TO THIS FILE ================*/
static bool prs2SimpleQualTestTuple();


/*--------------------------------------------------------------------
 *
 * prs2StubGetLocksForTuple
 *
 * given a collection of stub records and a tuple, find all the locks
 * that the tuple must inherit.
 *
 *--------------------------------------------------------------------
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
	oneStub = stubs->stubRecords[i];
	if (prs2StubTestTuple(tuple, buffer, tupDesc, oneStub->qualification)){
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
 * test if a tuple satisfies the given qualifications of a
 * stub record.
 *--------------------------------------------------------------------
 */
bool
prs2StubTestTuple(tuple, buffer, tupDesc, qual)
HeapTuple tuple;
Buffer buffer;
TupleDescriptor tupDesc;
Prs2StubQual qual;
{
    int i;

    if (qual->qualType == PRS2_NULL_STUBQUAL) {
	/*
	 * NULL quals are ALWAYS true!
	 */
	return(true);
    } else if (qual->qualType == PRS2_SIMPLE_STUBQUAL) {
	if (prs2SimpleQualTestTuple(tuple,buffer,tupDesc,qual->qual.simple))
	    return(true);
	else
	    return(false);
    } else if (qual->qualType == PRS2_COMPLEX_STUBQUAL) {
	switch (qual->qual.complex.boolOper) {
	    case AND:
		for (i=0; i<qual->qual.complex.nOperands; i++) {
		    if (!prs2StubTestTuple(tuple,
			    buffer,
			    tupDesc,
			    qual->qual.complex.operands[i])) {
			/*
			 * this is an AND, so if any subqualification
			 * is false, then the whole thing is false
			 */
			return(false);
		    }
		}
		return(true);
		break;
	    case OR:
		for (i=0; i<qual->qual.complex.nOperands; i++) {
		    if (prs2StubTestTuple(tuple,
			    buffer,
			    tupDesc,
			    qual->qual.complex.operands[i])) {
			/*
			 * this is an AND, so if any subqualification
			 * is true, then the whole thing is true
			 */
			return(true);
		    }
		}
		return(false);
		break;
	    case NOT:
		if (prs2StubTestTuple(tuple,
			buffer,
			tupDesc,
			qual->qual.complex.operands[0])) {
		    return(false);
		} else {
		    return(true);
		}
		break;
	    default:
		elog(WARN, "prs2StubTestTuple: Illegal boolOper");
	}
    } else {
	elog(WARN, "prs2StubTestTuple: Illegal qualType");
    }
}

/*--------------------------------------------------------------------
 *
 * prs2SimpleQualTestTuple
 *
 * test if a tuple satisfies the given "simple qualification"
 *--------------------------------------------------------------------
 */
static
bool
prs2SimpleQualTestTuple(tuple, buffer, tupDesc, simplequal)
HeapTuple tuple;
Buffer buffer;
TupleDescriptor tupDesc;
Prs2SimpleStubQualData simplequal;
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
		simplequal.attrNo,
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
    
    result = (int) fmgr(simplequal.operator, value, simplequal.constData);

    if (result) {
	return(true);
    } else {
	return(false);
    }
}
