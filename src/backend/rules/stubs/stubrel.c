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

#ifdef NOT_YET 

#include <stdio.h>
#include "catname.h"
#include "rproc.h"
#include "log.h"
#include "prs2.h"
#include "prs2stub.h"

/*-------------------------------------------------------------------
 *
 * prs2AddRelStub
 *
 */
void
prs2AddRelStub(relation, stub)
Relation relation;
Prs2Stub stub;
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
    scanKey.data[0].argument.objectId.value = relation->rd_id;
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
    currentStubs = prs2GetStubsFromTuple(tuple, buffer,
					relationRelation->rd_att);

    prs2AddOneStub(currentStubs, stub);

    /*
     * Create a new tuple (i.e. a copy of the old tuple
     * with its stub field changed and replace the old
     * tuple in the RelationRelation
     */
    newTuple = prs2PutStubsInTuple(tuple, buffer,
		    relationRelation,
		    currentStubs);

    RelationReplaceHeapTuple(relationRelation, &(tuple->t_ctid),
			    newTuple, (double *)NULL);
    
    RelationCloseHeapRelation(relationRelation);

}

#endif NOT_YET
