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

char *palloc();
Const RMakeConst();
Param RMakeParam();

/*-------------------
 * these routines are local to this file.
 */
static char *prs2StubQualToString();
static Prs2StubQual prs2StringToStubQual();
static char *prs2OperandToString();
static Node prs2StringToOperand();
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
	 */
	res = appendString(res, "qual: ", &maxLength);
	s2 = prs2StubQualToString(oneStub->qualification);
	res = appendString(res, s2, &maxLength);
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
	 */
	skipToken("lock:", s, &indx);
	skipToken("`", s, &indx);
	oneStub->lock = StringToRuleLock(&s[indx]);
	while (s[indx++] != '\'');
	/*
	 * now read the qualification
	 */
	skipToken("qual:", s, &indx);
	oneStub->qualification = prs2StringToStubQual(s, &indx);

	skipToken(")", s, &indx);	/* end of one stub */

	prs2AddOneStub(stubs, oneStub);

    }

    skipToken(")", s, &indx);
    return(stubs);
}

/*====================================================================
 * UTILITY ROUTINES LOCAL TO THIS FILE
 *====================================================================
 */

/*----------------------------------------------------------------------
 *
 * prs2StubQualToString
 *
 * Given a 'Prs2StubQual', generate a string out of it...
 *
 *----------------------------------------------------------------------
 */
static
char *
prs2StubQualToString(qual)
Prs2StubQual qual;
{
    char *res;
    char s1[100];
    char *s2;
    int maxLength;
    Size size;
    int l;
    char *p;
    Prs2SimpleStubQualData *simple;
    Prs2ComplexStubQualData *complex;

    /*
     * initialize 'res' to a null string.
     */
    res = NULL;
    maxLength = 0;

    if (qual->qualType == PRS2_NULL_STUBQUAL) {
	res = appendString(res, "NIL", &maxLength);
    } else if (qual->qualType == PRS2_SIMPLE_STUBQUAL) {
	simple = &(qual->qual.simple);
	sprintf(s1, "[oper: %ld ", simple->operator);
	res = appendString(res, s1, &maxLength);
	/*
	 * Now print the two operands.
	 */
	s2 = prs2OperandToString(simple->left);
	res = appendString(res, s2, &maxLength);
	pfree(s2);
	s2 = prs2OperandToString(simple->right);
	res = appendString(res, s2, &maxLength);
	pfree(s2);
	res = appendString(res, "]", &maxLength);	/* end simple qual */
    } else if (qual->qualType == PRS2_COMPLEX_STUBQUAL) {
	complex = &(qual->qual.complex);
	switch (complex->boolOper) {
	    case AND:
		sprintf(s1, "(AND %d ", complex->nOperands);
		break;
	    case OR:
		sprintf(s1, "(OR %d ", complex->nOperands);
		break;
	    case NOT:
		sprintf(s1, "(NOT %d ", complex->nOperands);
		break;
	    default:
		elog(WARN, "prs2StubQualToString: Illegal boolOper");
	}
	res = appendString(res, s1, &maxLength);
	/*
	 * Now print the children
	 */
	for (l=0; l<complex->nOperands; l++) {
	    s2 = prs2StubQualToString(complex->operands[l]);
	    res = appendString(res, s2, &maxLength);
	    pfree(s2);
	}
	/*
	 * print the closing parenthesis for the complex qual
	 */
	res = appendString(res, ")", &maxLength);
    } else {
	elog(WARN, "prs2StubQualToString: bad qualType!\n");
    }

    /*
     * we are done!
     */
    return(res);
}

/*----------------------------------------------------------------------
 *
 * prs2StringToStubQual
 *
 * Given the string representation of a 'Prs2StubQual', recreate the
 * struct.
 *
 * The string represantation starts at 's[*indx]'.
 * When this routine returns, '*indx' points to the next character
 * to be read...
 *
 *----------------------------------------------------------------------
 */
