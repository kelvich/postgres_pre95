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
#include "datum.h"
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
prs2StubToRawStub(relstub)
Prs2Stub relstub;
{
    long size;
    Prs2RawStub res;
    Prs2OneStub oneStub;
    Prs2SimpleQual qual;
    int i,j;
    char *s;

    /*
     * calculate the number fo bytes needed to hold
     * the raw representation of this stub record.
     */
    size = sizeof(relstub->numOfStubs);

    for (i=0; i< relstub->numOfStubs; i++) {
	/*
	 * calculate the size of each individual `Prs2OneStub'
	 * and add it ti the total size.
	 * Start with the fixed size part of the struct.
	 */
	oneStub = &(relstub->stubRecords[i]);
	size = size +
		sizeof(oneStub->ruleId) +
		sizeof(oneStub->stubId) +
		sizeof(oneStub->counter) +
		sizeof(oneStub->numQuals);
	/*
	 * now add the rule lock size
	 */
	size = size +
		prs2LockSize( prs2GetNumberOfLocks(oneStub->lock));
	/*
	 * finally add the size of each one of the
	 * `PrsSimpleQualData'
	 */
	for (j=0; j<oneStub->numQuals; j++) {
	    qual = &(oneStub->qualification[j]);
	    /*
	     * first the fixed size part of 'Prs2SimpleQualData' struct.
	     */
	    size = size +
		    sizeof(qual->attrNo) +
		    sizeof(qual->operator) +
		    sizeof(qual->constType) +
		    sizeof(qual->constByVal) +
		    sizeof(qual->constLength);
	    /*
	     * then the variable length size (the actual value of
	     * the constant)
	     */
	    size += prs2GetDatumSize(qual->constType,
				qual->constByVal,
				qual->constLength,
				qual->constData);
	}
    }

    /*
     * allocate a big enough `Prs2RawStubData' struct to hold the
     * whole thing...
     *
     * NOTE: Just as like in the "varlena" structure, the "size" field
     * in the Prs2RawStubData is the sum of its own size + the (variable)
     * size of the data that is following.
     */
    size = size + sizeof(res->vl_len);
    res = (Prs2RawStub) palloc(size);
    if (res == NULL) {
	elog(WARN,"prs2StubToRawStub: Out of memory");
    }
    res->vl_len = size;

    /*
     * now copy the whole thing to the res->vl_dat;
     */
    s = &(res->vl_dat[0]);

    bcopy((char *) &(relstub->numOfStubs), s, sizeof(relstub->numOfStubs));
    s += sizeof(relstub->numOfStubs);

    for (i=0; i< relstub->numOfStubs; i++) {
	oneStub = &(relstub->stubRecords[i]);
	/*
	 * copy the fixed size part of the record
	 */
	bcopy((char *)&(oneStub->ruleId), s, sizeof(oneStub->ruleId));
	s += sizeof(oneStub->ruleId);
	bcopy((char *)&(oneStub->stubId), s, sizeof(oneStub->stubId));
	s += sizeof(oneStub->stubId);
	bcopy((char *)&(oneStub->counter), s, sizeof(oneStub->counter));
	s += sizeof(oneStub->counter);
	bcopy((char *)&(oneStub->numQuals), s, sizeof(oneStub->numQuals));
	s += sizeof(oneStub->numQuals);
	/*
	 * now copy the rule lock
	 */
	size = prs2LockSize(prs2GetNumberOfLocks(oneStub->lock));
	bcopy((char *) relstub->stubRecords[i].lock, s, size);
	s+=size;
	/*
	 * finally copy each one of the
	 * `PrsSimpleQualData'
	 */
	for (j=0; j<relstub->stubRecords[i].numQuals; j++) {
	    qual = &(oneStub->qualification[j]);
	    /*
	     * fixed part first...
	     */
	    bcopy((char *)&(qual->attrNo), s, sizeof(qual->attrNo));
	    s += sizeof(qual->attrNo);
	    bcopy((char *)&(qual->operator), s, sizeof(qual->operator));
	    s += sizeof(qual->operator);
	    bcopy((char *)&(qual->constType), s, sizeof(qual->constType));
	    s += sizeof(qual->constType);
	    bcopy((char *)&(qual->constByVal), s, sizeof(qual->constByVal));
	    s += sizeof(qual->constByVal);
	    bcopy((char *)&(qual->constLength), s, sizeof(qual->constLength));
	    s += sizeof(qual->constLength);
	    /*
	     * now the value of the constant iteslf
	     */
	    size = prs2GetDatumSize(qual->constType,
				qual->constByVal,
				qual->constLength,
				qual->constData);
	    if (qual->constByVal) {
		bcopy((char *)&(qual->constData), s, size);
	    } else {
		bcopy((char *)DatumGetPointer(qual->constData), s, size);
	    }
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
prs2RawStubToStub(rawStub)
Prs2RawStub rawStub;
{
    Prs2Stub relstub;
    Prs2OneStub oneStub;
    Prs2SimpleQual qual;
    long size;
    int i,j;
    char *data;
    char *s;
    Prs2LocksData ruleLock;

    /*
     * create a new stub record
     */
    relstub = (Prs2Stub) palloc(sizeof(Prs2StubData));
    if (relstub == NULL) {
	elog(WARN, "prs2RawStubToStub: Out of memory");
    }

    /*
     * copy the number of individual stub records
     */
    data = VARDATA(rawStub);
    bcopy(data, (char *)&(relstub->numOfStubs), sizeof(relstub->numOfStubs));
    data += sizeof(relstub->numOfStubs);

    /*
     * allocate space for the `relstub->numOfStubs' records of
     *type 'Prs2OneStubData'
     */
    if (relstub->numOfStubs > 0 ) {
	size = relstub->numOfStubs * sizeof(Prs2OneStubData);
	relstub->stubRecords = (Prs2OneStub) palloc(size);
	if (relstub->stubRecords == NULL) {
	    elog(WARN, "prs2RawStubToStub: Out of memory");
	}
    }
    
    for (i=0; i< relstub->numOfStubs; i++) {
	oneStub = & (relstub->stubRecords[i]);
	/*
	 * copy the fixed part of the 'Prs2OneStub' first.
	 */
        bcopy(data, (char *)&(oneStub->ruleId), sizeof(oneStub->ruleId));
        data += sizeof(oneStub->ruleId);
        bcopy(data, (char *)&(oneStub->stubId), sizeof(oneStub->stubId));
        data += sizeof(oneStub->stubId);
        bcopy(data, (char *)&(oneStub->counter), sizeof(oneStub->counter));
        data += sizeof(oneStub->counter);
        bcopy(data, (char *)&(oneStub->numQuals), sizeof(oneStub->numQuals));
        data += sizeof(oneStub->numQuals);

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
	bcopy(data, (char*)(oneStub->lock), size);
	data += size;

	/*
	 * now allocate space for the qualification(s)
	 */
	if (oneStub->numQuals > 0) {
	    size = oneStub->numQuals * sizeof(Prs2SimpleQualData);
	    oneStub->qualification = (Prs2SimpleQual) palloc(size);
	    if (oneStub->qualification == NULL) {
		elog(WARN, "prs2RawStubToStub: Out of memory");
	    }
	}
	for (j=0; j<oneStub->numQuals; j++) {
	    /*
	     * copy the qualifications one by one
	     */
	    qual = & (oneStub->qualification[j]);

	    /*
	     * fixed part first
	     */
            bcopy(data, (char *)&(qual->attrNo), sizeof(qual->attrNo));
            data += sizeof(qual->attrNo);
            bcopy(data, (char *)&(qual->operator), sizeof(qual->operator));
            data += sizeof(qual->operator);
            bcopy(data, (char *)&(qual->constType), sizeof(qual->constType));
            data += sizeof(qual->constType);
            bcopy(data, (char *)&(qual->constByVal), sizeof(qual->constByVal));
            data += sizeof(qual->constByVal);
            bcopy(data, (char *)&(qual->constLength),sizeof(qual->constLength));
            data += sizeof(qual->constLength);

            size = prs2GetDatumSize(qual->constType,
                                qual->constByVal,
                                qual->constLength,
                                qual->constData);
	    if (qual->constByVal) {
		bcopy(data, (char *) &(qual->constData), size);
		data += size;
	    } else {
		if (size > 0) {
		    s = palloc(size);
		    if (s == NULL) {
			elog(WARN, "prs2RawStubToStub: Out of memory");
		    }
		}
		bcopy(data, s, size);
		data+=size;
		qual->constData = PointerGetDatum(s);
	    }
	}
    }

    /*
     * At last! we are done...
     */
    return(relstub);
}
