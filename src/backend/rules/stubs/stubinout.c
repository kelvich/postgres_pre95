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
 *
 *======================================================================
 */

#include <stdio.h>
#include <strings.h>
#include "rules/prs2.h"
#include "rules/prs2stub.h"
#include "parser/parse.h"	/* for the AND, NOT, OR */
#include "utils/log.h"
#include "tmp/datum.h"
#include "nodes/primnodes.h"
#include "nodes/primnodes.a.h"

extern Const RMakeConst();
extern Param RMakeParam();
extern LispValue StringToPlan();

/*-------------------
 * these routines are local to this file.
 */
static char *appendString();
static void skipToken();
static long readLongInt();
static char readChar();
static Name readName();

#define ISBLANK(c) ((c)==' ' || (c)=='\t' || (c)=='\n')


/*--------------------------------------------------------------------
 *
 * stubout
 *
 * Given a "Prs2RawStub" transform it to a string readable by humans
 *
 *--------------------------------------------------------------------
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
 *
 * Given a string rerpresentin a rule stub record, recreate
 * the 'Prs2RawStub' struct.
 *
 *--------------------------------------------------------------------
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
 *--------------------------------------------------------------------
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
    char *p;
    Size size;
    int l;

    /*
     * initialize res to the null string.
     */
    maxLength = 0;
    res = NULL;

    /*
     * print the number of stubs...
     */
    sprintf(s1, "(numOfStubs: %d", relstub->numOfStubs);
    res = appendString(res, s1, &maxLength);

    for (i=0; i<relstub->numOfStubs; i++) {
	/*
	 * print the information for this stub record
	 */
	oneStub = relstub->stubRecords[i];
	sprintf(s1, " (ruleId: %ld stubId: %u count: %d lock: ",
	    oneStub->ruleId,
	    oneStub->stubId,
	    oneStub->counter);
	res = appendString(res, s1, &maxLength);
	/*
	 * now print the rule lock
	 * Enclose it in double quotes, so that it
	 * will be easy for `prs2StringToStub' to read it
	 */
	s2 = RuleLockToString(oneStub->lock);
	res = appendString(res, "`", &maxLength);
	res = appendString(res, s2, &maxLength);
	res = appendString(res, "' ", &maxLength);
	pfree(s2);
	/*
	 * Now print the qualification...
	 * NOTE: in order to be able to easily read this
	 * qual back (by prs2StringToStub) we
	 * enclose it in quotes.
	 */
	res = appendString(res, "qual: ", &maxLength);
	s2 = lispOut(oneStub->qualification);
	res = appendString(res, "`", &maxLength);
	res = appendString(res, s2, &maxLength);
	res = appendString(res, "'", &maxLength);
	pfree(s2);
	res = appendString(res, ")", &maxLength); /* end of one stub */
    }
    res = appendString(res, ")", &maxLength);

    return(res);
}

/*--------------------------------------------------------------------
 *
 * prs2StringToStub
 *
 * GIven a "string" representation of a stub, recreate the stub.
 *--------------------------------------------------------------------
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
	oneStub = prs2MakeOneStub();	/* allocate space */
	skipToken("(", s, &indx);
	skipToken("ruleId:", s, &indx);
	l = readLongInt(s, &indx);
	oneStub->ruleId = (ObjectId) l;
	skipToken("stubId:", s, &indx);
	l = readLongInt(s, &indx);
	oneStub->stubId = (Prs2StubId) l;
	skipToken("count:", s, &indx);
	l = readLongInt(s, &indx);
	oneStub->counter = (int) l;
	/*
	 * now read the lock
	 * (remember: it was enclosed in quotes).
	 */
	skipToken("lock:", s, &indx);
	skipToken("`", s, &indx);
	oneStub->lock = StringToRuleLock(&s[indx]);
	while (s[indx++] != '\'');
	/*
	 * now read the qualification
	 * (remember: it was enclosed in quotes).
	 */
	skipToken("qual:", s, &indx);
	skipToken("`", s, &indx);
	oneStub->qualification = StringToPlan(&s[indx]);
	while (s[indx++] != '\'');

	skipToken(")", s, &indx);	/* end of one stub */

	prs2AddOneStub(stubs, oneStub);

    }

    skipToken(")", s, &indx);
    return(stubs);
}

/*====================================================================
 * MORE UTILITY ROUTINES LOCAL TO THIS FILE
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
 *
 * NOTE: this routine works even if 'res' is NULL.
 */
static char*
appendString(res, s, maxLengthP)
char *res;
char *s;
int *maxLengthP;
{
    char *temp;

    /*
     * if res is NULL, allocate some initial space
     */

    if (res == NULL) {
	*maxLengthP = 100;
	res = (char *) palloc(*maxLengthP);
	if (res==NULL) {
	    elog(WARN, "appendString: Out of memory!");
	}
	res[0] = '\0';
    }

    if (strlen(s) + strlen(res) >= *maxLengthP -1) {
	while (strlen(s) + strlen(res) >= *maxLengthP -1) {
	    *maxLengthP += 100;
	}
	temp = (char *) palloc(*maxLengthP);
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
    int sign;

    /*
     * skip blanks
     */
    while (ISBLANK(s[*indx])) {
	(*indx)++;
    }

    /*
     * Is this a negative number ?
     */
    sign = 1;
    if (s[*indx] == '-') {
	sign = -1;
	(*indx)++;
    }

    /*
     * Now read the number
     */
    res = 0;
    while (s[*indx] <='9' && s[*indx] >= '0') {
	res = 10*res + s[*indx] - '0';
	(*indx)++;
    }

    res = sign * res;

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

/*-----------------------------------------------------------------------
 * readName
 *
 * Read a name (i.e. a string at most 16 chars long) enclosed in quotes.
 * Returns a palloced Name.
 * 
 * If the name has embedded quotes they must be escaped with a
 * backslash...
 */
static
Name
readName(s, indx)
char *s;
int *indx;
{
    Name res;
    int i;
    char c, c1;

    res = (Name) palloc(sizeof(NameData));
    if (res == NULL) {
	elog(WARN, "readString: Out of memory");
    }

    skipToken("\"", s, indx);

    i = 0;
    c1 = '\0';
    c = readChar(s, indx);
    while (c!= '"' || (c=='"' && c1 == '\\')) {
	if (i>15) {
	    elog(WARN, "readString: Name too large!");
	}
	res->data[i] = c;
	i++;
	c1 = c;
	c = readChar(s, indx);
    }

    if(i<=15) {
	res->data[i] = '\0';
    }

    return(res);
}