static
Prs2StubQual
prs2StringToStubQual(s, indx)
char *s;
int *indx;
{
    Prs2StubQual qual;
    Size size;
    char *data;
    Datum constDatum;
    char c;
    int boolOper;
    int j,k;
    long l;

    /*
     * create the "Prs2StubQualData" struct.
     */
    qual = prs2MakeStubQual();

    /*
     * Initialize index and read the first character
     */
    c = readChar(s, indx);
    if (c == 'N') {
	/*
	 * then this ust be the word "NIL", i.e. a null qual
	 */
	skipToken("IL", s, indx);
	qual->qualType = PRS2_NULL_STUBQUAL;
    } else if (c == '[') {
	/*
	 * then this must be a "simple" qual
	 * Read the 'oper:' and the two operands.
	 * and 'len'.
	 */
	qual->qualType = PRS2_SIMPLE_STUBQUAL;
	skipToken("oper:", s, indx);
	l = readLongInt(s, indx);
	qual->qual.simple.operator = (ObjectId) l;
	qual->qual.simple.left = prs2StringToOperand(s, indx);
	qual->qual.simple.right = prs2StringToOperand(s, indx);
	skipToken("]", s, indx);	/* end of simple qual */
    } else if (c == '(') {
	/*
	 * it is a complex qual.
	 * Read the boolean operator.
	 */
	qual ->qualType = PRS2_COMPLEX_STUBQUAL;
	c = readChar(s, indx);
	if (c=='N') {
	    qual->qual.complex.boolOper = NOT;
	    skipToken("OT", s, indx);
	} else if (c=='O') {
	    qual->qual.complex.boolOper = OR;
	    skipToken("R", s, indx);
	} else if (c=='A') {
	    qual->qual.complex.boolOper = AND;
	    skipToken("ND", s, indx);
	} else {
	    elog(WARN,
		"prs2StringToStubQual: bad char '%c' in complex qual", c);
	}
	/*
	 * read the number of operands, and allocate enough
	 * space for the array of pointers to them.
	 */
	l = readLongInt(s, indx);
	qual->qual.complex.nOperands = l;
	size = qual->qual.complex.nOperands * sizeof(Prs2StubQual);
	qual->qual.complex.operands = (Prs2StubQual *)palloc(size);
	if (qual->qual.complex.operands == NULL) {
	    elog(WARN, "prs2StringToStubQual: Out of memory");
	}
	/*
	 * Now read one by one the operands.
	 */
	for (j=0; j<qual->qual.complex.nOperands; j++) {
	    qual->qual.complex.operands[j] = prs2StringToStubQual(s, indx);
	}
	/*
	 * skip the end of complex qual
	 */
	skipToken(")", s, indx);
    } else {
	/*
	 * Oooops!
	 */
	elog(WARN,"Syntax error while reading RuleStub qual (char='%c')", c);
    }

    /*
     * Done!
     */
    return(qual);
}

/*-------------------------------------------------------------------
 * prs2OperandToString
 *
 * Given an operand which must be a Param or Const node,
 * create a string readable by human beings with its representation.
 *-------------------------------------------------------------------
 */
static
char *
prs2OperandToString(operand)
Node operand;
{
    char *s;
    char s1[1000];
    int maxLength;
    char *res;
    int l;
    char *p;
    Size size;
    Param param;
    Const constant;

    maxLength = 0;
    res = NULL;

    res = appendString(res, "(", &maxLength);

    if (IsA(operand,Param)) {
	sprintf(s1, "param");
	param = (Param) operand;
	res = appendString(res, s1, &maxLength);
	if (param->paramkind != PARAM_NEW && param->paramkind != PARAM_OLD) {
	    elog(WARN,"prs2OperandToString: Param is not NEW or OLD");
	}
	sprintf(s1, " paramkind: %d", param->paramkind);
	res = appendString(res, s1, &maxLength);
	sprintf(s1, " paramid: %d", param->paramid);
	res = appendString(res, s1, &maxLength);
	sprintf(s1, " paramname: \"%s\"", param->paramname);
	res = appendString(res, s1, &maxLength);
	sprintf(s1, " paramtype: %ld", param->paramtype);
	res = appendString(res, s1, &maxLength);
    } else if (IsA(operand,Const)) {
	constant = (Const) operand;
	sprintf(s1, "const");
	res = appendString(res, s1, &maxLength);
	sprintf(s1, " consttype: %ld", constant->consttype);
	res = appendString(res, s1, &maxLength);
	sprintf(s1, " constlen: %hd", constant->constlen);
	res = appendString(res, s1, &maxLength);
	sprintf(s1, " constbyval: %d", (constant->constbyval ? 1 : 0 ));
	res = appendString(res, s1, &maxLength);
	sprintf(s1, " constisnull: %d", (constant->constisnull ? 1 : 0 ));
	res = appendString(res, s1, &maxLength);
	sprintf(s1, " constvalue: ");
	res = appendString(res, s1, &maxLength);
	if (constant->constisnull) {
	    sprintf(s1, "NIL ");
	    res = appendString(res, s1, &maxLength);
	} else {
	    /*
	     * Now print the datum
	     * First find its 'real' size.
	     */
	    size = datumGetSize (constant->constvalue,
				    constant->consttype,
				    constant->constbyval,
				    constant->constlen);
	    if (constant->constbyval) {
		p = (char *) &(constant->constvalue);
	    } else {
		/*
		 * `constant->constData' is a pointer to a structure.
		 */
		p = (char *) DatumGetPointer(constant->constvalue);
	    }
	    sprintf(s1, " %d {", size);
	    res = appendString(res, s1, &maxLength);
	    for(l=0; l<size; l++) {
		sprintf(s1, " %d", (int) (p[l]));
		res = appendString(res, s1, &maxLength);
	    }
	    res = appendString(res, "}", &maxLength);
	}
    } else {
	elog(WARN, "prs2OperandToString: operand is not Const or Param");
    }

    res = appendString(res, ")", &maxLength);
}


