/*========================================================================
 *
 * FILE:
 * stubraw.c
 *
 * IDENTIFICATION:
 * $Header$
 *
 * DESCRIPTION:
 *
 * Routines that transform a 'Prs2RawStub' to a 'Prs2Stub' and vice versa.
 *
 *========================================================================
 */

#include <stdio.h>
#include "log.h"
#include "prs2.h"
#include "prs2stub.h"

char *palloc();

/*-----------------------------------------------------------------------
 *
 * prs2StubToRawStub
 *
 * given a 'Prs2Stub' (which is a spaghetti of pointers)
 * transform it to a stream of bytes suitable for disk storage
 *
 */
Prs2RawStub
prs2StubToRawStub(stub)
Prs2Stub stub;
{
    long size;
    Prs2RawStub res;
    int i,j;
    char *s;

    /*
     * calculate the number fo bytes needed to hold
     * the raw representation ofthis stub record.
     */
    size = sizeof(Prs2StubData) - sizeof(Prs2OneStub);

    for (i=0; i< stub->numOfStubs; i++) {
	/*
	 * calculate the size of each individual `Prs2OneStub'
	 * and add it ti the total size.
	 * Start with the fixed size part of the struct.
	 */
	size += sizeof(Prs2OneStubData) - sizeof(RuleLock)
		    - sizeof(Prs2SimpleQual);
	/*
	 * now add the rule lock size
	 */
	size += prs2LockSize(
		    prs2GetNumberOfLocks(stub->stubRecords[i].lock));
	/*
	 * finally add the size of each one of the
	 * `PrsSimpleQualData'
	 */
	for (j=0; j<stub->stubRecords[i].numQuals; j++) {
	    size += sizeof(Prs2SimpleQualData) - sizeof(char *);
	    size += stub->stubRecords[i].qualification[j].constLength;
	}
    }

    /*
     * allocate a big enough `Prs2RawStubData' struct to hold the
     * whole thing...
     */
    res = (Prs2RawStub) palloc(sizeof(Prs2RawStubData) -1 + size);
    if (res == NULL) {
	elog(WARN,"prs2StubToRawStub: Out of memory");
    }
    res->size = size;

    /*
     * now copy the whole thing to the res->data;
     */
    s = &(res->data[0]);

    bcopy((char *) &(stub->numOfStubs), s, sizeof(stub->numOfStubs));
    s += sizeof(stub->numOfStubs);

    for (i=0; i< stub->numOfStubs; i++) {
	/*
	 * copy the fixed size part of the record
	 */
	size += sizeof(Prs2OneStubData) - sizeof(RuleLock)
		    - sizeof(Prs2SimpleQual);
	bcopy((char *)&(stub->stubRecords[i]), s, size);
	s += size;
	/*
	 * now copy the rule lock
	 */
	size += prs2LockSize(
		    prs2GetNumberOfLocks(stub->stubRecords[i].lock));
	bcopy((char *) stub->stubRecords[i].lock, s, size);
	s+=size;
	/*
	 * finally copy each one of the
	 * `PrsSimpleQualData'
	 */
	for (j=0; j<stub->stubRecords[i].numQuals; j++) {
	    /*
	     * fixed part first...
	     */
	    size += sizeof(Prs2SimpleQualData) - sizeof(char *);
	    bcopy((char *) &(stub->stubRecords[i].qualification[j]), s, size);
	    s+=size;
	    size += stub->stubRecords[i].qualification[j].constLength;
	    bcopy((char *) &(stub->stubRecords[i].qualification[j].constData),
		    s, size);
	    s += size;
	}
    }

    /* 
     * OK we are done!
     */
    return(res);
}


/*----------------------------------------------------------------------
 *
 * prs2RawStubToStub
 *
 * given a stream of bytes (like the one created by
 * 'prs2StubToRawStub') recreate the original stub record
 *
 */
Prs2Stub
prs2RawStubToStub(data)
char *data;
{
    Prs2Stub stub;
    Prs2OneStub oneStub;
    Prs2SimpleQual oneQual;
    long size;
    int i,j;
    char *s;
    Prs2LocksData ruleLock;

    /*
     * create a new stub record
     */
    stub = (Prs2Stub) palloc(sizeof(Prs2StubData));
    if (stub == NULL) {
	elog(WARN, "prs2RawStubToStub: Out of memory");
    }
    size = sizeof(Prs2StubData) - sizeof(Prs2OneStub);
    bcopy(data, (char *)&(stub->numOfStubs), size);
    data += size;

    size = stub->numOfStubs * sizeof(Prs2OneStubData);
    stub->stubRecords = (Prs2OneStub) palloc(size);
    if (stub->stubRecords == NULL) {
	elog(WARN, "prs2RawStubToStub: Out of memory");
    }
    
    for (i=0; i< stub->numOfStubs; i++) {
	oneStub = & (stub->stubRecords[i]);
	/*
	 */
	size = sizeof(Prs2OneStubData) - sizeof(RuleLock)
		    - sizeof(Prs2SimpleQual);
	bcopy(data, (char *) oneStub, size);
	data += size;

	/*
	 * now copy the rule lock
	 * XXX: NOTE:
	 * In order to copy the rule lock, we have to find its
	 * size. But in order to find its size we must read
	 * the 'numberOfLocks' field.
	 * We can not just cast 'data' to a RuleLock because
	 * of alignment problems, so we must copy it.
	 */
	bcopy(data, (char *) &ruleLock,
		sizeof(Prs2LocksData)-sizeof(Prs2OneLockData));
	size = prs2LockSize(prs2GetNumberOfLocks(&ruleLock));
	oneStub->lock = (RuleLock) palloc(size);
	if (oneStub->lock == NULL) {
	    elog(WARN, "prs2RawStubToStub: Out of memory");
	}
	bcopy(data, *(oneStub->lock), size);
	data += size;

	size = oneStub->numQuals * sizeof(Prs2SimpleQualData);
	oneStub->qualification = (Prs2SimpleQual) palloc(size);
	if (oneStub->qualification == NULL) {
	    elog(WARN, "prs2RawStubToStub: Out of memory");
	}
	for (j=0; j<oneStub->numQuals; j++) {
	    oneQual = & (oneStub->qualification[j]);
	    size = sizeof(Prs2SimpleQualData) - sizeof(char *);
	    bcopy(data, (char *) oneQual, size);
	    data += size;

	    size = oneQual->constLength;
	    oneQual->constData = palloc(size);
	    if (oneQual->constData == NULL) {
		elog(WARN, "prs2RawStubToStub: Out of memory");
	    }
	    bcopy(data, oneQual->constData, size);
	    data += size;
	}
    }

    /*
     * At last! we are done...
     */
    return(stub);
}
