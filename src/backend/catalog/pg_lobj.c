/*#define LOBJASSOCDB 1*/
/* ----------------------------------------------------------------
 *   FILE
 *      pg_lobj.c
 *
 *   DESCRIPTION
 *      routines to support manipulation of the pg_large_object relation
 *
 *   INTERFACE ROUTINES
 * struct varlena *LOassocOIDandLargeObjDesc(int *objtype(out), OID oid);
 * int LOputOIDandLargeObjDesc(OID oid, int objtype,
 *                             struct varlena *desc);
 * int LOunassocOID(OID oid);
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
 * struct varlena *LOassocOIDandLargeObjDesc(int *objtype(out), OID oid);
 *    returns association if exists.
 * LOputOIDandLargeObjDesc(OID oid, int objtype,
 *                         (in) LargeObject *desc);
 *    creates association in table.
 * int LOunassocOID(OID oid);
 *    removes OID association and does a remove on the large object
 *    descriptor.
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
#include "utils/large_object.h"
#include "catalog/pg_lobj.h"
#include "utils/rel.h"
#include "access/heapam.h"
#include "utils/log.h"

static void noaction() { }

/* This is ordered upon objtype values.  Make sure that entries are sorted
   properly. */

extern void LODestroyRef ARGS((void *)); /*hackproto*/

static struct {
#define SMALL_INT 0
#define BIG 1
    int bigcookie;
    void (*LOdestroy)ARGS((void *));
} LOprocs[] = {
    /* Inversion */
    { SMALL_INT, noaction }, /* not defined yet, probably destroy class */
    /* Unix */
    { BIG, LODestroyRef }
};

/* %1
 * Certain cookies are variable length, so desc is the actual value.
 * Other cookies are OIDs, so desc is the OID wrapped in a varlena structure.
 * we don't know this, but the caller of LOassocOID... should.
 */
int LOputOIDandLargeObjDesc(objOID, objtype, desc)
     oid objOID;
     int objtype;
     struct varlena *desc;
{
    HeapTuple objTuple;
#if LOBJASSOCDB
    elog(NOTICE,"LOputOIDandLargeObjDesc(%d,%d,%d)",objOID,objtype,desc);
#endif
    objTuple = SearchSysCacheTuple(LOBJREL,objOID);
    if (objTuple != NULL) { /* return error for now */
	return -1;
    } else {
	return CreateLOBJTuple(objOID,objtype,desc);
    }
}

int CreateLOBJTuple(objOID,objtype,desc)
     oid objOID;
     int objtype;
     struct varlena *desc;
{
    Datum  values[Natts_pg_large_object];
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
    values[i++] = (Datum) objOID;
    values[i++] = (Datum) objtype;
    values[i++] = PointerGetDatum(VARDATA(desc));

    tup = heap_formtuple(Natts_pg_large_object,
			 &lobjDesc->rd_att,
			 values,
			 nulls);
    heap_insert(lobjDesc,tup,(double *)NULL);

    pfree(tup);
    heap_close(lobjDesc);

    return 0;
}

struct varlena *LOassocOIDandLargeObjDesc(objtype, objOID)
     int *objtype;
     oid objOID;
{
    HeapTuple objTuple;
#if LOBJASSOCDB
    elog(NOTICE,"LOassocOIDandLargeObjDesc(%d)",objOID);
#endif
    objTuple = SearchSysCacheTuple(LOBJREL,objOID);
    if (objTuple != NULL) {
	struct large_object *objStruct;
	objStruct = (struct large_object *)GETSTRUCT(objTuple);
	Assert(objtype != NULL);
	*objtype = (int)objStruct->objtype;
	/*
	 * See note above (%1).
	 */
	return  &objStruct->object_descriptor;
    } else {
	return NULL;
    }
}

int LOunassocOID(objOID)
     oid objOID;
{
    HeapTuple objTuple;
    Relation lobjDesc;
    objTuple = SearchSysCacheTuple(LOBJREL,objOID);
    if (objTuple != NULL) {
	struct large_object *objStruct;

	lobjDesc = heap_openr(Name_pg_large_object);
	heap_delete(lobjDesc,&objTuple->t_ctid);
	heap_close(lobjDesc);

	objStruct = (struct large_object *)GETSTRUCT(objTuple);

	/* object dependent cleanup */
        switch (LOprocs[objStruct->objtype].bigcookie) {
          case SMALL_INT:
            LOprocs[objStruct->objtype].LOdestroy((void *) *((int *)VARDATA(&objStruct->object_descriptor)));
            break;
          case BIG:
            LOprocs[objStruct->objtype].LOdestroy(&objStruct->object_descriptor);
            break;
        }


	return 0;
    } else {
	return -1;
    }
}
