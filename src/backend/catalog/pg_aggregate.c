/* -----------------------------------
 *  FILE
 * 	pg_aggregate.c
 *
 *  DESCRIPTION
 *	routines to support manipulation of the pg_aggregate relation
 *
 *  INTERFACE ROUTINES
 *
 *
 *
 *  IDENTIFICATION
 *
 * --------------------------------
 */

#include "tmp/postgres.h"

RcsId("$Header$");

#include "access/ftup.h"
#include "access/heapam.h"
#include "access/relscan.h"
#include "access/skey.h"
#include "access/htup.h"
#include "utils/rel.h"
#include "utils/log.h"
#include "fmgr.h"

#include "catalog/catname.h"
#include "catalog/syscache.h"
#include "catalog/pg_operator.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_type.h"
#include "catalog/pg_aggregate.h"

/* ----------------
 *	AggregateDefine
 *
 *	Currently, redefining aggregates using the same name is not
 *	supported.  In such a case, a warning is printed that the 
 *	aggregate already exists.  If such is not the case, a new tuple
 *	is created and inserted in the aggregate relation.  The fields
 *	of this tuple are aggregate name, owner id, 2 transition functions
 *	(called aggtransfn1 and aggtransfn2), final function (aggfinalfn),
 *	type of data on which aggtransfn1 operates (aggbasetype), return
 *	types of the two transition functions (aggtranstype1 and 
 *	aggtranstype2), final return type (aggfinaltype), and initial values
 *	for the two state transition functions (agginitval1 and agginitval2).
 *	All types and functions must have been defined
 *	prior to defining the aggregate.
 * ---------------
 */