/*-------------------------------------------------------------------------
 * prs2StringToOperand
 *
 * Given a string containig a "human readable" representation of
 * an operand used in a stub qual, recreate the operand.
 * 
 *-------------------------------------------------------------------------
 */
static
Node
prs2StringToOperand(s, indx)
char *s;
int *indx;
{
    char c;
    long l;
    int size;
    int k;
    Param param;
    Const constant;
    char *data;

    skipToken("(", s, indx);

    c = readChar(s, indx);

    if (c=='p') {
	/*
	 * It's a param node
	 */
	skipToken("aram", s, indx);
	param = RMakeParam();
	skipToken("paramkind:", s, indx);
	l = readLongInt(s, indx);
	param->paramkind = (int)l;
	if (param->paramkind != PARAM_NEW && param->paramkind != PARAM_OLD) {
	    elog(WARN,"prs2StringToOperand: Param is not NEW or OLD");
	}
	skipToken("paramid:", s, indx);
	l = readLongInt(s, indx);
	param->paramid = (AttributeNumber)l;
	skipToken("paramname:", s, indx);
	param->paramname = readName(s, indx);
	skipToken("paramtype:", s, indx);
	l = readLongInt(s, indx);
	param->paramtype = l;
	skipToken(")", s, indx);
	return((Node) param);
    } else if (c=='c') {
	/*
	 * It's a Const node;
	 */
	skipToken("onst", s, indx);
	constant = RMakeConst();
	skipToken("consttype:", s, indx);
	l = readLongInt(s, indx);
	constant->consttype = (ObjectId) l;

	skipToken("constlen:", s, indx);
	l = readLongInt(s, indx);
	constant->constlen = (ObjectId) l;

	skipToken("constbyval:", s, indx);
	l = readLongInt(s, indx);
	constant->constbyval =  (l ? true : false );

	skipToken("constisnull:", s, indx);
	l = readLongInt(s, indx);
	constant->constisnull =  (l ? true : false );

	skipToken("constvalue:", s, indx);
	if (constant->constisnull) {
	    skipToken("NIL", s, indx);
	} else {
	    l = readLongInt(s, indx);
	    size = (int) l;
	    skipToken("{", s, indx);
	    if (constant->constbyval){
		data = (char *) &(constant->constvalue);
	    } else {
		data = palloc(size);
		Assert(data!=NULL);
		constant->constvalue = PointerGetDatum(data);
	    }
	    for (k=0; k<size; k++) {
		l = readLongInt(s, indx);
		data[k] = (char) (l);
	    }
	    skipToken("}", s, indx);        /* end of data */
	}
	skipToken(")", s, indx);        /* end of const */
	return((Node) constant);
    } else {
	elog(WARN, "prs2StringToOperand: param or const expected");
    }
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
	res = palloc(*maxLengthP);
	if (res==NULL) {
	    elog(WARN, "appendString: Out of memory!");
	}
	res[0] = '\0';
    }

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



