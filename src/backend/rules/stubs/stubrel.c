/*===================================================================
 *
 * FILE:
 * stubrel.c
 *
 * IDENTIFICATION:
 * $Header$
 *
 *
 *===================================================================
 */


#include <stdio.h>
#include "catalog/catname.h"
#include "access/tupdesc.h"
#include "access/ftup.h"
#include "utils/log.h"
#include "rules/prs2.h"
#include "rules/prs2stub.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_prs2stub.h"
#include "utils/lsyscache.h"
#include "utils/fmgr.h"


/*-------------------------------------------------------------------
 * LOCAL STUFF...
 */
static void prs2ChangeRelationStub();
static Prs2Stub prs2SlowlyGetRelationStub();

/*-------------------------------------------------------------------
 *
 * prs2AddRelationStub
 *
 * Add a new stub to the specified relation.
 *
 */
void
prs2AddRelationStub(relation, relstub)
Relation relation;
Prs2OneStub relstub;
{
    prs2ChangeRelationStub(relation, relstub, true);
}

/*-------------------------------------------------------------------
 *
 * prs2DeleteRelationStub
 *
 * delete the given stub from the given relation
 *-------------------------------------------------------------------
 */
void
prs2DeleteRelationStub(relation, relstub)
Relation relation;
Prs2OneStub relstub;
{
    prs2ChangeRelationStub(relation, relstub, false);
}

/*-------------------------------------------------------------------
 *
 * prs2ChangeRelationStub
 *
 * This routine is called by both 'prs2AddRelationStub' and
 * 'prs2DeleteRelationStub' and add/deletes  the given stub
 * to/from the given relation
 *
 * if `addFlag' is true then we add a stub, otherwise we delete it.
 *
 *-------------------------------------------------------------------
 */

static void
prs2ChangeRelationStub(relation, relstub, addFlag)
Relation relation;
Prs2OneStub relstub;
bool addFlag;
{

    Relation prs2stubRelation;
    HeapScanDesc scanDesc;
    ScanKeyData scanKey;
    HeapTuple tuple;
    Buffer buffer;
    HeapTuple newTuple;
    Prs2Stub currentStubs;
    bool newTupleFlag;
	NameData relname; /* Added so we can pass Name_pg_prs2stub to
					     RelationNameOpenHeapRelation */

    /*
     * Add a stub record a a relation
     * Go to the pg_prs2stub relation find the
     * appropriate tuple, and add the specified lock to it.
     */
	strcpy(relname.data, Name_pg_prs2stub);

    prs2stubRelation = RelationNameOpenHeapRelation(&relname);

    scanKey.data[0].flags = 0;
    scanKey.data[0].attributeNumber = Anum_pg_prs2stub_prs2relid;
    scanKey.data[0].procedure = ObjectIdEqualRegProcedure;
    scanKey.data[0].argument = ObjectIdGetDatum(relation->rd_id);
    scanDesc = RelationBeginHeapScan(prs2stubRelation,
					0, NowTimeQual,
					1, &scanKey);
    
    tuple = HeapScanGetNextTuple(scanDesc, 0, &buffer);
    if (HeapTupleIsValid(tuple)) {
	newTupleFlag = false;
    } else {
	/*
	 * We didn't find a tuple in pg_prs2stub for
	 * this relation.
	 * Is this a `delete stub' operation?
	 * If yes complain!
	 * Otherwise create a brand new tuple...
	 */
	if (!addFlag) {
	    elog(WARN, "DeleteRuleStub: Invalid rel OID %ld",relation->rd_id);
	} else {
	    /*
	     * We must create a new tuple
	     * Initially its 'prs2stub' field will contain
	     * an empty rule stub.
	     */
	    Datum values[Natts_pg_prs2stub];
	    char null[Natts_pg_prs2stub];
	    int i;
	    for (i=0; i<Natts_pg_prs2stub; i++)
		null[i] = 'n';
	    values[Anum_pg_prs2stub_prs2relid-1] = 
			ObjectIdGetDatum(relation->rd_id);
	    null[Anum_pg_prs2stub_prs2relid-1] = ' ';
	    values[Anum_pg_prs2stub_prs2stub-1] = 
		    StructPointerGetDatum(prs2StubToRawStub(prs2MakeStub()));
	    null[Anum_pg_prs2stub_prs2stub-1] = ' ';
	    newTupleFlag = true;
	    tuple = FormHeapTuple(Natts_pg_prs2stub,
				RelationGetTupleDescriptor(prs2stubRelation),
				values,
				null);
	}
    }

    /*
     * We have found the appropriate tuple of the pg_prs2stub relation.
     * Now find its old stubs...
     */
    currentStubs = prs2GetStubsFromPrs2StubTuple(tuple, buffer,
				RelationGetTupleDescriptor(prs2stubRelation));

    /*
     * Now add/delete the given stub from the current stubs
     */
    if (addFlag) {
	prs2AddOneStub(currentStubs, relstub);
    } else {
	prs2DeleteOneStub(currentStubs, relstub);
    }

    /*
     * Create a new tuple (a copy of the current tuple)
     * in the pg_prs2stub relation
     */
    newTuple = prs2PutStubsInPrs2StubTuple(tuple, buffer,
		    RelationGetTupleDescriptor(prs2stubRelation),
		    currentStubs);
    
#ifdef STUB_DEBUG
    if (STUB_DEBUG > 3) {
	printf("prs2ChangeRelationStub (op=%s) NEW TUPLE=\n",
		addFlag ? "add" : "delete");
	debugtup(newTuple, RelationGetTupleDescriptor(prs2stubRelation));
    }
#endif STUB_DEBUG

    if (newTupleFlag) {
	double dummy;
	pfree(tuple);
	RelationInsertHeapTuple(prs2stubRelation, newTuple, &dummy);
    } else {
	RelationReplaceHeapTuple(prs2stubRelation, &(tuple->t_ctid),
				newTuple, (double *)NULL);
    }
    
    RelationCloseHeapRelation(prs2stubRelation);

}