int /* return status */
AggregateDefine(aggName, aggtransfn1Name, aggtransfn2Name, aggfinalfnName, 
		aggbasetypeName, aggtransfn1typeName, aggtransfn2typeName,
		agginitval1, agginitval2)
    Name 	aggName, aggtransfn1Name, aggtransfn2Name, aggfinalfnName; 
    Name	aggbasetypeName, aggtransfn1typeName, aggtransfn2typeName;
    String	agginitval1, agginitval2;
{
    register		i;
    Relation		aggdesc;
    HeapTuple		tup;
    char		nulls[Natts_pg_aggregate];
    char		*values[Natts_pg_aggregate];
    Form_pg_proc	proc;
    ObjectId		xfn1 = InvalidObjectId;
    ObjectId		xfn2 = InvalidObjectId;
    ObjectId		ffn = InvalidObjectId;
    ObjectId		xbase = InvalidObjectId;
    ObjectId		xret1 = InvalidObjectId;
    ObjectId		xret2 = InvalidObjectId;
    ObjectId		fret = InvalidObjectId;
    ObjectId		farg = InvalidObjectId;
    ObjectId		fnArgs[8];
    
    bzero(fnArgs, 8 * sizeof(ObjectId));

    /* sanity checks */
    if (!NameIsValid(aggName))
	elog(WARN, "AggregateDefine: no aggregate name supplied");
    tup = SearchSysCacheTuple(AGGNAME, aggName->data,
			      (char *) NULL, (char *) NULL, (char *) NULL);
    if (HeapTupleIsValid(tup))
	elog(WARN, "AggregateDefine: aggregate \"%-*s\" already exists",
	     sizeof(NameData), aggName->data);

    if (!NameIsValid(aggtransfn1Name) && !NameIsValid(aggtransfn2Name))
	elog(WARN, "AggregateDefine: aggregate must have at least one transition function");

    if (NameIsValid(aggtransfn1Name)) {
	tup = SearchSysCacheTuple(TYPNAME, aggbasetypeName);
	if(!HeapTupleIsValid(tup))
	    elog(WARN, "AggregateDefine: Type \"%-*s\" undefined",
		 sizeof(NameData), aggbasetypeName);
	xbase = tup->t_oid;

	tup = SearchSysCacheTuple(TYPNAME, aggtransfn1typeName);
	if(!HeapTupleIsValid(tup))
	    elog(WARN, "AggregateDefine: Type \"%-*s\" undefined",
		 sizeof(NameData), aggtransfn1typeName);
	xret1 = tup->t_oid;

	fnArgs[0] = xret1;
	fnArgs[1] = xbase;
	tup = SearchSysCacheTuple(PRONAME, aggtransfn1Name->data,
				  2,
				  fnArgs,
				  (char *) NULL);
	if(!HeapTupleIsValid(tup))
	    elog(WARN, "AggregateDefine: \"%-*s\"(\"%-*s\", \"%-*s\",) does not exist",
		 sizeof(NameData), aggtransfn1Name->data,
		 sizeof(NameData), aggtransfn1typeName->data,
		 sizeof(NameData), aggbasetypeName->data);
	if (((Form_pg_proc) GETSTRUCT(tup))->prorettype != xret1)
	    elog(WARN, "AggregateDefine: return type of \"%-*s\" is not \"%-*s\"",
		 sizeof(NameData), aggtransfn1Name->data,
		 sizeof(NameData), aggtransfn1typeName->data);
	xfn1 = tup->t_oid;
	if (!ObjectIdIsValid(xfn1) || !ObjectIdIsValid(xret1) ||
	    !ObjectIdIsValid(xbase))
	    elog(WARN, "AggregateDefine: bogus function \"%-*s\"",
		 sizeof(NameData), aggfinalfnName->data);
    }

    if (NameIsValid(aggtransfn2Name)) {
	tup = SearchSysCacheTuple(TYPNAME, aggtransfn2typeName);
	if(!HeapTupleIsValid(tup))
	    elog(WARN, "AggregateDefine: Type \"%-*s\" undefined",
		 sizeof(NameData), aggtransfn2typeName);
	xret2 = tup->t_oid;

	fnArgs[0] = xret2;
	fnArgs[1] = 0;
	tup = SearchSysCacheTuple(PRONAME, aggtransfn2Name->data,
				  1,
				  fnArgs,
				  (char *) NULL);
	if(!HeapTupleIsValid(tup))
	    elog(WARN, "AggregateDefine: \"%-*s\"(\"%-*s\") does not exist",
		 sizeof(NameData), aggtransfn2Name->data,
		 sizeof(NameData), aggtransfn2typeName->data);
	if (((Form_pg_proc) GETSTRUCT(tup))->prorettype != xret2)
	    elog(WARN, "AggregateDefine: return type of \"%-*s\" is not \"%-*s\"",
		 sizeof(NameData), aggtransfn2Name->data,
		 sizeof(NameData), aggtransfn2typeName->data);
	xfn2 = tup->t_oid;
	if (!ObjectIdIsValid(xfn2) || !ObjectIdIsValid(xret2))
	    elog(WARN, "AggregateDefine: bogus function \"%-*s\"",
		 sizeof(NameData), aggfinalfnName->data);
    }

    /* more sanity checks */
    if (NameIsValid(aggtransfn1Name) && NameIsValid(aggtransfn2Name) &&
	!NameIsValid(aggfinalfnName))
	elog(WARN, "AggregateDefine: Aggregate must have final function with both transition functions");

    if ((!NameIsValid(aggtransfn1Name) || !NameIsValid(aggtransfn2Name)) &&
	NameIsValid(aggfinalfnName))
	elog(WARN, "AggregateDefine: Aggregate cannot have final function without both transition functions");

    if (NameIsValid(aggfinalfnName)) {
        fnArgs[0] = xret1;
	fnArgs[1] = xret2;
	tup = SearchSysCacheTuple(PRONAME, aggfinalfnName->data,
				  2,
				  fnArgs,
				  (char *) NULL);
	if(!HeapTupleIsValid(tup))
	    elog(WARN, "AggregateDefine: \"%-*s\"(\"%-*s\", \"%-*s\") does not exist",
		 sizeof(NameData), aggfinalfnName->data,
		 sizeof(NameData), aggtransfn1typeName->data,
		 sizeof(NameData), aggtransfn2typeName->data);
	ffn = tup->t_oid;
	proc = (Form_pg_proc) GETSTRUCT(tup);
	fret = proc->prorettype;
	if (!ObjectIdIsValid(ffn) || !ObjectIdIsValid(fret))
	    elog(WARN, "AggregateDefine: bogus function \"%-*s\"",
		 sizeof(NameData), aggfinalfnName->data);
    }

    /*
     * If transition function 2 is defined, it must have an initial value,
     * whereas transition function 1 does not, which allows man and min
     * aggregates to return NULL if they are evaluated on empty sets.
     */
    if (ObjectIdIsValid(xfn2) && !PointerIsValid(agginitval2))
	elog(WARN, "AggregateDefine: transition function 2 MUST have an initial value");
    
    /* initialize nulls and values */
    for(i=0; i < Natts_pg_aggregate; i++) {
	nulls[i] = ' ';
	values[i] = (char *) NULL;
    }
    values[Anum_pg_aggregate_aggname-1] = (char *) aggName;
    values[Anum_pg_aggregate_aggowner-1] = (char *) GetUserId();
    values[Anum_pg_aggregate_aggtransfn1-1] = (char *) xfn1;
    values[Anum_pg_aggregate_aggtransfn2-1] = (char *) xfn2;
    values[Anum_pg_aggregate_aggfinalfn-1] = (char *) ffn;

    if (!ObjectIdIsValid(xfn1)) {
	values[Anum_pg_aggregate_aggbasetype-1] = (char *)InvalidObjectId;
	values[Anum_pg_aggregate_aggtranstype1-1] = (char *)InvalidObjectId;
	values[Anum_pg_aggregate_aggtranstype2-1] = (char *) xret2;
	values[Anum_pg_aggregate_aggfinaltype-1] = (char *) xret2;
    }
    else if (!ObjectIdIsValid(xfn2)) {
	values[Anum_pg_aggregate_aggbasetype-1] = (char *) xbase;
	values[Anum_pg_aggregate_aggtranstype1-1] = (char *) xret1;
	values[Anum_pg_aggregate_aggtranstype2-1] = (char *)InvalidObjectId;
	values[Anum_pg_aggregate_aggfinaltype-1] = (char *) xret1;
    }
    else {
	values[Anum_pg_aggregate_aggbasetype-1] = (char *) xbase;
	values[Anum_pg_aggregate_aggtranstype1-1] = (char *) xret1;
	values[Anum_pg_aggregate_aggtranstype2-1] = (char *) xret2;
	values[Anum_pg_aggregate_aggfinaltype-1] = (char *) fret;
    }

    if (ObjectIdIsValid(agginitval1))
	values[Anum_pg_aggregate_agginitval1-1] = agginitval1;
    else
	nulls[Anum_pg_aggregate_agginitval1-1] = 'n';

    if (ObjectIdIsValid(agginitval2))
	values[Anum_pg_aggregate_agginitval2-1] = agginitval2;
    else
	nulls[Anum_pg_aggregate_agginitval2-1] = 'n';
    
    if (!RelationIsValid(aggdesc = heap_openr(AggregateRelationName)))
	elog(WARN, "AggregateDefine: could not open \"%-*s\"",
	     sizeof(NameData), AggregateRelationName);
    if (!HeapTupleIsValid(tup = heap_formtuple(Natts_pg_aggregate,
					       &aggdesc->rd_att,
					       values,
					       nulls)))
	elog(WARN, "AggregateDefine: heap_formtuple failed");
    if (!ObjectIdIsValid(heap_insert(aggdesc, tup, (double *) NULL)))
	elog(WARN, "AggregateDefine: heap_insert failed");
    heap_close(aggdesc);
}

