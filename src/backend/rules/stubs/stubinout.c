/*======================================================================
 *
 * FILE:
 * stubinout.c
 *
 * IDENTIFICATION:
 * $Header$
 *
 * DESCRIPTION
 *
 * This file contains the routines that transform a stub
 * record to a string and vice versa.
 * If the stub record is in the 'Prs2Stub' format use the routines
 * 'prs2StubToString' and 'prs2StringToStub'. If they are in the
 * 'Prs2RawStub' format, usre 'stubin' and 'stubout'
 *
 * NOTE: XXX!!!
 * 'prs2StringToStub' (and 'stubin' are NOT IMPLEMENTED yet!
 *
 *======================================================================
 */

#include <stdio.h>
#include <strings.h>
#include "rules/prs2.h"
#include "rules/prs2stub.h"
#include "utils/log.h"

char *palloc();

/*-------------------
 * these routines are local to this file.
 */
static char *appendString();
static void skipToken();
static long readLongInt();
static char readChar();

#define ISBLANK(c) ((c)==' ' || (c)=='\t' || (c)=='\n')


/*--------------------------------------------------------------------
 *
 * stubout
 */
char *
stubout(rawStub)
Prs2RawStub rawStub;
{
    Prs2Stub relstub;
    char *res;

    relstub = prs2RawStubToStub(rawStub);
    res = prs2StubToString(relstub);
    prs2FreeStub(relstub, true);
    return(res);
}

/*--------------------------------------------------------------------
 *
 * stubin
 */
Prs2RawStub
stubin(s)
char *s;
{
    Prs2Stub relstub;
    Prs2RawStub rawStub;

    relstub = prs2StringToStub(s);
    rawStub = prs2StubToRawStub(relstub);
    prs2FreeStub(relstub, true);
    return(rawStub);
}
/*--------------------------------------------------------------------
 *
 * prs2StubToString
 * 
 * Given a stub record create a string containing a represeantation
 * suitable for printing
 *
 * NOTE: space is allocated for the string. When you don't need it
 * anymore, use 'pfree()'
 */
char *
prs2StubToString(relstub)
Prs2Stub relstub;
{
    
    char *res; 
    char s1[200];
    char *s2;
    int i,j, k;
    int maxLength;
    Prs2OneStub oneStub;
    Prs2SimpleQual oneQual;
    char *p;
    int size;
    int l;

    maxLength = 100;
    res = palloc(maxLength);
    if (res==NULL) {
	elog(WARN, "RuleStubToString: Out of memory!");
    }

    sprintf(res, "(numOfStubs: %d", relstub->numOfStubs);

    for (i=0; i<relstub->numOfStubs; i++) {
	/*
	 * print the information for this stub record
	 */
	oneStub = &(relstub->stubRecords[i]);
	sprintf(s1, " (ruleId: %ld stubId: %u count: %d nQuals: %d lock: ",
	    oneStub->ruleId,
	    oneStub->stubId,
	    oneStub->counter,
	    oneStub->numQuals);
	res = appendString(res, s1, &maxLength);
	/*
	 * now print the rule lock
	 * Enclose it in double quotes, so that it
	 * will be easy for `prs2StringToStub' to read it
	 */
	s2 = RuleLockToString(oneStub->lock);
	appendString(res, "`", &maxLength);
	res = appendString(res, s2, &maxLength);
	appendString(res, "'", &maxLength);
	pfree(s2);
	/*
	 * Now print the qualifications...
	 */
	for (j=0; j<oneStub->numQuals; j++) {
	    oneQual = &(oneStub->qualification[j]);
	    sprintf(s1,
		"(attrNo: %d opr: %ld type: %ld byval: %c len: %ld data:",
		oneQual->attrNo,
		oneQual->operator,
		oneQual->constType,
		(oneQual->constByVal ? 't' : 'f'),
		oneQual->constLength);
	    res = appendString(res, s1, &maxLength);

	    /*
	     * Now print the datum
	     */
	    if (oneQual->constByVal) {
		/*
		 * print all `sizeof(Datum)' bytes,
		 * because of different machines have
		 * diferrent alignements....
		 */
		p = (char *) &(oneQual->constData);
		size = sizeof(Datum);
	    } else {
		/* 
		 * `oneQual->constData' is a pointer to a structure.
		 * Calculate the "real" length of the data.
		 */
		p = (char *) DatumGetPointer(oneQual->constData);
		if (oneQual->constLength < 0 ) {
		    size = VARSIZE(DatumGetPointer(oneQual->constData));
		} else {
		    size = oneQual->constLength;
		}
	    }
	    sprintf(s1, " %d {", size);
	    res = appendString(res, s1, &maxLength);
	    for(l=0; l<size; l++) {
		sprintf(s1, " %d", (int) (p[l]));
		res = appendString(res, s1, &maxLength);
	    }
	    res = appendString(res, "}", &maxLength);

	    res = appendString(res, ")", &maxLength);
	}
	res = appendString(res, ")", &maxLength);
    }
    res = appendString(res, ")", &maxLength);

    return(res);
}

/*--------------------------------------------------------------------
 *
 * prs2StringToStub
 */
