/*===================================================================
 *
 * FILE:
 * stubrel.c
 *
 * IDENTIFICATION:
 * $Header$
 *
 *
 * DESCRIPTION:
 * These routines access the system relation "pg_prs2stub.h" where
 * the ruls stubs for the various relations are stored.
 * Each relation can have zero or more stubs, and unfortunately
 * if it has more "more" than "zero" we end up with a total stub
 * size greater than 8K. And as tuples can NOT be longer than 8K,
 * and big objects are not implemented yet, we have to break up the
 * stubs in smaller pieces, each one stores in a separate tuple.
 *
 * The pg_prs2stub relation has the following fields:
 *	prs2relid:	the oid of the relation
 *	prs2islast:	true if this is the last piece of stub
 *			we have to look for...
 * 	prs2no:		the number of the stub piece stored in this
 *			tuple (we start countiong from 0).
 *	prs2stub:	the stub (or a small piece of it..)
 *
 * for example, if the relation with oid 1234 has its stub stored
 * in 3 pieces, then pg_prs2stub will contain the following 3 tuples:
 *
 *   prs2relid   prs2islast  prs2no	prs2stub
 *   ---------   ----------  ------     ---------
 *	1234		f	0	< 1st piece >
 *	1234		f	1	< 2nd piece >
 *	1234		t	2	< 3rd piece >
 *
 * The 'prs2stub' field is of type "stub", and in order to reconstruct
 * the "total" stub we have to create the union of them.
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


static Prs2Stub prs2SlowlyGetRelationStub();

#ifdef OBSOLETE
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

	ScanKeyEntryInitialize(&scanKey.data[0], 0, Anum_pg_prs2stub_prs2relid, 
						   ObjectIdEqualRegProcedure,
						   ObjectIdGetDatum(relation->rd_id));

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
	    char nullarr[Natts_pg_prs2stub];
	    int i;
	    for (i=0; i<Natts_pg_prs2stub; i++)
		nullarr[i] = 'n';
	    values[Anum_pg_prs2stub_prs2relid-1] = 
			ObjectIdGetDatum(relation->rd_id);
	    nullarr[Anum_pg_prs2stub_prs2relid-1] = ' ';
	    values[Anum_pg_prs2stub_prs2stub-1] = 
		    StructPointerGetDatum(prs2StubToRawStub(prs2MakeStub()));
	    nullarr[Anum_pg_prs2stub_prs2stub-1] = ' ';
	    newTupleFlag = true;
	    tuple = FormHeapTuple(Natts_pg_prs2stub,
				RelationGetTupleDescriptor(prs2stubRelation),
				values,
				nullarr);
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
#endif

/*-------------------------------------------------------------------
 *
 * prs2DeleteRelationStub
 *
 * Delete the given stub from the given relation
 *
 */
void
prs2DeleteRelationStub(relation, relstub)
Relation relation;
Prs2OneStub relstub;
{
    ObjectId reloid;
    Prs2Stub stubs;

    reloid = relation->rd_id;
    stubs = prs2GetRelationStubs(reloid);
    prs2DeleteOneStub(stubs, relstub);
    prs2ReplaceRelationStub(reloid, stubs);
}

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
    ObjectId reloid;
    Prs2Stub stubs;

    reloid = relation->rd_id;
    stubs = prs2GetRelationStubs(reloid);
    prs2AddOneStub(stubs, relstub);
    prs2ReplaceRelationStub(reloid, stubs);
}