char *
AggNameGetInitVal(aggName, xfuncno, isNull)
    char	*aggName;
    int		xfuncno;
    bool	*isNull;
{
    HeapTuple	tup;
    Relation	aggRel;
    int		initValAttno;
    ObjectId	transtype;
    text	*textInitVal;
    char	*strInitVal, *initVal;
    extern char	*textout();
    
    Assert(PointerIsValid((Pointer) aggName));
    Assert(PointerIsValid((Pointer) isNull));
    Assert(xfuncno == 1 || xfuncno == 2);
    tup = SearchSysCacheTuple(AGGNAME, aggName, (char *) NULL,
			      (char *) NULL, (char *) NULL);
    if (!HeapTupleIsValid(tup))
	elog(WARN, "AggNameGetInitVal: cache lookup failed for aggregate \"%-*s\"",
	     sizeof(NameData), aggName);
    if (xfuncno == 1) {
	transtype = ((Form_pg_aggregate) GETSTRUCT(tup))->aggtranstype1;
	initValAttno = Anum_pg_aggregate_agginitval1;
    }
    else if (xfuncno == 2) {
	transtype = ((Form_pg_aggregate) GETSTRUCT(tup))->aggtranstype2;
	initValAttno = Anum_pg_aggregate_agginitval2;
    }
    
    aggRel = heap_openr(AggregateRelationName);
    if (!RelationIsValid(aggRel))
	elog(WARN, "AggNameGetInitVal: could not open \"%-*s\"",
	     sizeof(NameData), AggregateRelationName->data);
    /* 
     * must use fastgetattr in case one or other of the init values is NULL
     */
    textInitVal = (text *) fastgetattr(tup, initValAttno, &aggRel->rd_att,
				       isNull);
    if (!PointerIsValid((Pointer) textInitVal))
	*isNull = true;
    if (*isNull) {
	heap_close(aggRel);
	return((char *) NULL);
    }
    strInitVal = textout(textInitVal);
    heap_close(aggRel);
    
    tup = SearchSysCacheTuple(TYPOID, (char *) transtype, (char *) NULL,
			      (char *) NULL, (char *) NULL);
    if (!HeapTupleIsValid(tup)) {
	pfree(strInitVal);
	elog(WARN, "AggNameGetInitVal: cache lookup failed on aggregate transition function return type");
    }
    initVal = fmgr(((Form_pg_type) GETSTRUCT(tup))->typinput, strInitVal);
    pfree(strInitVal);
    return(initVal);
}
