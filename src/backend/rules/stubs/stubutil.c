/*======================================================================
 *
 * FILE:
 * stubutil.c
 *
 * IDENTIFICATION
 * $Header$
 *
 * DESCRIPTION:
 * 
 * some general utility routines for the world famous 'stub records'
 *
 *======================================================================
 */

#include <stdio.h>
#include <strings.h>
#include "utils/log.h"
#include "tmp/datum.h"
#include "rules/prs2.h"
#include "rules/prs2stub.h"

extern char *palloc();

/*-------------------------------------------------------------------
 *
 * prs2StubQualIsEqual
 *
 * Returns true if the two stub qualifications given are
 * equal.
 *
 * NOTE: see restrictions in 'datumIsEqual' for testing whether
 * two datums are equal...
 *
 *-------------------------------------------------------------------
 */
bool
prs2StubQualIsEqual(q1, q2)
Prs2StubQual q1;
Prs2StubQual q2;
{
    int i;

#ifdef STUB_DEBUG
    /*
     * sanity check...
     */
    if (q1 == NULL || q2 == NULL) {
	elog(WARN, "prs2StubQualIsEqual: called with NULL qual");
    }
#endif STUB_DEBUG

    /*
     * If the types of qualification are different, then
     * (obviously) they are not equal
     */
    if (q1->qualType != q2->qualType) {
	return(false);
    }

    if (q1->qualType == PRS2_NULL_STUBQUAL) {
	/*
	 * Both quals are null, therefore they are equal
	 */
	return(true);
    } else if (q1->qualType == PRS2_SIMPLE_STUBQUAL) {
	/*
	 * both quals are of the "simple" type.
	 * All their fields must be equal.
	 * NOTE: take a look in 'datumIsEqual' for retrictions
	 * in the datum equality test...
	 */
	if (q1->qual.simple.attrNo != q2->qual.simple.attrNo) {
	    return(false);
	}
	if (q1->qual.simple.operator != q2->qual.simple.operator) {
	    return(false);
	}
	if (q1->qual.simple.constType != q2->qual.simple.constType) {
	    return(false);
	}
	if (q1->qual.simple.constByVal != q2->qual.simple.constByVal) {
	    return(false);
	}
	if (q1->qual.simple.constLength != q2->qual.simple.constLength) {
	    return(false);
	}
	if (!datumIsEqual(
			q1->qual.simple.constType,
			q1->qual.simple.constByVal,
			q1->qual.simple.constLength,
			q1->qual.simple.constData,
			q2->qual.simple.constData)) {
	    return(false);
	}
	/*
	 * at last, everything is equal!
	 */
	return(true);
    } else if (q1->qualType == PRS2_COMPLEX_STUBQUAL) {
	/*
	 * OK now, don't panic, all we have to do is test
	 * whether the boolean operators of the complex
	 * qualifications are the same and that their
	 * children are equal too...
	 *
	 * NOTE: The qualifications must be structured
	 * in exactly the same way, e.g. the children must
	 * appear in the same order, etc. in order to be
	 * assumed equal. This routine does not handle isomorphic
	 * quals correctly! (it thinks they are different!).
	 */
	if (q1->qual.complex.boolOper != q1->qual.complex.boolOper) {
	    return(false);
	}
	if (q1->qual.complex.nOperands!= q1->qual.complex.nOperands) {
	    return(false);
	}
	for (i=0; i<q1->qual.complex.nOperands; i++) {
	    if (!prs2StubQualIsEqual(q1->qual.complex.operands[i],
				     q2->qual.complex.operands[i])) {
		/*
		 * Two "children" subquals are different, therefore their
		 * parents are assumed different too..
		 */
		return(false);
	    }
	}
	return(true);
    }
}

/*-------------------------------------------------------------------
 *
 * prs2OneStubIsEqual
 *
 * return true if the two 'Prs2OneStub's are the same
 *
 * NOTE1: we don't check the 'counter' field
 * NOTE2: we don't check the 'lock' field either, because if
 * 2 stub records have the same 'ruleId' and 'stubId' they are
 * guaranteed to have the same lock!
 *-------------------------------------------------------------------
 */
