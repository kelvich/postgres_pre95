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

#include "catalog/catname.h"
#include "catalog/syscache.h"
#include "catalog/pg_operator.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_aggregate.h"

/* --------------------
 *  support  function in
 *  utils/adt/regproc.c --
 *  converts procname to proid.
 * -------------------
 */

extern int32 regprocin();

ObjectId
AggregateGetWithOpenRelation(pg_aggregate_desc, aggName,
			    int1ObjectId, int2ObjectId, finObjectId)
    Relation	pg_aggregate_desc;  /*reldesc for pg_aggregate */
    Name	aggName;	    /* name of aggregate to fetch */
    ObjectId	int1ObjectId;	    /* oid of internal function1 */
    ObjectId	int2ObjectId;	    /* oid of internal function2 */
    ObjectId    finObjectId;	    /* oid of final function */
{
    HeapScanDesc	pg_aggregate_scan;
    ObjectId		aggregateObjectId;
    HeapTuple		tup;

    static ScanKeyEntryData     aggKey[4] = {
	    { 0, AggregateNameAttributeNumber,  NameEqualRegProcedure },
	    { 0, AggregateIntFunc1AttributeNumber,  ObjectIdEqualRegProcedure },
	    { 0, AggregateIntFunc2AttributeNumber, ObjectIdEqualRegProcedure },
	    { 0, AggregateFinFuncAttributeNumber, ObjectIdEqualRegProcedure },
	};

    fmgr_info(NameEqualRegProcedure,     &aggKey[0].func, &aggKey[0].nargs);
    fmgr_info(ObjectIdEqualRegProcedure, &aggKey[1].func, &aggKey[1].nargs);
    fmgr_info(ObjectIdEqualRegProcedure, &aggKey[2].func, &aggKey[2].nargs);
    fmgr_info(ObjectIdEqualRegProcedure, &aggKey[3].func, &aggKey[3].nargs);


	/*----------------
	 * form scan key
	 * --------------
	 */
     aggKey[0].argument = NameGetDatum(aggName);
     aggKey[1].argument = ObjectIdGetDatum(int1ObjectId);
     aggKey[2].argument = ObjectIdGetDatum(int2ObjectId);
     aggKey[3].argument = ObjectIdGetDatum(finObjectId);

	/* begin the scan */

     pg_aggregate_scan = heap_beginscan(pg_aggregate_desc,
				   	0,
					SelfTimeQual,
					3,
					(ScanKey) aggKey);

 	/* fetch the aggregate tuple, if it exists, and determine the
	 * proper return oid value.
	 */
        tup = heap_getnext(pg_aggregate_scan, 0, (Buffer *) 0);
        aggregateObjectId = HeapTupleIsValid(tup) ? tup->t_oid : InvalidObjectId;
	/* close the scan and return the oid. */

	heap_endscan(pg_aggregate_scan);

	return
		aggregateObjectId;
}

/* ------------------------------
 *  AggregateGet
 *
 *	finds the aggregate associated with the specified name and 
 *	internal and final functions names.
 * -----------------------------
 */
ObjectId
AggregateGet(aggName, int1funcName, int2funcName, finfuncName)
    Name 	aggName;  /* name of aggregate */
    Name	int1funcName;  /* internal functions  */
    Name	int2funcName;
    Name	finfuncName; /* final function*/
{
    Relation		pg_aggregate_desc;
    HeapScanDesc	pg_aggregate_scan;
    HeapTuple		tup;

    ObjectId		aggregateObjectId;
    ObjectId		int1funcObjectId = InvalidObjectId;
    ObjectId		int2funcObjectId = InvalidObjectId;
    ObjectId		finfuncObjectId = InvalidObjectId;

 	/* sanity checks */

    Assert(NameIsValid(aggName));
    Assert(NameIsValid(int1funcName) || NameIsValid(finfuncName) || \
					NameIsValid(int2funcName));

    /* look up aggregate functions.  Note: functions must be defined before
     * aggregates
     */
     if(NameIsValid(int1funcName)) {
        int1funcObjectId = (ObjectId) regprocin(int1funcName);
	if(!ObjectIdIsValid(int1funcObjectId)) {
	    elog(WARN, "AggregateGet: internal function %s not registered",
						int1funcName);
	}
     }
     if(NameIsValid(int2funcName)) {
	int2funcObjectId = (ObjectId) regprocin(int2funcName);
	if(!ObjectIdIsValid(int2funcObjectId)) {
	    elog(WARN, "AggregateGet: internal function %s not registered",
						int2funcName);
	}
     }
     if (NameIsValid(finfuncName)) {
	 finfuncObjectId = (ObjectId) regprocin(finfuncName);
	 if(!ObjectIdIsValid(finfuncObjectId)) {
	     elog(WARN, "AggregateGet: final function %s not registered",
						      finfuncName);
     	 }
     }

	/* open the pg_aggregate relation */

     pg_aggregate_desc = heap_openr(AggregateRelationName);

	/* get the oid for the aggregate with the appropriate name
	 * and internal/final final functions. 
	 */
     aggregateObjectId = AggregateGetWithOpenRelation(pg_aggregate_desc,
							aggName,
							int1funcObjectId,
							int2funcObjectId,
							finfuncObjectId);
	/* close the relation and return the aggregate oid*/

     heap_close(pg_aggregate_desc);

     return
	aggregateObjectId;
}

