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
#include "tmp/postgres.h"
#include "parser/parse.h"       /* for the AND, NOT, OR */
#include "utils/log.h"
#include "rules/prs2.h"
#include "rules/prs2stub.h"
#include "tmp/datum.h"
#include "nodes/primnodes.h"
#include "nodes/primnodes.a.h"

extern char *palloc();
extern Const RMakeConst();
extern Param RMakeParam();
extern LispValue StringToPlan();


/*-----------------------------------------------------------------------
 *
 * prs2StubToRawStub
 *
 * given a 'Prs2Stub' (which is a spaghetti of pointers)
 * transform it to a stream of bytes suitable for disk storage
 *
 *-----------------------------------------------------------------------
 */
Prs2RawStub
prs2StubToRawStub(relstub)
Prs2Stub relstub;
{
    Size size;
    Prs2RawStub res;
    Prs2OneStub oneStub;
    char **stubQual;
    int i,j;
    int len;
    char *s;

    /*
     * NOTE: this routine does a two pass "parsing" of 'relstub'.
     * The first one to find the size needed for the raw stub
     * representation and the second to actually store the information.
     * However as in order to complete the first step we must 
     * transform the stub qualifications to strings and then repeat
     * this process again during the second pass and as this takes
     * quite some time, we instead opt to store the strings
     * (the ascii reperesentations) of the stubs quals in an array
     * during the first pass so we do not have to recalculate them 
     * during the second!)
     */
    if (relstub->numOfStubs>0) {
	stubQual = (char **)palloc(sizeof(char *) * relstub->numOfStubs);
	if (stubQual == NULL) {
	    elog(WARN, "prs2StubToRawStub: Out of memory!\n");
	}
    }

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
	oneStub = relstub->stubRecords[i];
	size = size +
		sizeof(oneStub->ruleId) +
		sizeof(oneStub->stubId) +
		sizeof(oneStub->counter);
	/*
	 * now add the rule lock size
	 */
	size = size +
		prs2LockSize( prs2GetNumberOfLocks(oneStub->lock));
	/*
	 * finally add the size of the stub's qualification.
	 * NOTE: we store it a string preceded by an integer
	 * holding its length.
	 * Don't forget to store this string to an array
	 * so that we don't have to recalculate it later.
	 */
	s = lispOut(oneStub->qualification);
	stubQual[i] = s;
	size = size + (strlen(s) + sizeof(int));
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
	oneStub = relstub->stubRecords[i];
	/*
	 * copy the fixed size part of the record
	 */
	bcopy((char *)&(oneStub->ruleId), s, sizeof(oneStub->ruleId));
	s += sizeof(oneStub->ruleId);
	bcopy((char *)&(oneStub->stubId), s, sizeof(oneStub->stubId));
	s += sizeof(oneStub->stubId);
	bcopy((char *)&(oneStub->counter), s, sizeof(oneStub->counter));
	s += sizeof(oneStub->counter);
	/*
	 * now copy the rule lock
	 */
	size = prs2LockSize(prs2GetNumberOfLocks(oneStub->lock));
	bcopy((char *) oneStub->lock, s, size);
	s+=size;
	/*
	 * finally copy the qualification
	 * First the length of the string and then the string itself.
	 */
	len = strlen(stubQual[i]);
	bcopy((char *) &len, s, sizeof(int));
	s += sizeof(int);
	bcopy(stubQual[i], s, len);
	s += len;
    }

    /*
     * Keep POSTGRES memory clean!
     */
    for (i=0; i< relstub->numOfStubs; i++) {
	pfree(stubQual[i]);
    }
    if (relstub->numOfStubs > 0)
	pfree(stubQual);

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
    Size size;
    int i,j;
    int len;
    char *data;
    char *s;
    Prs2LocksData ruleLock;

    /*
     * create a new stub record
     */
    relstub = prs2MakeStub();
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
	size = relstub->numOfStubs * sizeof(Prs2OneStub);
	relstub->stubRecords = (Prs2OneStub *) palloc(size);
	if (relstub->stubRecords == NULL) {
	    elog(WARN, "prs2RawStubToStub: Out of memory");
	}
    } else {
	relstub->stubRecords = NULL;
    }
    
    for (i=0; i< relstub->numOfStubs; i++) {
	oneStub = prs2MakeOneStub();
	relstub->stubRecords[i] = oneStub;
	/*
	 * copy the fixed part of the 'Prs2OneStub' first.
	 */
        bcopy(data, (char *)&(oneStub->ruleId), sizeof(oneStub->ruleId));
        data += sizeof(oneStub->ruleId);
        bcopy(data, (char *)&(oneStub->stubId), sizeof(oneStub->stubId));
        data += sizeof(oneStub->stubId);
        bcopy(data, (char *)&(oneStub->counter), sizeof(oneStub->counter));
        data += sizeof(oneStub->counter);

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
	 * now recreate the qualification (which is stored
	 * as a string containing the ascii representation
	 * of a LispValue).
	 * First read the length of the string & then 
	 * create the LispValue out of the string.
	 */
	bcopy(data, (char*)&len, sizeof(int));
	data += sizeof(int);
	oneStub->qualification = StringToPlan(data);
	data += len;
    }

    /*
     * At last! we are done...
     */
    return(relstub);
}
