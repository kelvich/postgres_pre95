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

char *palloc();
Const RMakeConst();
Param RMakeParam();

/*--------- static routines local to this file ---------------*/
static int prs2StubQualToRawStubQual();
static Prs2StubQual prs2RawStubQualToStubQual();
static int prs2OperandToRawOperand();
static Node prs2RawOperandToOperand();

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
	 * NOTE: the first argument to 'prs2StubQualToRawStubQual'
	 * is NULL, so this routine will only return the size.
	 */
	size = size + prs2StubQualToRawStubQual(NULL, oneStub->qualification);
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
	 */
	size = prs2StubQualToRawStubQual(s, oneStub->qualification);
	s += size;
    }

    /* 
     * OK we are done!
     */
    return(res);
}

/*-------------------------------------------------------------------------
 *
 * prs2StubQualToRawStubQual
 *
 * Given a stub qualification (a 'Prs2StubQual') transform it
 * to a "flat" stream of bytes and return its size..
 *
 * If the pointer "s" is NULL, then this routine does no copying and
 * returns the number of bytes needed to hold the raw (flat)
 * representation of the qual.
 *
 * If "s" is non-NULL, then we also copy the flat representation there.
 *
 * 'Prs2StubQual' is a tree and we do a depth-first traversal...
 *-------------------------------------------------------------------------
 */
static
int 
prs2StubQualToRawStubQual(s, qual)
char *s;
Prs2StubQual qual;
{
    int i;
    Size size;
    int tempsize;
    Prs2SimpleStubQualData *simple;
    Prs2ComplexStubQualData *complex;

    /*
     * first the fixed part of "Prs2StubQual"
     */
    size = sizeof(qual->qualType);
    if (s!=NULL) {
	bcopy((char*)&(qual->qualType), s, sizeof(qual->qualType));
	s += sizeof(qual->qualType);
    }

    /*
     * Now take care of the union. This contains either
     * a "Prs2SimpleQualData" (a leaf node) or a "Prs2ComplexQualData"
     * (non-leaf node)
     */
    if (qual->qualType == PRS2_COMPLEX_STUBQUAL) {
	/*
	 * We have a non leaf node.
	 * First take care of the fixed length part.
	 */
	complex = &(qual->qual.complex);
	size += sizeof(complex->boolOper);
	size += sizeof(complex->nOperands);
	if (s!=NULL) {
            bcopy((char *)&(complex->boolOper), s, sizeof(complex->boolOper));
            s += sizeof(complex->boolOper);
            bcopy((char *)&(complex->nOperands), s, sizeof(complex->nOperands));
            s += sizeof(complex->nOperands);
	}
	/*
	 * Now recursively copy its children.
	 */
	for (i=0; i<complex->nOperands; i++) {
	    tempsize = prs2StubQualToRawStubQual(s, complex->operands[i]);
	    size += tempsize;
	    if (s!=NULL) {
		s += tempsize;
	    }
	}
    } else if (qual->qualType == PRS2_SIMPLE_STUBQUAL) {
	/*
	 * this is a leaf node, so the union "qual" contains a
	 * "Prs2SimpleQualData" struct. Find its size & copy it.
	 * First start with the operator oid.
	 */
	simple = &(qual->qual.simple);
	size = size +
		sizeof(simple->operator);
	if (s!=NULL) {
            bcopy((char *)&(simple->operator), s, sizeof(simple->operator));
            s += sizeof(simple->operator);
	}
	/*
	 * now the two operands.
	 */
	tempsize = prs2OperandToRawOperand(s, simple->left);
	size += tempsize;
	if (s!= NULL) {
	    s += tempsize;
	}
	tempsize = prs2OperandToRawOperand(s, simple->right);
	size += tempsize;
	if (s!= NULL) {
	    s += tempsize;
	}
    } else if (qual->qualType == PRS2_NULL_STUBQUAL) {
	/*
	 * Do nothing!
	 */
    } else {
	/*
	 * Oooops! a bad-bad-bad thing to happen...
	 */
	elog(WARN, "Prs2StubQualToRawStubQual: Bad qual type!");
    }


    /*
     * we are done, return the size.
     */
    return(size);
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
	 * now recreate the qualification
	 * NOTE: we passe a pointer to 'data', so that 'data'
	 * will advance accordingly as bytes are read.
	 */
	oneStub->qualification = prs2RawStubQualToStubQual(&data);
    }

    /*
     * At last! we are done...
     */
    return(relstub);
}

/*-------------------------------------------------------------------------
 *
 * prs2RawStubQualToStubQual
 *
 * This is the inverse of "prs2StubQualToRawStubQual". Given a stream
 * of bytes recreate the stub's qualification.
 *
 * NOTE: XXX XXX XXX !!! !!! !!! SOS SOS SOS !!! !!! !!! XXX XXX XXX
 *
 * The argument 's' is a POINTER to a char POINTER !
 * The value of the char pointer (i.e. '*s') because the pointer is
 * advanced as we read the bytes. Make SURE that you make a copy of the
 * pointer if you want to use it after the call to this routine !!!!!
 *
 * NOTE 2 : we traverse the qualification tree in depth first order.
 *
 *-------------------------------------------------------------------------
 */