bool
prs2OneStubIsEqual(stub1, stub2)
Prs2OneStub stub1;
Prs2OneStub stub2;
{
    int i;

    if (stub1->ruleId != stub2->ruleId) {
	return(false);
    }
    if (stub1->stubId != stub2->stubId) {
	return(false);
    }

    /*
     * Test the qualifications...
     */
    if (!prs2StubQualIsEqual(stub1->qualification, stub2->qualification)) {
	return(false);
    }

    return(true);
}

/*-------------------------------------------------------------------
 *
 * prs2SearchStub
 *
 * Given a 'Prs2Stub' search and return he index to the 'Prs2OneStub' which
 * is equal to the given 'Prs2OneStub'
 * If no such 'Prs2OneStub; exists, then return -1
 *
 *-------------------------------------------------------------------
 */
int
prs2SearchStub(stubs, oneStub)
Prs2Stub stubs;
Prs2OneStub oneStub;
{
    int i;
    for (i=0; i<stubs->numOfStubs; i++) {
	if (prs2OneStubIsEqual(oneStub, &(stubs->stubRecords[i]))) {
	    /*
	     * We've found it! Return its index
	     */
	    return(i);
	}
    }
    /*
     * Nop! We didn't find it!
     */
    return(-1);
}

/*-------------------------------------------------------------------
 *
 * prs2AddOneStub
 *
 * Add a new stub record.
 * Note that it is possible to have many copies of the same
 * stub record. Therefore before phusically appending the new stub,
 * see if a similar copy already exists. If yes, then
 * just increment the appropriate counter.
 *
 * NOTE: We do NOT make a copy of the 'newStub'. So do NOT pfree
 * anything contained in there !
 *-------------------------------------------------------------------
 */
void
prs2AddOneStub(oldStubs, newStub)
Prs2Stub oldStubs;
Prs2OneStub newStub;
{
    int i;
    Prs2OneStub s;
    int indx;
    Prs2OneStub *temp;
    int n;

    /*
     * does this stub already exist ?
     */
    indx = prs2SearchStub(oldStubs, newStub);
    if (indx >=0) {
	/*
	 * this record already exists.
	 * Increment its counter and return
	 */
	oldStubs->stubRecords[i]->counter += 1;
	return;
    }
    /*
     * this stub record did not exist. We must create a
     * completely new entry...
     * NOTE: 'Prs2Stub->stubRecords' is an array of POINTERS to
     * the 'Prs2OneStubData' records.
     */
    n = oldStubs->numOfStubs;
    temp = (Prs2OneStub *) palloc((n+1) * sizeof(Prs2OneStub));
    for (i=0; i<n; i++) {
	temp[i] = oldStubs->stubRecords[i];
    }
    temp[n] = newStub;
    /*
     * pfree the old array of pointers and replace it with the
     * new one 'temp'.
     */
    if (oldStubs->numOfStubs > 0 && oldStubs->stubRecords != NULL)
	pfree(oldStubs->stubRecords);
    oldStubs->stubRecords = temp;
    oldStubs->numOfStubs+= 1;

}

/*-------------------------------------------------------------------
 *
 * prs2DeleteOneStub
 *
 * Delete a stub record.
 *-------------------------------------------------------------------
 */
void
prs2DeleteOneStub(oldStubs, oldStub)
Prs2Stub oldStubs;
Prs2OneStub oldStub;
{
    int i;
    Prs2OneStub s;
    int n;
    int indx;

    /*
     * does this stub already exist ?
     */
    indx = prs2SearchStub(oldStubs, oldStub);
    if (indx < 0) {
	/*
	 * No such record existed. complain!
	 */
	elog(WARN,"prs2DeleteOneStub: stub not found");
    }

    /*
     * So, this record already existed.
     * Decrement its counter.
     */
    s = oldStubs->stubRecords[i];
    (s->counter)--;

    /*
     * if the counter is non zero, we are done
     */
    if (s->counter > 0) {
	return;
    }

    /*
     * else we must physically delete this record
     * 
     * Move the pointer entry of the last stub record to the one to be
     * deleted and decrement the number of stub records.
     * Also free the stub record.
     */
    n = oldStubs->numOfStubs;
    prs2FreeOneStub(oldStubs->stubRecords[indx]);
    oldStubs->stubRecords[indx] = oldStubs->stubRecords[n-1];
    oldStubs->numOfStubs -= 1;

}