/*-------------------------------------------------------------------
 *
 * prs2ReplaceRelationStub
 *
 * Replace the relation-level stubs oif the given relation
 * withe the given ones. The old ones are completely
 * ignored.
 *-------------------------------------------------------------------
 */

void
prs2ReplaceRelationStub(relationOid, newStubs)
ObjectId relationOid;
Prs2Stub newStubs;
{

    Relation prs2stubRelation;
    HeapScanDesc scanDesc;
    ScanKeyData scanKey;
    HeapTuple tuple;
    Buffer buffer;
    HeapTuple newTuple;
    Prs2Stub currentStubs;
    bool newTupleFlag;
	NameData relname;

    /*
     * Go to the pg_prs2stub relation (i.e. pg_relation), find the
     * appropriate tuple, and add the specified lock to it.
     */
    strcpy(relname.data, Name_pg_prs2stub);

    prs2stubRelation = RelationNameOpenHeapRelation(&relname);

    scanKey.data[0].flags = 0;
    scanKey.data[0].attributeNumber = Anum_pg_prs2stub_prs2relid;
    scanKey.data[0].procedure = ObjectIdEqualRegProcedure;
    scanKey.data[0].argument = ObjectIdGetDatum(relationOid);
    scanDesc = RelationBeginHeapScan(prs2stubRelation,
					0, NowTimeQual,
					1, &scanKey);
    
    tuple = HeapScanGetNextTuple(scanDesc, 0, &buffer);
    if (HeapTupleIsValid(tuple)) {
	newTupleFlag = false;
    } else {
	/*
	 * We didn't find a tuple in pg_prs2stub for
	 * this relation.
	 * We must create a new tuple
	 * Initially its 'prs2stub' field will contain
	 * an empty rule stub.
	 */
	Datum values[Natts_pg_prs2stub];
	char null[Natts_pg_prs2stub];
	int i;
	for (i=0; i<Natts_pg_prs2stub; i++)
	    null[i] = 'n';
	values[Anum_pg_prs2stub_prs2relid-1] = 
		    ObjectIdGetDatum(relationOid);
	null[Anum_pg_prs2stub_prs2relid-1] = ' ';
	values[Anum_pg_prs2stub_prs2stub-1] = 
		StructPointerGetDatum(prs2StubToRawStub(prs2MakeStub()));
	null[Anum_pg_prs2stub_prs2stub-1] = ' ';
	newTupleFlag = true;
	tuple = FormHeapTuple(Natts_pg_prs2stub,
			    RelationGetTupleDescriptor(prs2stubRelation),
			    values,
			    null);
    }

    /*
     * We have found the appropriate tuple of the pg_prs2stub relation.
     * Now replace the current stubs with the given one
     */
    newTuple = prs2PutStubsInPrs2StubTuple(tuple, buffer,
		    RelationGetTupleDescriptor(prs2stubRelation),
		    newStubs);
    
#ifdef STUB_DEBUG
    if (STUB_DEBUG > 3) {
	printf("prs2ReplaceRelationStub NEW TUPLE=\n");
	debugtup(newTuple, RelationGetTupleDescriptor(prs2stubRelation));
    }
#endif STUB_DEBUG

    if (newTupleFlag) {
	double dummy;
	pfree(tuple);
	RelationInsertHeapTuple(prs2stubRelation, newTuple, &dummy);
    } else {
	RelationReplaceHeapTuple(prs2stubRelation, &(tuple->t_ctid),
				newTuple, (double *)NULL);
    }
    
    RelationCloseHeapRelation(prs2stubRelation);

}