static
Prs2StubQual
prs2RawStubQualToStubQual(s)
char **s;
{

    int i;
    Size size;
    char *ptr;
    Prs2StubQual qual;
    Prs2SimpleStubQualData *simple;
    Prs2ComplexStubQualData *complex;

    /*
     * Create a "Prs2StubQual" struct
     */
    qual = prs2MakeStubQual();

    /*
     * Now read the fixed part of "Prs2StubQual"
     */
    bcopy(*s, (char*)&(qual->qualType), sizeof(qual->qualType));
    *s += sizeof(qual->qualType);

    /*
     * Now take care of the union. This contains either
     * a "Prs2SimpleQualData" (a leaf node) or a "Prs2ComplexQualData"
     * (non-leaf node)
     */
    if (qual->qualType == PRS2_COMPLEX_STUBQUAL) {
	/*
	 * We have a non leaf node.
	 * First take care of the fixed length part.
	 */
	complex = &(qual->qual.complex);
	bcopy(*s, (char *)&(complex->boolOper), sizeof(complex->boolOper));
	*s += sizeof(complex->boolOper);
	bcopy(*s, (char *)&(complex->nOperands), sizeof(complex->nOperands));
	*s += sizeof(complex->nOperands);
	/*
	 * Now recursively recreate the children.
	 * First allocate the array of pointers to "Pr2StubQualData"
	 * structures (i.e. an array of "Prs2StubQual").
	 */
	if (complex->nOperands <= 0) {	/* better be safe than sorry... */
	    elog(WARN,"prs2RawStubQualToStubQual: nOperands<=0!!!");
	}
	size = complex->nOperands * sizeof(Prs2StubQual);
	complex->operands = (Prs2StubQual *)palloc(size);
	if (complex->operands == NULL) {
	    elog(WARN, "Prs2RawStubQualToStubQual: Out of memory!");
	}
	for (i=0; i<complex->nOperands; i++) {
	    complex->operands[i] = prs2RawStubQualToStubQual(s);
	}
    } else if (qual->qualType == PRS2_SIMPLE_STUBQUAL) {
	/*
	 * this is a leaf node, so the union "qual" contains a
	 * "Prs2SimpleQualData" struct. Find its size & copy it.
	 */
	simple = &(qual->qual.simple);
	bcopy(*s, (char *)&(simple->operator), sizeof(simple->operator));
	*s += sizeof(simple->operator);
	simple->left = prs2RawOperandToOperand(s);
	simple->right = prs2RawOperandToOperand(s);
    } else if (qual->qualType == PRS2_NULL_STUBQUAL) {
	/*
	 * Do nothing
	 */
    } else {
	/*
	 * Oooops! a bad-bad-bad thing to happen...
	 */
	elog(WARN, "Prs2RawStubQualToStubQual: Bad qual type!");
    }

    /*
     * we are done, return the "Prs2StubQual"
     */
    return(qual);
}

/*-------------------------------------------------------------------
 * prs2OperandToRawOperand
 *
 * Given an operand which must be a Param or Const node,
 * create their "flat" representation. If pointer 's' is not NULL,
 * copy it there, otherwise just return the size needed for 's'.
 *-------------------------------------------------------------------
 */