/*-----------------------------------------------------------------------
 *
 * prs2MakeStub
 *
 * create an (empty) stub record
 *
 *-----------------------------------------------------------------------
 */
Prs2Stub
prs2MakeStub()
{
    Prs2Stub res;

    res = (Prs2Stub) palloc(sizeof(Prs2StubData));
    if (res == NULL) {
	elog(WARN, "prs2MakeStub: Out of memory\n");
    }
    res->numOfStubs = 0;
    res->stubRecords = NULL;

    return(res);
}


/*-----------------------------------------------------------------------
 *
 * prs2FreeStub
 *
 * free all space occupied by a stub record
 *-----------------------------------------------------------------------
 */
void
prs2FreeStub(relstub)
Prs2Stub relstub;
{
    int i,j;
    Prs2OneStub oneStub;

    Assert(PointerIsValid(relstub));

    /*
     * One by one free all the 'Prs2OneStub' structs.
     */
    for (i=0; i<relstub->numOfStubs; i++) {
	oneStub = relstub->stubRecords[i];
	prs2FreeOneStub(oneStub);
    }

    if (relstub->numOfStubs>0 && relstub->stubRecords != NULL) {
	pfree(relstub->stubRecords);
    }

#ifdef STUB_DEBUG
    bzero(relstub, sizeof(Prs2Stub));
#endif STUB_DEBUG
    pfree(relstub);
}

/*-----------------------------------------------------------------------
 *
 * prs2CopyStub
 *
 * Make a copy of the given stub.
 *
 * NOTE: We copy EVERYTHING!
 *-----------------------------------------------------------------------
 */
Prs2Stub
prs2CopyStub(stubs)
Prs2Stub stubs;
{
    Prs2Stub res;
    int i;

    res = prs2MakeStub();

    res->numOfStubs = stubs->numOfStubs;

    for (i=0; i<stubs->numOfStubs; i++) {
	res->stubRecords[i] = prs2CopyOneStub(stubs->stubRecords[i]);
    }

    return(res);
}


/*-----------------------------------------------------------------------
 *
 * prs2MakeOneStub
 *
 * create an initialized Prs2OneStub record.
 *
 *-----------------------------------------------------------------------
 */
Prs2OneStub
prs2MakeOneStub()
{
    Prs2OneStub res;

    res = (Prs2OneStub) palloc(sizeof(Prs2OneStubData));
    if (res == NULL) {
	elog(WARN, "prs2MakeOneStub: Out of memory\n");
    }

    return(res);
}

/*-----------------------------------------------------------------------
 *
 * prs2FreeOneStub
 *
 * free the space allocated by a call to `prs2MakeOneStub'
 * Also free the stub's qualification.
 *-----------------------------------------------------------------------
 */
void
prs2FreeOneStub(oneStub)
Prs2OneStub oneStub;
{
    /*
     * First free the stub qual
     */
    prs2FreeStubQual(oneStub->qualification);

    /*
     * then free the stub itself
     */
#ifdef STUB_DEBUG
    bzero(oneStub, sizeof(Prs2OneStub));
#endif STUB_DEBUG
    pfree(oneStub);
}

/*-----------------------------------------------------------------------
 *
 * prs2CopyOneStub
 *
 * Make and return a copy of the given 'Prs2OneStub'.
 * NOTE: copy its qualification too...
 *-----------------------------------------------------------------------
 */
Prs2OneStub
prs2CopyOneStub(oneStub)
Prs2OneStub oneStub;
{
    Prs2OneStub res;

    res = prs2MakeOneStub();

    res->ruleId = oneStub->ruleId;
    res->stubId = oneStub->stubId;
    res->counter = oneStub->counter;
    res->lock = prs2CopyLocks(oneStub->lock);
    res->qualification = prs2CopyStubQual(oneStub->qualification);

    return(res);
}

