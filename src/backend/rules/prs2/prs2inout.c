/*======================================================================
 *
 * FILE:
 * prs2inout.c
 *
 * IDENTIFICATION:
 * $Header$
 *
 * These are the in/out routines for rule locks.
 *
 *======================================================================
 */

#include <stdio.h>
#include <strings.h>
#include "rules/prs2.h"
#include "utils/log.h"
#include "utils/palloc.h"

/*-------------------------------------------------------------
 * these routines are local to this file.
 *-------------------------------------------------------------
 */
static char *appendString();
static void skipToken();
static long readLongInt();
static char readChar();

#define ISBLANK(c) ((c)==' ' || (c)=='\t' || (c)=='\n')


/*--------------------------------------------------------------------
 *
 * RuleLockToString
 *
 * Given a rule lock create a string containing a represantation
 * of a rule lock suitable for reading by human beings
 *
 * NOTE: space is allocated for the string. When you don't need it
 * anymore, use 'pfree()'
 */
char *
RuleLockToString(locks)
RuleLock locks;
{
    char *res, *temp;
    char s1[200];
    int i;
    int maxLength;
    Prs2OneLock oneLock;


    /*
     * Initialize `maxLength' and `res'
     */
    maxLength = 100;
    res = (char *) palloc(maxLength);
    if (res==NULL) {
	elog(WARN, "RuleLockToString: Out of memeory!");
    }
    res[0] = '\0';

    /*
     * empty locks are a special case...
     */
    if (locks == NULL) {
	/*
	 * a NULL lock is assumed to be an empty lock
	 */
	res = appendString(res, "(numOfLocks: 0)", &maxLength);
	return(res);
    }

    /*
     * Deal with a non empty lock...
     */
    sprintf(s1, "(numOfLocks: %d", locks->numberOfLocks);
    res = appendString(res, s1, &maxLength);

    for (i=0; i<prs2GetNumberOfLocks(locks); i++) {
	/*
	 * NOTE: oid are long ints, plan numbers are uint16, i.e. shorts
	 * ditto for attribute numbers,  and lock types are chars.
	 */
	oneLock = prs2GetOneLockFromLocks(locks, i);
	sprintf(s1, " (ruleId: %ld lockType: %c attrNo: %d planNo: %d)",
	    prs2OneLockGetRuleId(oneLock),
	    prs2OneLockGetLockType(oneLock),
	    prs2OneLockGetAttributeNumber(oneLock),
	    prs2OneLockGetPlanNumber(oneLock));
	res = appendString(res, s1, &maxLength);
    }

    res = appendString(res, ")", &maxLength);

    return(res);
}

/*--------------------------------------------------------------------
 *
 * StringToRuleLock
 *
 * this is the opposite of 'RuleLockToString'.
 * Given a string representing a rule lock, recreate the original
 * rulelock.
 *
 * NOTE: space is allocated for the new lock. Pfree it when 
 * you don't need it any more...
 *
 */
RuleLock
StringToRuleLock(s)
char *s;
{
    int i, numOfLocks;
    ObjectId ruleId;
    Prs2LockType lockType;
    AttributeNumber attrNo;
    Prs2PlanNumber planNo;
    int index;
    long l;
    char c;
    RuleLock lock;


    /*
     * Create an initially empty rule lock.
     */
    lock =  prs2MakeLocks();

    /*
     * Now read the number of locks
     */
    index = 0;
    skipToken("(", s, &index);
    skipToken("numOfLocks:", s, &index);
    l = readLongInt(s, &index);
    numOfLocks = (int) l;

    for (i=0; i<numOfLocks; i++) {
	/*
	 * read each lock's data
	 */
	skipToken("(", s, &index);
	skipToken("ruleId:", s, &index);
	l = readLongInt(s, &index);
	ruleId = (ObjectId) l;
	skipToken("lockType:", s, &index);
	c = readChar(s, &index);
	lockType = (Prs2LockType) c;
	skipToken("attrNo:", s, &index);
	l = readLongInt(s, &index);
	attrNo = (AttributeNumber) l;
	skipToken("planNo:", s, &index);
	l = readLongInt(s, &index);
	planNo = (Prs2PlanNumber) l;
	skipToken(")", s, &index);
	/*
	 * now append them into the rule lock structure..
	 */
	lock = prs2AddLock(lock, ruleId, lockType, attrNo, planNo);
    }

    skipToken(")", s, &index);
    return(lock);
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
 * starts at 's[*index]'. '*index' is incremented accordingly
 * If there is a mismatch, signal a syntax error.
 */
static void
skipToken(token, s, index)
char *token;
char *s;
int *index;
{
    char *t;

    /*
     * skip blanks
     */
    while (ISBLANK(s[*index])) {
	(*index)++;
    }

    t = token;
    while (*t != '\0') {
	if (s[*index] != *t) {
	    elog(WARN,"Syntax error while reading RuleLock, token=%s", token);
	}
	t++;
	(*index)++;
    }
}

/*--------------------------------------------------------------------
 *
 * readLongInt
 *
 * read an (long) integer from the string that
 * starts at 's[*index]'. '*index' is incremented accordingly
 */
static long
readLongInt(s, index)
char *s;
int *index;
{
    long res;
    int sign;

    /*
     * skip blanks
     */
    while (ISBLANK(s[*index])) {
	(*index)++;
    }

    /*
     * Is this a negative number ?
     */
    sign = 1;
    if (s[*index] == '-') {
	sign = -1;
	(*index)++;
    }

    /*
     * Now read the number
     */
    res = 0;
    while (s[*index] <='9' && s[*index] >= '0') {
	res = 10*res + s[*index] - '0';
	(*index)++;
    }

    res = res * sign;
    return(res);
}

/*--------------------------------------------------------------------
 *
 * readChar
 *
 * read the first non blank character from the string that
 * starts at 's[*index]'. '*index' is incremented accordingly
 */
static char
readChar(s, index)
char *s;
int *index;
{
    char c;

    /*
     * skip blanks
     */
    while (ISBLANK(s[*index])) {
	(*index)++;
    }

    c = s[*index];
    (*index)++;
    return(c);
}
