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
    Prs2RawStub *pieces, res;
    int npieces;
    int maxsize;
    int i;

    maxsize = 1000000000;
    pieces = prs2StubToSmallRawStubs(relstub, maxsize, &npieces);
    if (npieces != 1) {
	elog(WARN, "Hey! this is a pretty hefty stub you've got there!");
    }

    res = pieces[0];
    pfree(pieces);

    return(res);
}

/*-----------------------------------------------------------------------
 *
 * prs2StubToSmallRawStubs
 *
 * Similar to 'prs2StubToRawStub' but if the resulting stub is too big,
 * then it breaks it in many pieces, each one of them being
 * less than 'maxSize' bytes long.
 * It returns an array of 'Prs2RawStub'. This array has '*nStubPiecesP'
 * entries.
 *
 * THE ECOLOGICAL MESSAGE OF THE WEEK:
 * Do not waste memory, recycle it! So, don't forget to pfree the stubs
 * and the array when you are done!
 *-----------------------------------------------------------------------
 */
Prs2RawStub *
prs2StubToSmallRawStubs(relstub, maxSize, nStubPiecesP)
Prs2Stub relstub;
int *nStubPiecesP;
{
    Size thisSize, headerSize, sumSizes, locksize;
    int nPieces;
    Prs2RawStub *res;
    Prs2OneStub oneStub;
    char **stubQual;
    int *stubSizes;
    int i,j;
    int len;
    char *s;
    int nstubs;

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
	stubSizes = (int *)palloc(sizeof(int) * relstub->numOfStubs);
	if (stubSizes == NULL) {
	    elog(WARN, "prs2StubToRawStub: Out of memory!\n");
	}
    } else {
	stubQual = NULL;
	stubSizes = NULL;
    }

    /*
     * calculate the number of bytes needed to hold
     * the raw representation of this stub record.
     * First we calculate the # of bytes needed for
     * each individual `Prs2OneStub' and we store it
     * in array 'stubSizes'
     */

    for (i=0; i< relstub->numOfStubs; i++) {
	/*
	 * calculate the size of each individual `Prs2OneStub'
	 * Start with the fixed size part of the struct.
	 */
	oneStub = relstub->stubRecords[i];
	thisSize = sizeof(oneStub->ruleId) +
		sizeof(oneStub->stubId) +
		sizeof(oneStub->counter);
	/*
	 * now add the rule lock size
	 */
	thisSize = thisSize +
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
	thisSize = thisSize + (strlen(s) + sizeof(int));
	/*
	 * store the size to 'stubSizes' and also
	 * add it to the total size
	 */
	stubSizes[i] = thisSize;

    }

    /*
     * calculate in how many pieces we have to break
     * the stub so that each piece is less than 'maxSize'
     * bytes long.
     *
     * NOTE: Just as like in the "varlena" structure, the "size" field
     * in the Prs2RawStubData is the sum of its own size + the (variable)
     * size of the data that is following.
     */
    nPieces = 0;
    headerSize = sizeof(res[0]->vl_len) + sizeof(relstub->numOfStubs);
    sumSizes = headerSize;
    for (i=0; i< relstub->numOfStubs; i++) {
	if (sumSizes + stubSizes[i] > maxSize) {
	    nPieces++;
	    sumSizes = stubSizes[i] + headerSize;
	} else {
	    sumSizes = sumSizes + stubSizes[i];
	}
    }
    nPieces++;

    /*
     * OK, allocate an array of that many raw stub pieces
     */
    res = (Prs2RawStub *) palloc(sizeof(Prs2RawStub) * nPieces);
    if (res==NULL) {
	elog(WARN,"prs2StubToRawStub: Out of memory");
    }

    /*
     * Now allocate enough space for each 
     * such piece
     */
    j=0;
    sumSizes = headerSize;
    for (i=0; i< relstub->numOfStubs; i++) {
	if (sumSizes + stubSizes[i] > maxSize) {
	    res[j] = (Prs2RawStub)palloc(sumSizes);
	    if (res[j]==NULL) {
		elog(WARN,"prs2StubToRawStub: Out of memory");
	    }
	    res[j]->vl_len = sumSizes;
	    j++;
	    sumSizes = stubSizes[i] + headerSize;
	} else {
	    sumSizes = sumSizes + stubSizes[i];
	}
    }
    res[j] = (Prs2RawStub)palloc(sumSizes);
    if (res[j]==NULL) {
	elog(WARN,"prs2StubToRawStub: Out of memory");
    }
    res[j]->vl_len = sumSizes;

    
    /*
     * now copy the stubs to the raw stub pieces.
     */
    j=0;
    s = &(res[j]->vl_dat[0]) + sizeof(relstub->numOfStubs);
    sumSizes = headerSize;
    nstubs = 0;


    for (i=0; i< relstub->numOfStubs; i++) {
	if (sumSizes + stubSizes[i] > maxSize) {
	    /*
	     * time to start a new piece...
	     * first update the 'numOfStubs' field of the previous
	     * piece
	     */
	    bcopy((char *) &nstubs, &(res[j]->vl_dat[0]), sizeof(nstubs));
	    /*
	     * now continue with the next stub
	     */
	    j++;
	    s = &(res[j]->vl_dat[0]) + sizeof(relstub->numOfStubs);
	    sumSizes = headerSize;
	    nstubs = 0;
	}
	nstubs ++;
	/*
	 * copy this prs2OneStub to the raw stub piece
	 */
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
	locksize = prs2LockSize(prs2GetNumberOfLocks(oneStub->lock));
	bcopy((char *) oneStub->lock, s, locksize);
	s+=locksize;
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
     * Don't forget to update the 'numOfStubs' field of the
     * last piece!
     */
    bcopy((char *) &nstubs, &(res[j]->vl_dat[0]), sizeof(nstubs));

    /*
     * Keep POSTGRES memory clean!
     */
    for (i=0; i< relstub->numOfStubs; i++) {
	pfree(stubQual[i]);
    }
    if (relstub->numOfStubs > 0) {
	pfree(stubQual);
	pfree(stubSizes);
    }

    /* 
     * OK we are done!
     */
    *nStubPiecesP = nPieces;
    return(res);
}