static
int
prs2OperandToRawOperand(s, operand)
char *s;
Node operand;
{

    int size;
    int tempsize;
    Param param;
    Const constant;

    size = 0;

    if (IsA(operand,Param)) {
	size ++;
	if (s!= NULL) {
	    *s = 'p';		/* to indicate a Param node */
	    s++;
	}
	param = (Param) operand;
	if (param->paramkind != PARAM_NEW && param->paramkind != PARAM_OLD) {
	    elog(WARN,"prs2OperandToRawOperand: param is not NEW or OLD");
	}
	size += sizeof(param->paramkind) +
		    sizeof(param->paramid) +
		    sizeof(*(param->paramname)) +
		    sizeof(param->paramtype);
	if (s!= NULL) {
	    bcopy((char *)&(param->paramkind), s, sizeof(param->paramkind));
	    s += sizeof(param->paramkind);
	    bcopy((char *)&(param->paramid), s, sizeof(param->paramid));
	    s += sizeof(param->paramid);
	    bcopy((char *)(param->paramname), s, sizeof(*(param->paramname)));
	    s += sizeof(*(param->paramname));
	    bcopy((char *)&(param->paramtype), s, sizeof(param->paramtype));
	    s += sizeof(param->paramtype);
	}
    } else if (IsA(operand,Const)) {
	size ++;
	if (s!= NULL) {
	    *s = 'c';		/* to indicate a Const node */
	    s++;
	}
	constant = (Const) operand;
	/*
	 * First copy the fixed part of the Const node
	 */
	size += sizeof(constant->consttype) +
		    sizeof(constant->constlen) +
		    sizeof(constant->constbyval) +
		    sizeof(constant->constisnull);
	if (s!= NULL) {
	    bcopy((char *)&constant->consttype, s, sizeof(constant->consttype));
	    s += sizeof(constant->consttype);
	    bcopy((char *)&constant->constlen, s, sizeof(constant->constlen));
	    s += sizeof(constant->constlen);
	    bcopy((char *)&constant->constbyval, s, sizeof(constant->constbyval));
	    s += sizeof(constant->constbyval);
	    bcopy((char *)&constant->constisnull, s,
				sizeof(constant->constisnull));
	    s += sizeof(constant->constisnull);
	}
	/*
	 * Now copy the datum value...
	 */
	tempsize = datumGetSize(constant->constvalue,
				constant->consttype,
				constant->constbyval,
				constant->constlen);
	size += tempsize + sizeof(tempsize);
	if (s!= NULL) {
	    if (constant->constbyval) {
		bcopy((char *)&tempsize, s, sizeof(tempsize));
		s += sizeof(tempsize);
		bcopy((char *)&(constant->constvalue), s, tempsize);
		s += tempsize;
	    } else {
		bcopy((char *)&tempsize, s, sizeof(tempsize));
		s += sizeof(tempsize);
		bcopy((char *)DatumGetPointer(constant->constvalue), s, tempsize);
		s += tempsize;
	    }
	}
    } else {
	/*
	 * operand is not Const or Param
	 */
	elog(WARN, "prs2OperandToRawOperand: operand is not Const or Param");
    }

    return(size);
}

/*---------------------------------------------------------------------
 * prs2RawOperandToOperand
 *
 * This is the inverse of 'prs2OperandToRawOperand'.
 * Given a stream of bytes (the "flat" representation of an operand)
 * recreate the operand itself.
 *
 * NOTE: 's' is a pointer to a pointer.
 *---------------------------------------------------------------------
 */
static
Node
prs2RawOperandToOperand(s)
char **s;
{
    Size size;
    char *ptr;
    Param param;
    Const constant;

    if (**s == 'p') {
	/*
	 * This is a param node.
	 */
	*s += 1;	/* skip the 'p' */
	param = RMakeParam();
	bcopy(*s, (char*)&(param->paramkind), sizeof(param->paramkind));
	*s += sizeof(param->paramkind);
	if (param->paramkind != PARAM_NEW && param->paramkind != PARAM_OLD) {
	    elog(WARN,"prs2RawOperandToOperand: param is not NEW or OLD");
	}
	bcopy(*s, (char*)&(param->paramid), sizeof(param->paramid));
	*s += sizeof(param->paramid);
	param->paramname = (Name) palloc(sizeof(*(param->paramname)));
	bcopy(*s, (char*)(param->paramname), sizeof(*(param->paramname)));
	*s += sizeof(*(param->paramname));
	bcopy(*s, (char*)&(param->paramtype), sizeof(param->paramtype));
	*s += sizeof(param->paramtype);
	return((Node) param);
    } else if (**s == 'c') {
	/*
	 * This is a const node
	 */
	*s += 1;	/* skip the 'c' */
	constant = RMakeConst();
	bcopy(*s, (char*)&(constant->consttype), sizeof(constant->consttype));
	*s += sizeof(constant->consttype);
	bcopy(*s, (char*)&(constant->constlen), sizeof(constant->constlen));
	*s += sizeof(constant->constlen);
	bcopy(*s, (char*)&(constant->constbyval), sizeof(constant->constbyval));
	*s += sizeof(constant->constbyval);
	bcopy(*s, (char*)&(constant->constisnull), sizeof(constant->constisnull));
	*s += sizeof(constant->constisnull);
	/*
	 * now the variable length part...
	 */
	bcopy(*s, (char *)&(size), sizeof(size));
	*s += sizeof(size);
	if (constant->constbyval) {
	    bcopy(*s, (char *)&(constant->constvalue), size);
	    *s += size;
	} else {
	    if (size > 0) {
		/*
		 * allocate memory for the constant's value
		 */
		ptr = palloc(size);
		if (ptr==NULL) {
		    elog(WARN,"prs2RawOperandToOperand: out of memory!");
		}
		bcopy(*s, ptr, size);
		*s += size;
	    } else {
		ptr = NULL;
	    }
	    constant->constvalue = PointerGetDatum(ptr);
	}
	return((Node) constant);
    } else {
	elog(WARN, "prs2RawOperandToOperand: This is node a Param or Const");
    }
}