/*======================================================================
 *
 * prs2GetRelationStubs
 *
 * given a relation OID, find all the associated rule stubs.
 *
 *======================================================================
 */
Prs2Stub
prs2GetRelationStubs(relOid)
ObjectId relOid;
{

    Prs2RawStub rawStubs;
    Prs2Stub relstub;

    /*
     * find the Prs2RawStub of the relation
     * If there is no entry for this relation in pg_prs2stub
     * then `get_relstubs' will return NULL
     */
    rawStubs = get_relstub(relOid);
    if (rawStubs == NULL) {
	relstub = prs2MakeStub();
    } else {
	/*
	 * transform the Prs2RawStub to a Prs2Stub
	 */
	relstub = prs2RawStubToStub(rawStubs);
	/*
	 * free the Prs2RawStub
	 * NOTE: we do that because get_relstub creates a COPY of
	 * the raw relation stubs found in the tuple.
	 */
	pfree(rawStubs);
    }

#ifdef OUZODEBUG
    {
	Prs2Stub relstub2;
	Name get_rel_name();
	char *s1, *s2;

	relstub2 = prs2SlowlyGetRelationStub(relOid);
	printf("--> Looking stubs for relation %ld (%s)\n",
	    relOid, get_rel_name(relOid));
	s1 = prs2StubToString(relstub);
	printf("--> syscache stub = %s\n", s1);
	s2 = prs2StubToString(relstub2);
	printf("--> relscan  stub = %s\n", s2);

	relstub = relstub2;
    }
#endif OUZODEBUG

    return(relstub);
}

/*======================================================================
 * prs2SlowlyGetRelationStub
 *
 * Get the stubs of a relation NOT by looking at the sys cache, 
 * but by actually scanning the pg_prs2stub system catalog.
 * Used for debugging (this stupid sys cache invalidation bug again,
 * argh!!!!)
 *======================================================================
 */
