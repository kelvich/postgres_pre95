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
#include "catname.h"
#include "tupdesc.h"
#include "ftup.h"
#include "log.h"
#include "prs2.h"
#include "prs2stub.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_relation.h"
#include "lsyscache.h"


/*-------------------------------------------------------------------
 * LOCAL STUFF...
 */
static void prs2ChangeRelationStub();


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
 */

static void
prs2ChangeRelationStub(relation, relstub, addFlag)
Relation relation;
Prs2OneStub relstub;
bool addFlag;
{

    Relation relationRelation;
    HeapScanDesc scanDesc;
    ScanKeyData scanKey;
    HeapTuple tuple;
    Buffer buffer;
    HeapTuple newTuple;
    Prs2Stub currentStubs;

    /*
     * Add a stub record a a relation
     * Go to the RelationRelation (i.e. pg_relation), find the
     * appropriate tuple, and add the specified lock to it.
     */
    relationRelation = RelationNameOpenHeapRelation(RelationRelationName);

    scanKey.data[0].flags = 0;
    scanKey.data[0].attributeNumber = ObjectIdAttributeNumber;
    scanKey.data[0].procedure = ObjectIdEqualRegProcedure;
    scanKey.data[0].argument = ObjectIdGetDatum(relation->rd_id);
    scanDesc = RelationBeginHeapScan(relationRelation,
					0, NowTimeQual,
					1, &scanKey);
    
    tuple = HeapScanGetNextTuple(scanDesc, 0, &buffer);
    if (!HeapTupleIsValid(tuple)) {
	elog(WARN, "AddStub: Invalid rel OID %ld", relation->rd_id);
    }

    /*
     * We have found the appropriate tuple of the RelationRelation.
     * Now find its old locks, and add the new one
     */
    currentStubs = prs2GetStubsFromRelationTuple(tuple, buffer,
				RelationGetTupleDescriptor(relationRelation));

    if (addFlag) {
	prs2AddOneStub(currentStubs, relstub);
    } else {
	prs2DeleteOneStub(currentStubs, relstub);
    }

    /*
     * Create a new tuple (i.e. a copy of the old tuple
     * with its stub field changed and replace the old
     * tuple in the RelationRelation
     */
    newTuple = prs2PutStubsInRelationTuple(tuple, buffer,
		    RelationGetTupleDescriptor(relationRelation),
		    currentStubs);
    
#ifdef STUB_DEBUG
    if (STUB_DEBUG > 3) {
	printf("prs2ChangeRelationStub (op=%s) NEW TUPLE=\n",
		addFlag ? "add" : "delete");
	debugtup(newTuple, RelationGetTupleDescriptor(relationRelation));
    }
#endif STUB_DEBUG

    RelationReplaceHeapTuple(relationRelation, &(tuple->t_ctid),
			    newTuple, (double *)NULL);
    
    RelationCloseHeapRelation(relationRelation);

}

/*======================================================================
 *
 * prs2GetRelationStubs
 *
 * given a relation OID, find all the associated rule stubs.
 *
 */
Prs2Stub
prs2GetRelationStubs(relOid)
ObjectId relOid;
{

    Prs2RawStub rawStubs;
    Prs2Stub relstub;

    /*
     * find the Prs2RawStub of the relation
     */
    rawStubs = get_relstub(relOid);
    if (rawStubs == NULL) {
	elog(WARN,
	    "prs2GetRelationStubs: cache lookup failed for relId = %ld",
	    relOid);
    }

    /*
     * now transform the Prs2RawStub to a Prs2Stub
     */
    relstub = prs2RawStubToStub(rawStubs);

    /*
     * free the Prs2RawStub
     * NOTE: we do that because get_relstub creates a COPY of
     * the raw relation stubs found in the tuple.
     */
    pfree(rawStubs);

    return(relstub);
}

/*======================================================================
 *
 * prs2GetStubsFromRelationTuple
 *
 * given a tuple form the relation relation, return
 * the stubs stroed in this tuple
 */
Prs2Stub
prs2GetStubsFromRelationTuple(tuple, buffer, tupleDescriptor)
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
			    RelationStubAttributeNumber,
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
 * prs2PutStubsInRelationTuple
 *
 * Store the given stubs to a Relation relation tuple
 * return the new tuple
 *
 */
HeapTuple
prs2PutStubsInRelationTuple(tuple, buffer, tupleDescriptor, relstubs)
HeapTuple tuple;
Buffer buffer;
TupleDescriptor tupleDescriptor;
Prs2Stub relstubs;
{
    Datum values[RelationRelationNumberOfAttributes];
    char null[RelationRelationNumberOfAttributes];
    int i;
    HeapTuple newTuple;
    Datum datum;
    Boolean isNull;
    Prs2RawStub rawStub;

    /*
     * extract the values of the old tuple
     */
    for (i=0; i<RelationRelationNumberOfAttributes; i++) {
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
    values[RelationStubAttributeNumber-1] = datum;
    null[RelationStubAttributeNumber-1] = ' ';
    pfree(rawStub);

    /*
     * Now form the new tuple
     */
    newTuple = FormHeapTuple(
		    RelationRelationNumberOfAttributes,
		    tupleDescriptor,
		    values,
		    null);

    return(newTuple);

}