/*-------------------------------------------------------------------
 *
 * prs2ReplaceRelationStub
 *
 * Replace the relation-level stubs of the given relation
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
    NameData relname;

    Prs2RawStub *pieces;
    int npieces;
    Datum values[Natts_pg_prs2stub];
    char nullarr[Natts_pg_prs2stub];
    int i,j;


    /*
     * First transform this stub in a raw stub (i.e. Prs2RwStub)
     * and break it into small pieces, (so that each tuple is
     * smaller than 8K.
     */
    pieces = prs2StubToSmallRawStubs(newStubs, 7000, &npieces);
    
    /*
     * Go to the pg_prs2stub relation (i.e. pg_relation), and start
     * scanning it for the appropriate tuples.
     */
    strcpy(relname.data, Name_pg_prs2stub);

    prs2stubRelation = RelationNameOpenHeapRelation(&relname);

    ScanKeyEntryInitialize(&scanKey.data[0], 0, Anum_pg_prs2stub_prs2relid,
					       ObjectIdEqualRegProcedure,
					       ObjectIdGetDatum(relationOid));

    scanDesc = RelationBeginHeapScan(prs2stubRelation,
					0, NowTimeQual,
					1, &scanKey);
    
    /*
     * delete all the old tuples
     */
    do {
	tuple = HeapScanGetNextTuple(scanDesc, 0, &buffer);
	if (HeapTupleIsValid(tuple)) {
	    heap_delete(prs2stubRelation, &(tuple->t_ctid));
#ifdef STUB_DEBUG
	    printf("prs2ReplaceRelationStub OLD TUPLE =\n");
	    debugtup(tuple,
		RelationGetTupleDescriptor(prs2stubRelation));
#endif STUB_DEBUG
	}
    } while (HeapTupleIsValid(tuple));

    /*
     * and add thew new ones...
     */
    for (j=0; j<Natts_pg_prs2stub; j++)
	nullarr[j] = 'n';
    values[Anum_pg_prs2stub_prs2relid-1] = 
		ObjectIdGetDatum(relationOid);
    nullarr[Anum_pg_prs2stub_prs2relid-1] = ' ';
    for (i=0; i<npieces; i++) {
	/*
	 * create a new tuple
	 */
	if (i==npieces-1)
	    values[Anum_pg_prs2stub_prs2islast-1] = CharGetDatum(true);
	else
	    values[Anum_pg_prs2stub_prs2islast-1] = CharGetDatum(false);
	nullarr[Anum_pg_prs2stub_prs2islast-1] = ' ';
	values[Anum_pg_prs2stub_prs2no-1] = Int32GetDatum(i);
	nullarr[Anum_pg_prs2stub_prs2no-1] = ' ';
	values[Anum_pg_prs2stub_prs2stub-1] = 
		StructPointerGetDatum(pieces[i]);
	nullarr[Anum_pg_prs2stub_prs2stub-1] = ' ';
	tuple = FormHeapTuple(Natts_pg_prs2stub,
			    RelationGetTupleDescriptor(prs2stubRelation),
			    values,
			    nullarr);
	/*
	 * append it to the relation
	 */
#ifdef STUB_DEBUG
	printf("prs2ReplaceRelationStub NEW TUPLE (%d out of %d) =\n",
		i, npieces);
	debugtup(tuple, RelationGetTupleDescriptor(prs2stubRelation));
#endif STUB_DEBUG
	heap_insert(prs2stubRelation, tuple, (double *)NULL);
    }
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

    Prs2RawStub temp, rawStubs;
    Prs2RawStub rawStubsPiece;
    Prs2Stub relstub;
    int no;
    bool islast;

    /*
     * find the Prs2RawStub of the relation
     * If there is no entry for this relation in pg_prs2stub
     * then `get_relstubs' will return NULL
     *
     * NOTE: get_relstub creates a COPY of
     * the raw relation stubs found in the tuple.
     * so this memory should be freed when no longer needed.
     */

    no = 0;
    rawStubs = NULL;
    do {
	rawStubsPiece = get_relstub(relOid, no, &islast);
	no++;
	if (rawStubsPiece == NULL) {
	    islast = true;
	} else {
	    /*
	     * we found s piece of the stub.
	     * "Append" it to the current stubs...
	     */
	    if (rawStubs == NULL) {
		rawStubs = rawStubsPiece;
	    } else {
		temp = prs2RawStubUnion(rawStubs, rawStubsPiece);
		pfree(rawStubsPiece);
		pfree(rawStubs);
		rawStubs = temp;
	    }
	}
    } while (!islast);

    if (rawStubs==NULL) {
	/*
	 * no stubs found for this relation
	 */
	relstub = prs2MakeStub();
    } else {
	relstub = prs2RawStubToStub(rawStubs);
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
    Prs2RawStub temp, rawStub, rawStubPiece;
    NameData relname;
    Datum datum;
    Boolean isNull;

    /*
     * Go to the pg_prs2stub relation (i.e. pg_relation), find the
     * appropriate tuple
     */
    strcpy(relname.data, Name_pg_prs2stub);

    prs2stubRelation = RelationNameOpenHeapRelation(&relname);

    ScanKeyEntryInitialize(&scanKey.data[0], 0, Anum_pg_prs2stub_prs2relid,
					       ObjectIdEqualRegProcedure, 
					       ObjectIdGetDatum(relationOid));

    scanDesc = RelationBeginHeapScan(prs2stubRelation,
					0, NowTimeQual,
					1, &scanKey);
    
    /*
     * find all the tuples of pg_prs2stub for this relation.
     */
    rawStub = NULL;
    do {
	tuple = HeapScanGetNextTuple(scanDesc, 0, &buffer);
	if (HeapTupleIsValid(tuple)) {
	    datum = HeapTupleGetAttributeValue(
			    tuple,
			    buffer,
			    Anum_pg_prs2stub_prs2stub,
			    RelationGetTupleDescriptor(prs2stubRelation),
			    &isNull);
	    if (!isNull) {
		rawStubPiece = (Prs2RawStub) DatumGetPointer(datum);
		if (rawStub == NULL) {
		    rawStub = rawStubPiece;
		} else {
		    temp = prs2RawStubUnion(rawStub, rawStubPiece);
		    pfree(rawStubPiece);
		    pfree(rawStub);
		    rawStub = temp;
		}
	    }
	}
    } while (HeapTupleIsValid(tuple));

    if (rawStub == NULL) {
	result = prs2MakeStub();
    } else {
	result = prs2RawStubToStub(rawStub);
	pfree(rawStub);
    }

    RelationCloseHeapRelation(prs2stubRelation);

    return(result);

}

#ifdef OBSOLETE
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
    char nullarr[Natts_pg_prs2stub];
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
	nullarr[i] = isNull ? 'n' : ' ';
    }

    /*
     * change the value of the 'stub' attribute of the tuple
     */
    rawStub = prs2StubToRawStub(relstubs);
    datum = PointerGetDatum(rawStub);
    values[Anum_pg_prs2stub_prs2stub-1] = datum;
    nullarr[Anum_pg_prs2stub_prs2stub-1] = ' ';

    /*
     * Now form the new tuple
     */
    newTuple = FormHeapTuple(
		    Natts_pg_prs2stub,
		    tupleDescriptor,
		    values,
		    nullarr);

    pfree(rawStub);
    return(newTuple);

}
#endif OBSOLETE
