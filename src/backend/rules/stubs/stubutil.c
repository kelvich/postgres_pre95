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
#include "utils/palloc.h"
#include "tmp/postgres.h"
#include "tmp/datum.h"
#include "rules/prs2.h"
#include "rules/prs2stub.h"
#include "nodes/primnodes.h"
#include "nodes/primnodes.a.h"

extern Node CopyObject();
extern bool _equalLispValue();

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
LispValue q1;
LispValue q2;
{
    if (_equalLispValue(q1, q2))
	return(true);
    else
	return(false);
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
	if (prs2OneStubIsEqual(oneStub, stubs->stubRecords[i])) {
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
	pfree((Pointer)oldStubs->stubRecords);
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
     * Don't forget to free the 'oneStub'
     */
    n = oldStubs->numOfStubs;
    pfree((Pointer)oldStubs->stubRecords[indx]);
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
#ifdef STUB_DEBUG
	bzero(oneStub, sizeof(Prs2OneStubData));
#endif STUB_DEBUG
	pfree((Pointer)oneStub);
    }

    if (relstub->numOfStubs>0 && relstub->stubRecords != NULL) {
	pfree((Pointer)relstub->stubRecords);
    }

#ifdef STUB_DEBUG
    bzero(relstub, sizeof(Prs2StubData));
#endif STUB_DEBUG
    pfree((Pointer)relstub);
}

/*----------------------------------------------------------------------
 * prs2RemoveStubsOfRule
 *
 * Remove the stubs of the given rule (in place).
 * If no stubs of this rule were found return false, else true.
 *----------------------------------------------------------------------
 */
bool
prs2RemoveStubsOfRule(stubs, ruleId)
Prs2Stub stubs;
ObjectId ruleId;
{
    int i;
    Prs2OneStub thisStub;
    bool result;

    result = false;
    i=0;
    while (i<stubs->numOfStubs) {
	thisStub = stubs->stubRecords[i];
	if (thisStub->ruleId == ruleId) {
	    /*
	     * remove this stub by copying over it the last stub
	     * The result is more or less similar to moving all the
	     * stubs above the one to be deleted, one place down.
	     * So, we must NOT increment 'i' !
	     */
	    result = true;
	    stubs->stubRecords[i] = stubs->stubRecords[stubs->numOfStubs-1];
	    pfree((Pointer)thisStub);
	    stubs->numOfStubs -=1;
	} else {
	    /*
	     * examine the next stub...
	     */
	    i++;
	}
    }

    return(result);
}