/* ----------------
 *	AggregateDef
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
AggregateDef(aggName, xitionfunc1Name, xitionfunc2Name, finalfuncName, 
				initaggval, initsecval)
    Name 	aggName;
    Name	xitionfunc1Name, xitionfunc2Name, finalfuncName; 
				/* step and final functions,resp.*/
    char *initaggval;
    char *initsecval;	/* initial conditions */
{
    register		i;
    Relation		pg_aggregate_desc;
    HeapTuple		tup;
    bool		defined;
    char		nulls[AggregateRelationNumberOfAttributes];
    char		*values[AggregateRelationNumberOfAttributes];
    ObjectId		finObjectId;
    ObjectId		int1ObjectId;
    ObjectId		int2ObjectId;
    ObjectId		aggregateObjectId;

    /* sanity checks */
    Assert(NameIsValid(aggName));
    Assert(NameIsValid(xitionfunc1Name));
    Assert(NameIsValid(xitionfunc2Name));
    Assert(NameIsValid(finalfuncName));

    aggregateObjectId = AggregateGet(aggName,
				     xitionfunc1Name,
				     xitionfunc2Name,
				     finalfuncName);

	/* initialize nulls and values */
   	for(i=0; i < AggregateRelationNumberOfAttributes; i++) {
	    nulls[i] = ' ';
	    values[i] = (char *) NULL;
	}

    /* look up aggfuncs in registered procedures--find the return type of
     * the function name to determine the internal and final types.
     */
     tup = SearchSysCacheTuple(PRONAME,
			      (char *) xitionfunc1Name,
			      (char *) NULL,
			      (char *) NULL,
			      (char *) NULL);
     if(!PointerIsValid(tup))
	elog(WARN, "AggregateDefine: transition function % is nonexistent",
				xitionfunc1Name);
     
     values[ AggregateIntFunc1AttributeNumber-1] = (char *) tup->t_oid;
     values[ AggregateInternalTypeAttributeNumber-1] = 
	 (char *) ((struct proc *) GETSTRUCT(tup))->prorettype;

     tup = SearchSysCacheTuple(PRONAME,
			     (char *) xitionfunc2Name,
			     (char *) NULL,
			     (char *) NULL,
    			     (char *) NULL);
     if(!PointerIsValid(tup))
	elog(WARN, "AggregateDefine: transition function % is nonexistent",
				xitionfunc2Name);

    values[ AggregateIntFunc2AttributeNumber-1] = (char *) tup->t_oid;

     tup = SearchSysCacheTuple(PRONAME,
				(char *) finalfuncName,
				(char *) NULL,
				(char *) NULL,
				(char *) NULL);

     if(!PointerIsValid(tup))
	elog(WARN, "AggregateDefine: final function % is nonexistent",
					     finalfuncName);

     values[AggregateFinFuncAttributeNumber-1] = (char *)tup->t_oid;
     values[AggregateFinalTypeAttributeNumber-1] = 
	 (char *)((struct proc *) GETSTRUCT(tup))->prorettype;

    /* set up values for aggregate tuples */
    values[0] = (char *) aggName;
    values[1] = (char *) (ObjectId) getuid();
    values[7] = (char *) initaggval;
    values[8] = (char *) initsecval;
    /* the others were taken care of earlier*/

    /* form tuple and insert in aggregate relation */

     pg_aggregate_desc = heap_openr(AggregateRelationName); 
     tup = heap_formtuple(AggregateRelationNumberOfAttributes,
			  &pg_aggregate_desc->rd_att,
			  values,
			  nulls);
     heap_insert(pg_aggregate_desc, (HeapTuple)tup, (double *)NULL);
     heap_close(pg_aggregate_desc);
}





