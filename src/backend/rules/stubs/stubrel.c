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
#include "rproc.h"
#include "anum.h"
#include "tupdesc.h"
#include "ftup.h"
#include "log.h"
#include "prs2.h"
#include "prs2stub.h"

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
prs2AddRelationStub(relation, stub)
Relation relation;
Prs2OneStub stub;
{
    prs2ChangeRelationStub(relation, stub, true);
}

/*-------------------------------------------------------------------
 *
 * prs2DeleteRelationStub
 *
 * delete the given stub from the given relation
 */
void
prs2DeleteRelationStub(relation, stub)
Relation relation;
Prs2OneStub stub;
{
    prs2ChangeRelationStub(relation, stub, false);
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
prs2ChangeRelationStub(relation, stub, addFlag)
Relation relation;
Prs2OneStub stub;
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
	prs2AddOneStub(currentStubs, stub);
    } else {
	prs2DeleteOneStub(currentStubs, stub);
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
    printf("prs2ChangeRelationStub (op=%s) NEW TUPLE=\n",
	    addFlag ? "add" : "delete");
    debugtup(newTuple, RelationGetTupleDescriptor(relationRelation));
#endif STUB_DEBUG

    RelationReplaceHeapTuple(relationRelation, &(tuple->t_ctid),
			    newTuple, (double *)NULL);
    
    RelationCloseHeapRelation(relationRelation);

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
    Prs2Stub stub;

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
	stub = prs2MakeStub();
	return(stub);
    } else {
	rawStub = (Prs2RawStub) DatumGetPointer(datum);
	stub = prs2RawStubToStub(rawStub);
	return(stub);
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
prs2PutStubsInRelationTuple(tuple, buffer, tupleDescriptor, stubs)
HeapTuple tuple;
Buffer buffer;
TupleDescriptor tupleDescriptor;
Prs2Stub stubs;
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
    rawStub = prs2StubToRawStub(stubs);
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