/*-----------------------------------------------------------------------
 *
 * prs2MakeStubQual
 *
 * Create an empty 'Prs2StubQualData' record
 *-----------------------------------------------------------------------
 */
Prs2StubQual
prs2MakeStubQual()
{
    Prs2StubQual res;

    res = (Prs2StubQual) palloc(sizeof(Prs2StubQualData));
    if (res==NULL) {
	elog(WARN, "prs2MakeStubQual: Out of memory!");
    }

    return(res);
}

/*-----------------------------------------------------------------------
 *
 * prs2FreeStubQual
 *
 * Free the space occupied by a stub's qualification.
 *
 *-----------------------------------------------------------------------
 */

void
prs2FreeStubQual(qual)
Prs2StubQual qual;
{
    int i;

    /*
     * first free all children nodes (if any)
     */
    if (qual->qualType == PRS2_SIMPLE_STUBQUAL) {
	/*
	 * should we free the bytes pointed by 'constData' too ?
	 * I think yes!
	 */
	datumFree(qual->qual.simple.constType,
			qual->qual.simple.constByVal,
			qual->qual.simple.constLength,
			qual->qual.simple.constData);
    } else if (qual->qualType == PRS2_COMPLEX_STUBQUAL) {
	/*
	 * free the operands
	 */
	for (i=0; i<qual->qual.complex.nOperands; i++) {
	    prs2FreeStubQual(qual->qual.complex.operands[i]);
	}
	/*
	 * free the array of pointers to the operands
	 */
#ifdef STUB_DEBUg
	bzero(qual->qual.complex.operands, PSIZE(qual->qual.complex.operands));
#endif STUB_DEBUG
	pfree(qual->qual.complex.operands);
    } else if (qual->qualType == PRS2_NULL_STUBQUAL) {
	/*
	 * do nothing
	 */
    } else {
	elog(WARN, "prs2FreeStubQual: Illegal qualType");
    }
    
    /*
     * Now free the 'qual' itself...
     */
#ifdef STUB_DEBUg
    bzero(qual, sizeof(qual));
#endif STUB_DEBUG
    pfree(qual);
}

/*-----------------------------------------------------------------------
 *
 * prs2CopyStubQual
 *
 * Make a copy of the stub's qualification.
 *
 *-----------------------------------------------------------------------
 */

Prs2StubQual
prs2CopyStubQual(qual)
Prs2StubQual qual;
{
    Prs2StubQual res;
    int i;
    int size;

    if (qual->qualType == PRS2_NULL_STUBQUAL) {
	/*
	 * easy!
	 */
	res = prs2MakeStubQual();
	res->qualType = PRS2_NULL_STUBQUAL;
    } else if (qual->qualType == PRS2_SIMPLE_STUBQUAL) {
	/*
	 */
	res = prs2MakeStubQual();
	res->qualType = PRS2_SIMPLE_STUBQUAL;
	res->qual.simple.attrNo = qual->qual.simple.attrNo;
	res->qual.simple.operator = qual->qual.simple.operator;
	res->qual.simple.constType = qual->qual.simple.constType;
	res->qual.simple.constByVal = qual->qual.simple.constByVal;
	res->qual.simple.constLength = qual->qual.simple.constLength;
	res->qual.simple.constData =
		datumCopy( qual->qual.simple.constType,
			qual->qual.simple.constByVal,
			qual->qual.simple.constLength,
			qual->qual.simple.constData);
    } else if (qual->qualType == PRS2_COMPLEX_STUBQUAL) {
	res = prs2MakeStubQual();
	res->qualType = PRS2_COMPLEX_STUBQUAL;
	res->qual.complex.nOperands = qual->qual.complex.nOperands;
	size = qual->qual.complex.nOperands * sizeof(Prs2StubQual);
	res->qual.complex.operands = (Prs2StubQual *)palloc(size);
	/*
	 * Now copy the children one by one...
	 */
	for (i=0; i<qual->qual.complex.nOperands; i++) {
	    res->qual.complex.operands[i] =
			    prs2CopyStubQual(qual->qual.complex.operands[i]);
	}
    } else {
	elog(WARN, "prs2FreeStubQual: Illegal qualType");
    }

    return(res);
}
