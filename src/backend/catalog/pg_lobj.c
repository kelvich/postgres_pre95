/* ----------------------------------------------------------------
 *   FILE
 *      pg_lobj.c
 *
 *   DESCRIPTION
 *      routines to support manipulation of the pg_large_object relation
 *
 *   INTERFACE ROUTINES
 * LargeObject *LOassocOIDandLargeObjDesc(OID oid);
 * int LOputOIDandLargeObjDesc(OID oid, LargeObject *desc);
 *
 *   NOTES
 *
 *   IDENTIFICATION
 *      $Header$
 * ----------------------------------------------------------------
 */
/*
 * Wrappers for management of system table associating OIDs to large object
 * descriptors
 *
 * LargeObject *LOassocOIDandLargeObjDesc(OID oid);
 *    returns association if exists.
 * LOputOIDandLargeObjDesc((in) OID oid, (in) LargeObject *desc);
 *    creates association in table.
 *
 * Schema:
 *    lobjassoc is <OID,LargeObjectDescriptor> keyed on OID.
 *
 * Description of algorithms:
 *   LOassocOIDandLargeObjDesc given oid:
 *     if <OID> key exists, return large object descriptor, else return error.
 *   LOputOIDandLargeObjDesc given oid and large object descriptor:
 *     if <OID> exists, return error for now, else do a put of <OID,type,desc>.
 */

#include <stdio.h>
#include "catalog/syscache.h"
#include "catalog/pg_lobj.h"
#include "utils/rel.h"
#include "utils/large_object.h"
#include "access/heapam.h"

int LOputOIDandLargeObjDesc(objOID, desc)
     oid objOID;
     LargeObject *desc;
{
    HeapTuple objTuple;
    objTuple = SearchSysCacheTuple(LOBJREL,objOID);
    if (objTuple != NULL) { /* return error for now */
	return -1;
    } else {
	return CreateLOBJTuple(objOID,desc);
    }
}

int CreateLOBJTuple(objOID,desc)
     oid objOID;
     LargeObject *desc; /* assume structural equivalence to varlena */
{
    char *values[Natts_pg_large_object];
    char nulls[Natts_pg_large_object];
    HeapTuple tup;
    Relation lobjDesc;
    int i;

    lobjDesc = heap_openr(Name_pg_large_object);
    for (i = 0 ; i < Natts_pg_large_object; i++) {
	nulls[i] = ' ';
	values[i] = NULL;
    }
    
    i = 0;
    values[i++] = (char *) objOID;
    values[i++] = (char *) desc;
    
    tup = heap_formtuple(Natts_pg_large_object,
			 &lobjDesc->rd_att,
			 values,
			 nulls);
    heap_insert(lobjDesc,tup,(double *)NULL);

    pfree(tup);
    heap_close(lobjDesc);

    return 0;
}

LargeObject *LOassocOIDandLargeObjDesc(objOID)
     oid objOID;
{
    HeapTuple objTuple;
    objTuple = SearchSysCacheTuple(LOBJREL,objOID);
    if (objTuple != NULL) {
	struct large_object *objStruct;
	objStruct = (struct large_object *)GETSTRUCT(objTuple);
	
	/*variable length portion */
	return (LargeObject *) VARDATA(&objStruct->object_descriptor);
    } else {
	return NULL;
    }
}