/*----------------------------------------------------------------------
 *
 * prs2RawStubToStub
 *
 * given a stream of bytes (like the one created by
 * 'prs2StubToRawStub') recreate the original stub record
 *
 *----------------------------------------------------------------------
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

/*----------------------------------------------------------------------
 * prs2RawStubUnion
 *
 * Make and return the union of the two given raw stubs.
 *----------------------------------------------------------------------
 */
Prs2RawStub
prs2RawStubUnion(stub1, stub2)
Prs2RawStub stub1;
Prs2RawStub stub2;
{
    Prs2RawStub res;
    int size, size1, size2;
    int nstubs, nstubs1, nstubs2;
    char *data1, *data2;
    char *s;

    /*
     * the data of each stub consists of the 4 bytes used
     * to store the number of the "small stub record" (i.e.
     * the equivalent to 'Prs2OneStubData' structure) contained
     * in this stub + the data for these "small stub records".
     * All this data is stored in the "vl_dat" field.
     *
     * Note that the number stored in "vl_len" is the size of all
     * this stuff + the size of the "vl_len" field itself.
     *
     * The size of data  of the union will be the sum of space needed
     * for the "small stub records" for stub 1, the space
     * needed for the "small stub records" of stub2 and
     * finally the 4 bytes to store the total number of
     * new "small stub records".
     */

    /*
     * read the 'number of stubs' information into 'nstubs1'
     * and make 'data1' point to the first "small stub record"
     * 'size1' is the size of the information for the "small
     * stub records" only.
     */
    s = &(stub1->vl_dat[0]);
    bcopy(s, (char *) &nstubs1, sizeof(nstubs1));
    data1 = s + sizeof(nstubs1);
    size1 = stub1->vl_len - sizeof(stub1->vl_len) - sizeof(nstubs1);

    /*
     * same thing for stub 2
     */
    s = &(stub2->vl_dat[0]);
    bcopy(s, (char *) &nstubs2, sizeof(nstubs2));
    data2 = s + sizeof(nstubs2);
    size2 = stub2->vl_len - sizeof(stub2->vl_len) - sizeof(nstubs2);

    /*
     * find the total number of stubs and calculate the
     * total size of the their union (including the number of
     * small stub records and the size of 'vl_len')
     */
    nstubs = nstubs1 + nstubs2;
    size = size1 + size2 + sizeof(nstubs) + sizeof(res->vl_len);


    /*
     * allocate space for the new stub and copy there the
     * information about the number of stubs
     */
    res = (Prs2RawStub) palloc(size);
    if (res == NULL) {
	elog(WARN,"prs2RawStubUnion: Out of memory");
    }
    res->vl_len = size;
    s = &(res->vl_dat[0]);
    bcopy((char *)&nstubs, s, sizeof(nstubs));

    /*
     * now concatenate the data for the small stub records of
     * stub1 and stub2 and copy them in the new union stub
     */
    s += sizeof(nstubs);
    bcopy(&(stub1->vl_dat[0]), s, size1);
    s += size1;
    bcopy(&(stub2->vl_dat[0]), s, size2);

    /*
     * OK, we are done...
     */
    return(res);
}


