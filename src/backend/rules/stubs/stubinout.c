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