Prs2Stub
prs2StringToStub(s)
char *s;
{
    int i,j,k;
    int numOfStubs;
    Prs2Stub stubs;
    Prs2OneStub oneStub;
    ObjectId ruleId;
    Prs2StubId stubId;
    int count;
    int nQuals;
    Datum constDatum;
    Prs2SimpleQual quals;
    AttributeNumber attrNo;
    int indx;
    long l;
    char c;
    RuleLock lock;
    Size size;
    char *data;


    /*
     * Create an initially empty rule stub.
     */
    stubs = prs2MakeStub();

    /*
     * Now read the number of stubs;
     */
    indx = 0;
    skipToken("(", s, &indx);
    skipToken("numOfStubs:", s, &indx);
    l = readLongInt(s, &indx);
    numOfStubs = (int) l;

    for (i=0; i<numOfStubs; i++) {
	/*
	 * read one `Prs2OneStub'
	 * First read the ruleId, stubId, counter, numQuals
	 */
	skipToken("(", s, &indx);
	skipToken("ruleId:", s, &indx);
	l = readLongInt(s, &indx);
	ruleId = (ObjectId) l;
	skipToken("stubId:", s, &indx);
	l = readLongInt(s, &indx);
	stubId = (Prs2StubId) l;
	skipToken("count:", s, &indx);
	l = readLongInt(s, &indx);
	count = (int) l;
	skipToken("nQuals:", s, &indx);
	l = readLongInt(s, &indx);
	nQuals = (int) l;
	/*
	 * now read the lock
	 */
	skipToken("lock:", s, &indx);
	skipToken("`", s, &indx);
	lock = StringToRuleLock(&s[indx]);
	while (s[indx++] != '\'');
	/*
	 * now read the quals
	 */
	quals = prs2MakeSimpleQuals(nQuals);
	for (j=0; j<nQuals; j++) {
	    skipToken("(", s, &indx);
	    skipToken("attrNo:", s, &indx);
	    l = readLongInt(s, &indx);
	    quals[j].attrNo = (AttributeNumber) l;

	    skipToken("opr:", s, &indx);
	    l = readLongInt(s, &indx);
	    quals[j].operator = (ObjectId) l;

	    skipToken("type:", s, &indx);
	    l = readLongInt(s, &indx);
	    quals[j].constType = (ObjectId) l;

	    skipToken("byval:", s, &indx);
	    c = readChar(s, &indx);
	    quals[j].constByVal = (c=='t' ? true : false);

	    skipToken("len:", s, &indx);
	    l = readLongInt(s, &indx);
	    quals[j].constLength = (Size) l;

	    /*
	     * now read the const data
	     */
	    skipToken("data:", s, &indx);
	    l = readLongInt(s, &indx);
	    size = (Size) l;
	    skipToken("{", s, &indx);

	    if (quals[j].constByVal){
		data = (char *) (&constDatum);
	    } else {
		data = palloc(size);
		Assert(data!=NULL);
		constDatum = PointerGetDatum(data);
	    }
	    for (k=0; k<size; k++) {
		l = readLongInt(s, &indx);
		data[k] = (char) (l);
	    }
	    quals[j].constData = constDatum;
	    skipToken("}", s, &indx);
	    skipToken(")", s, &indx);
	}

	skipToken(")", s, &indx);
	oneStub = prs2MakeOneStub( ruleId, stubId, count, nQuals,
					lock, quals);
	prs2AddOneStub(stubs, oneStub);

    }

    skipToken(")", s, &indx);
    return(stubs);
}

/*====================================================================
 * UTILITY ROUTINES LOCAL TO THIS FILE
 *====================================================================
 */

/*--------------------------------------------------------------------
 *
 * appendString
 *
 * append string 's' to the string 'res'. *maxLengthP is the
 * memory size allocated for 'res'. If this is not enough to hold
 * both the old contents of 'res' and the appended string 's',
 * allocate some more memory. 
 * It returns the result of the string concatenation, and
 * if more space has been allocated, *maxLengthP is updated.
 */
static char*
appendString(res, s, maxLengthP)
char *res;
char *s;
int *maxLengthP;
{
    char *temp;

    if (strlen(s) + strlen(res) >= *maxLengthP -1) {
	while (strlen(s) + strlen(res) >= *maxLengthP -1) {
	    *maxLengthP += 100;
	}
	temp = palloc(*maxLengthP);
	if (temp==NULL) {
	    elog(WARN, "appendString: Out of memory!");
	}
	strcpy(temp, res);
	pfree(res);
	res = temp;
    }
    strcat(res, s);
    return(res);
}

/*--------------------------------------------------------------------
 *
 * skipToken
 *
 * read & skip the specified token 'token' from the string that
 * starts at 's[*indx]'. '*indx' is incremented accordingly
 * If there is a mismatch, signal a syntax error.
 */
static void
skipToken(token, s, indx)
char *token;
char *s;
int *indx;
{
    char *t;

    /*
     * skip blanks
     */
    while (ISBLANK(s[*indx])) {
	(*indx)++;
    }

    t = token;
    while (*t != '\0') {
	if (s[*indx] != *t) {
	    elog(WARN,"Syntax error while reading RuleStub, token=%s", token);
	}
	t++;
	(*indx)++;
    }
}

/*--------------------------------------------------------------------
 *
 * readLongInt
 *
 * read an (long) integer from the string that
 * starts at 's[*indx]'. '*indx' is incremented accordingly
 */
static long
readLongInt(s, indx)
char *s;
int *indx;
{
    long res;

    /*
     * skip blanks
     */
    while (ISBLANK(s[*indx])) {
	(*indx)++;
    }

    res = 0;
    while (s[*indx] <='9' && s[*indx] >= '0') {
	res = 10*res + s[*indx] - '0';
	(*indx)++;
    }

    return(res);
}

/*--------------------------------------------------------------------
 *
 * readChar
 *
 * read the first non blank character from the string that
 * starts at 's[*indx]'. '*indx' is incremented accordingly
 */
static char
readChar(s, indx)
char *s;
int *indx;
{
    char c;

    /*
     * skip blanks
     */
    while (ISBLANK(s[*indx])) {
	(*indx)++;
    }

    c = s[*indx];
    (*indx)++;
    return(c);
}
