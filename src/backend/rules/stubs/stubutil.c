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
#include "log.h"
#include "prs2.h"
#include "prs2stub.h"

extern char *palloc();

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
 */
bool
prs2OneStubIsEqual(stub1, stub2)
Prs2OneStub stub1;
Prs2OneStub stub2;
{
    int i;
    Prs2SimpleQual qual1, qual2;

    if (stub1->ruleId != stub2->ruleId) {
	return(false);
    }
    if (stub1->ruleId != stub2->stubId) {
	return(false);
    }
    if (stub1->ruleId != stub2->numQuals) {
	return(false);
    }

    /*
     * XXX!!!!
     * NOTE: we assume that the 'simple quals' must be in the same
     * order. Is this a good assumption ???
     */
    for (i=0; i<stub1->numQuals; i++) {
	qual1 = &(stub1->qualification[i]);
	qual2 = &(stub2->qualification[i]);
	if (qual1->attrNo != qual2->attrNo) {
	    return(false);
	}
	if (qual1->operator != qual2->operator) {
	    return(false);
	}
	if (qual1->constType != qual2->constType) {
	    return(false);
	}
	if (qual1->constLength != qual2->constLength) {
	    return(false);
	}
	if (bcmp(qual1->constData, qual2->constData, qual1->constLength)){
	    return(false);
	}
    }

    return(true);
}

/*-------------------------------------------------------------------
 *
 * prs2SearchStub
 *
 * Given a 'Prs2Stub' search and return the 'Prs2OneStub' which
 * is equal to the given 'Prs2OneStub'
 * If no such record is found, return NULL
 */
Prs2OneStub
prs2SearchStub(stubs, oneStub)
Prs2Stub stubs;
Prs2OneStub oneStub;
{
    int i;
    for (i=0; i<stubs->numOfStubs; i++) {
	if (prs2OneStubIsEqual(oneStub, &(stubs->stubRecords[i]))) {
	    /*
	     * We've found it! Return a ponter to it.
	     */
	    return(&(stubs->stubRecords[i]));
	}
    }
    /*
     * Nop! We didn't find it!
     */
    return(NULL);
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
 */
void
prs2AddOneStub(oldStubs, newStub)
Prs2Stub oldStubs;
Prs2OneStub newStub;
{
    int i;
    Prs2OneStub s;
    Prs2OneStub temp;
    int n;

    /*
     * does this stub already exist ?
     */
    s = prs2SearchStub(oldStubs, newStub);
    if (s!=NULL) {
	/*
	 * this record already exists.
	 * Increment its counter.
	 */
	(s->counter)++;
    }
    /*
     * this stub record did not exist. We must create a
     * completely new entry...
     */
    n = oldStubs->numOfStubs;
    temp = (Prs2OneStub) palloc((n+1) * sizeof(Prs2OneStubData));
    bcopy((char *) oldStubs->stubRecords,
	    (char *) temp,
	    n * sizeof(Prs2OneStubData));
    bcopy(newStub,
	(char *) temp[n],
	sizeof(Prs2OneStubData));
    pfree(oldStubs->stubRecords);
    oldStubs->stubRecords = temp;
    oldStubs->stubRecords += 1;

}

/*-------------------------------------------------------------------
 *
 * prs2DeleteOneStub
 *
 * Delete a stub record.
 */
void
prs2DeleteOneStub(oldStubs, oldStub)
Prs2Stub oldStubs;
Prs2OneStub oldStub;
{
    int i;
    Prs2OneStub s;
    int n;

    /*
     * does this stub already exist ?
     */
    s = prs2SearchStub(oldStubs, oldStub);
    if (s==NULL) {
	/*
	 * No such record existed. complain!
	 */
	elog(WARN,"prs2DeleteOneStub: stub not found");
    }

    /*
     * So, this record already existed.
     * Decrement its counter.
     */
    (s->counter)++;

    /*
     * if the counter is non zero, we are done
     */
    if (s->counter > 0) {
	return;
    }

    /*
     * else we must physically delete this record
     * Move the last record to its place and decrement
     * the counter of stub records.
     *
     * NOTE: we don't free any memory...
     */

    n = oldStubs->numOfStubs;
    bcopy((char *) &(oldStubs->stubRecords[n-1]), s, sizeof(Prs2OneStubData));
    oldStubs->stubRecords -= 1;

}

/*-----------------------------------------------------------------------
 *
 * prs2MakeStub
 *
 * create an (empty) stub record
 *
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
 */
void
prs2FreeStub(stub)
Prs2Stub stub;
{
    int i,j;
    Prs2OneStub oneStub;
    Prs2SimpleQual oneQual;

    Assert(PointerIsValid(stub));

    for (i=0; i<stub->numOfStubs; i++) {
	oneStub = & (stub->stubRecords[i]);
	pfree(oneStub->lock);
	for (j=0; j<oneStub->numQuals; j++) {
	    oneQual = &(oneStub->qualification[j]);
	    pfree(oneQual->constData);
	}
	pfree(oneStub->qualification);
    }
    if (stub->stubRecords != NULL) {
	pfree(stub->stubRecords);
    }

    pfree(stub);
}

