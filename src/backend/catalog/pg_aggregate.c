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
 *	of this tuple are aggregate name, owner id, transition function
 *	(called aggfunc1), final function (aggfunc2), internal type (return
 *	type of the transition function), and final type (return type of
 *	the final function).  All types and functions must have been defined
 *	prior to defining the aggregate.
 * ---------------
 */
int /* return status */
AggregateDefine(aggName, aggtransfn1Name, aggtransfn2Name, aggfinalfnName, 
		agginitval1, agginitval2)
	Name 	aggName, aggtransfn1Name, aggtransfn2Name, aggfinalfnName; 
	String	agginitval1, agginitval2;
{
	register	i;
	Relation	aggdesc;
	HeapTuple	tup;
	char		nulls[Natts_pg_aggregate];
	char		*values[Natts_pg_aggregate];
	Form_pg_proc	proc;
	ObjectId	xfn1 = InvalidObjectId;
	ObjectId	xfn2 = InvalidObjectId;
	ObjectId	ffn = InvalidObjectId;
	ObjectId	xret1 = InvalidObjectId;
	ObjectId	xret2 = InvalidObjectId;
	ObjectId	fret = InvalidObjectId;
	ObjectId	farg;

	/* sanity checks */
	if (!NameIsValid(aggName))
		elog(WARN, "AggregateDefine: no aggregate name supplied");
	tup = SearchSysCacheTuple(AGGNAME, aggName->data,
				  (char *) NULL, (char *) NULL, (char *) NULL);
	if (HeapTupleIsValid(tup))
		elog(WARN, "AggregateDefine: aggregate \"%-*s\" already exists",
		     sizeof(NameData), aggName->data);
	
	if (NameIsValid(aggtransfn1Name)) {
		tup = SearchSysCacheTuple(PRONAME, aggtransfn1Name->data,
					  (char *) NULL, (char *) NULL,
					  (char *) NULL);
		if(!HeapTupleIsValid(tup))
			elog(WARN, "AggregateDefine: \"%-*s\" does not exist",
			     sizeof(NameData), aggtransfn1Name->data);
		xfn1 = tup->t_oid;
		xret1 = ((Form_pg_proc) GETSTRUCT(tup))->prorettype;
		if (!ObjectIdIsValid(xfn1) || !ObjectIdIsValid(xret1))
			elog(WARN, "AggregateDefine: bogus function \"%-*s\"",
			     sizeof(NameData), aggtransfn1Name->data);
	}
	if (NameIsValid(aggtransfn2Name)) {
		tup = SearchSysCacheTuple(PRONAME, aggtransfn2Name->data,
					  (char *) NULL, (char *) NULL,
					  (char *) NULL);
		if(!HeapTupleIsValid(tup))
			elog(WARN, "AggregateDefine: \"%-*s\" does not exist",
			     sizeof(NameData), aggtransfn2Name->data);
		xfn2 = tup->t_oid;
		xret2 =  ((Form_pg_proc) GETSTRUCT(tup))->prorettype;
		if (!ObjectIdIsValid(xfn2) || !ObjectIdIsValid(xret2))
			elog(WARN, "AggregateDefine: bogus function \"%-*s\"",
			     sizeof(NameData), aggtransfn2Name->data);
	}
	if (NameIsValid(aggfinalfnName)) {
		tup = SearchSysCacheTuple(PRONAME, aggfinalfnName->data,
					  (char *) NULL, (char *) NULL,
					  (char *) NULL);
		if(!HeapTupleIsValid(tup))
			elog(WARN, "AggregateDefine: \"%-*s\" does not exist",
			     sizeof(NameData), aggfinalfnName->data);
		ffn = tup->t_oid;
		proc = (Form_pg_proc) GETSTRUCT(tup);
		fret = proc->prorettype;
		farg = proc->proargtypes.data[0];
		if (!ObjectIdIsValid(ffn) ||
		    !ObjectIdIsValid(fret) ||
		    !ObjectIdIsValid(farg) ||
		    (proc->pronargs > 2) ||
		    (proc->pronargs == 2 &&
		     farg != proc->proargtypes.data[1]))
			elog(WARN, "AggregateDefine: bogus function \"%-*s\"",
			     sizeof(NameData), aggfinalfnName->data);
	}
	
	if (!ObjectIdIsValid(xfn1) && !ObjectIdIsValid(xfn2))
		elog(WARN, "AggregateDefine: no valid transition functions provided");
	if (!ObjectIdIsValid(xret1))
		xret1 = xret2;
	if (ObjectIdIsValid(farg) && (farg != xret1))
		elog(WARN, "AggregateDefine: final function argument type (%d) != transition function return type (%d)",
		     farg, xret1);
	if (ObjectIdIsValid(xfn2) && !PointerIsValid(agginitval2))
		elog(WARN, "AggregateDefine: second transition function MUST have an initial value");
	
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
	values[Anum_pg_aggregate_aggtranstype-1] = (char *) xret1;
	values[Anum_pg_aggregate_aggfinaltype-1] = (char *) fret;
	values[Anum_pg_aggregate_agginitval1-1] = agginitval1;
	values[Anum_pg_aggregate_agginitval2-1] = agginitval2;

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
AggNameGetInitVal(aggName, initValAttno, isNull)
	char	*aggName;
	int	initValAttno;
	bool	*isNull;
{
	HeapTuple	tup;
	Relation	aggRel;
	ObjectId	transtype;
	text		*textInitVal;
	char		*strInitVal, *initVal;
	extern char	*textout();

	Assert(PointerIsValid((Pointer) aggName));
	Assert(PointerIsValid((Pointer) isNull));

	tup = SearchSysCacheTuple(AGGNAME, aggName, (char *) NULL,
				  (char *) NULL, (char *) NULL);
	if (!HeapTupleIsValid(tup))
		elog(WARN, "AggNameGetInitVal: cache lookup failed for aggregate \"%-*s\"",
		     sizeof(NameData), aggName);
	transtype = ((Form_pg_aggregate) GETSTRUCT(tup))->aggtranstype;

	aggRel = heap_openr(AggregateRelationName);
	if (!RelationIsValid(aggRel))
		elog(WARN, "AggNameGetInitVal: could not open \"%-*s\"",
		     sizeof(NameData), AggregateRelationName->data);
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