static
Prs2Stub
prs2SlowlyGetRelationStub(relationOid)
ObjectId relationOid;
{

    Relation prs2stubRelation;
    HeapScanDesc scanDesc;
    ScanKeyData scanKey;
    HeapTuple tuple;
    Buffer buffer;
    HeapTuple newTuple;
    Prs2Stub result;
    NameData relname;

    /*
     * Go to the pg_prs2stub relation (i.e. pg_relation), find the
     * appropriate tuple
     */
    strcpy(relname.data, Name_pg_prs2stub);

    prs2stubRelation = RelationNameOpenHeapRelation(&relname);

    scanKey.data[0].flags = 0;
    scanKey.data[0].attributeNumber = Anum_pg_prs2stub_prs2relid;
    scanKey.data[0].procedure = ObjectIdEqualRegProcedure;
    scanKey.data[0].argument = ObjectIdGetDatum(relationOid);
    scanDesc = RelationBeginHeapScan(prs2stubRelation,
					0, NowTimeQual,
					1, &scanKey);
    
    tuple = HeapScanGetNextTuple(scanDesc, 0, &buffer);
    if (HeapTupleIsValid(tuple)) {
	result = prs2GetStubsFromPrs2StubTuple(tuple, buffer,
			RelationGetTupleDescriptor(prs2stubRelation));
    } else {
	/*
	 * We didn't find a tuple in pg_prs2stub for
	 * this relation. Return an empty rule stub.
	 */
	result = prs2MakeStub();
    }

    RelationCloseHeapRelation(prs2stubRelation);

    return(result);

}

/*======================================================================
 *
 * prs2GetStubsFromPrs2StubTuple
 *
 * given a tuple form the pg_prs2stub relation, return
 * the rule stubs stored in this tuple
 *======================================================================
 */
Prs2Stub
prs2GetStubsFromPrs2StubTuple(tuple, buffer, tupleDescriptor)
HeapTuple tuple;
Buffer buffer;
TupleDescriptor tupleDescriptor;
{
    Datum datum;
    Boolean isNull;
    Prs2RawStub rawStub;
    Prs2Stub relstub;

    /*
     * extract the `Prs2RawStub' from the tuple
     * and then transform it to a `Prs2Stub'
     */
    datum = HeapTupleGetAttributeValue(
			    tuple,
			    buffer,
			    Anum_pg_prs2stub_prs2stub,
			    tupleDescriptor,
			    &isNull);

    if (isNull) {
	/*
	 * create an empty stub
	 */
	relstub = prs2MakeStub();
	return(relstub);
    } else {
	rawStub = (Prs2RawStub) DatumGetPointer(datum);
	relstub = prs2RawStubToStub(rawStub);
	return(relstub);
    }
}

/*======================================================================
 *
 * prs2PutStubsInPrs2StubTuple
 *
 * Store the given stubs to a pg_prs2stub relation tuple
 * return the new tuple
 *
 *======================================================================
 */
HeapTuple
prs2PutStubsInPrs2StubTuple(tuple, buffer, tupleDescriptor, relstubs)
HeapTuple tuple;
Buffer buffer;
TupleDescriptor tupleDescriptor;
Prs2Stub relstubs;
{
    Datum values[Natts_pg_prs2stub];
    char null[Natts_pg_prs2stub];
    int i;
    HeapTuple newTuple;
    Datum datum;
    Boolean isNull;
    Prs2RawStub rawStub;

    /*
     * extract the values of the old tuple
     */
    for (i=0; i<Natts_pg_prs2stub; i++) {
	values[i] = HeapTupleGetAttributeValue(
				tuple,
				buffer,
				i+1,
				tupleDescriptor,
				&isNull);
	null[i] = isNull ? 'n' : ' ';
    }

    /*
     * change the value of the 'stub' attribute of the tuple
     */
    rawStub = prs2StubToRawStub(relstubs);
    datum = PointerGetDatum(rawStub);
    values[Anum_pg_prs2stub_prs2stub-1] = datum;
    null[Anum_pg_prs2stub_prs2stub-1] = ' ';

    /*
     * Now form the new tuple
     */
    newTuple = FormHeapTuple(
		    Natts_pg_prs2stub,
		    tupleDescriptor,
		    values,
		    null);

    pfree(rawStub);
    return(newTuple);

}
