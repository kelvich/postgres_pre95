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
#include "prs2.h"
#include "prs2stub.h"
#include "log.h"

char *palloc();

/*-------------------
 * these routines are local to this file.
 */
static char *appendString();

#define ISBLANK(c) ((c)==' ' || (c)=='\t' || (c)=='\n')


/*--------------------------------------------------------------------
 *
 * stubout
 */
char *
stubout(rawStub)
Prs2RawStub rawStub;
{
    Prs2Stub stub;
    char *res;

    stub = prs2RawStubToStub(rawStub);
    res = prs2StubToString(stub);
    prs2FreeStub(stub);
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
    Prs2Stub stub;
    Prs2RawStub rawStub;

    stub = prs2StringToStub(s);
    rawStub = prs2StubToRawStub(stub);
    prs2FreeStub(stub);
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
prs2StubToString(stub)
Prs2Stub stub;
{
    
    char *res; 
    char s1[200];
    char *s2;
    int i,j, k;
    int maxLength;
    Prs2OneStub oneStub;
    Prs2SimpleQual oneQual;

    maxLength = 100;
    res = palloc(maxLength);
    if (res==NULL) {
	elog(WARN, "RuleStubToString: Out of memory!");
    }

    sprintf(res, "(numOfStubs: %d", stub->numOfStubs);

    for (i=0; i<stub->numOfStubs; i++) {
	/*
	 * print the information for this stub record
	 */
	oneStub = &(stub->stubRecords[i]);
	sprintf(s1, " (ruleId: %ld stubId: %u count: %d nQuals: %d",
	    oneStub->ruleId,
	    oneStub->stubId,
	    oneStub->counter,
	    oneStub->numQuals);
	res = appendString(res, s1, &maxLength);
	/*
	 * now print the rule lock
	 */
	s2 = RuleLockToString(oneStub->lock);
	res = appendString(res, s2, &maxLength);
	pfree(s2);
	/*
	 * Now print the qualifications...
	 */
	for (j=0; j<oneStub->numQuals; j++) {
	    oneQual = &(oneStub->qualification[j]);
	    sprintf(s1,
		"(attrNo: %d operator: %ld constType: %ld constLen: %ld data:",
		oneQual->attrNo,
		oneQual->operator,
		oneQual->constType,
		oneQual->constLength);
	    res = appendString(res, s1, &maxLength);
	    for (k=0; k<oneQual->constLength; k++) {
		sprintf("%d ", oneQual->constData[k]);
		res = appendString(res, s1, &maxLength);
	    }
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
    elog(WARN, "StringToRuleStub: NOT CURRENTLY IMPLEMENTED");
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
