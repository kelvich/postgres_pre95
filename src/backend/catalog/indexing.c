/*
 * indexing.c - This file contains routines to support indices defined
 *              on system catalogs.
 *
 * $Header$
 */

#include "tmp/postgres.h"
#include "utils/rel.h"
#include "utils/log.h"
#include "utils/oidcompos.h"
#include "access/htup.h"
#include "access/heapam.h"
#include "access/genam.h"
#include "access/attnum.h"
#include "access/ftup.h"
#include "access/funcindex.h"
#include "storage/buf.h"
#include "nodes/execnodes.h"
#include "catalog/pg_index.h"
#include "catalog/pg_proc.h"
#include "catalog/syscache.h"
#include "catalog/indexing.h"
#include "lib/index.h"

/*
 * Names of indices on the following system catalogs:
 *
 *	pg_attribute
 *	pg_proc
 *	pg_type
 *	pg_naming
 */
static NameData	AttributeNameIndexData = { "pg_attnameind" };
static NameData	AttributeNumIndexData  = { "pg_attnumind" };
static NameData	ProcedureNameIndexData = { "pg_procnameind" };
static NameData	ProcedureOidIndexData  = { "pg_procidind" };
static NameData	TypeNameIndexData      = { "pg_typenameind" };
static NameData	TypeOidIndexData       = { "pg_typeidind" };
static NameData NamingNameIndexData    = { "pg_namingind" };
static NameData NamingParentIndexData  = { "pg_namparind" };

Name	AttributeNameIndex = &AttributeNameIndexData;
Name	AttributeNumIndex  = &AttributeNumIndexData;
Name	ProcedureNameIndex = &ProcedureNameIndexData;
Name	ProcedureOidIndex  = &ProcedureOidIndexData;
Name	TypeNameIndex      = &TypeNameIndexData;
Name	TypeOidIndex       = &TypeOidIndexData;
Name	NamingNameIndex    = &NamingNameIndexData;
Name	NamingParentIndex  = &NamingParentIndexData;

char *Name_pg_attr_indices[Num_pg_attr_indices] = {AttributeNameIndexData.data,
						   AttributeNumIndexData.data};
char *Name_pg_proc_indices[Num_pg_proc_indices] = {ProcedureNameIndexData.data,
						   ProcedureOidIndexData.data};
char *Name_pg_type_indices[Num_pg_type_indices] = {TypeNameIndexData.data,
						   TypeOidIndexData.data};
char *Name_pg_name_indices[Num_pg_name_indices] = {NamingNameIndexData.data,
						   NamingParentIndexData.data};

/*
 * Changes (appends) to catalogs can (and does) happen at various places
 * throughout the code.  We need a generic routine that will open all of
 * the indices defined on a given catalog a return the relation descriptors
 * associated with them.
 */
void
CatalogOpenIndices(nIndices, names, idescs)
    int nIndices;
    char *names[];
    Relation idescs[];
{
    int i;
    NameData b;

    for (i=0; i<nIndices; i++)
    {
	/*
	 * Make AMopenr happy by giving it a Name instead of char *
	 */
	strncpy(&b.data[0], names[i], 16);

	idescs[i] = AMopenr(&b);
	Assert(idescs[i]);
    }
}

/*
 * This is the inverse routine to CatalogOpenIndices()
 */
void
CatalogCloseIndices(nIndices, idescs)
    int nIndices;
    Relation *idescs;
{
    int i;

    for (i=0; i<nIndices; i++)
	AMclose(idescs[i]);
}


/*
 * For the same reasons outlined above CatalogOpenIndices() we need a routine
 * that takes a new catalog tuple and inserts an associated index tuple into 
 * each catalog index.
 */
void
CatalogIndexInsert(idescs, nIndices, heapRelation, heapTuple)
    Relation *idescs;
    int nIndices;
    Relation heapRelation;
    HeapTuple heapTuple;
{
    HeapTuple pgIndexTup;
    TupleDescriptor heapDescriptor;
    Form_pg_index pgIndexP;
    IndexTuple newIndxTup;
    Datum datum;
    AttributeNumber natts, *attnumP;
    FuncIndexInfo finfo, *finfoP;
    char nulls[INDEX_MAX_KEYS];
    int i;

    heapDescriptor =  RelationGetTupleDescriptor(heapRelation);

    for (i=0; i<nIndices; i++)
    {
	TupleDescriptor     indexDescriptor;

	indexDescriptor = RelationGetTupleDescriptor(idescs[i]);
	pgIndexTup = (HeapTuple)SearchSysCacheTuple(INDEXRELID,
						    idescs[i]->rd_id,
						    NULL,
						    NULL,
						    NULL);
	Assert(pgIndexTup);
	pgIndexP = (Form_pg_index)GETSTRUCT(pgIndexTup);

	/*
	 * Compute the number of attributes we are indexing upon.
	 * very important - can't assume one if this is a functional
	 * index.
	 */
	for (attnumP=(&pgIndexP->indkey.data[0]), natts=0;
	     *attnumP != InvalidAttributeNumber;
	     attnumP++, natts++)
	    ;

	if (pgIndexP->indproc != InvalidObjectId)
	{
	    FIgetnArgs(&finfo) = natts;
	    natts = 1;
	    FIgetProcOid(&finfo) = pgIndexP->indproc;
	    *(FIgetname(&finfo)) = '\0';
	    finfoP = &finfo;
	}
	else
	    finfoP = (FuncIndexInfo *)NULL;

	FormIndexDatum(natts,
		       (AttributeNumber *)&pgIndexP->indkey.data[0],
		       heapTuple,
		       heapDescriptor,
		       InvalidBuffer,
		       &datum,
		       nulls,
		       finfoP);

	newIndxTup = (IndexTuple)FormIndexTuple(natts,indexDescriptor,
						&datum,nulls);
	Assert(newIndxTup);
	/*
	 * Doing this structure assignment makes me quake in my boots when I 
	 * think about portability.
	 */
	newIndxTup->t_tid = heapTuple->t_ctid;

	(void) AMinsert(idescs[i], newIndxTup, 0, 0);
   }
}

/*
 * This is needed at initialization when reldescs for some of the crucial
 * system catalogs are created and nailed into the cache.
 */
bool
CatalogHasIndex(catName, catId)
    char *catName;
    ObjectId catId;
{
    Relation    pg_relation;
    HeapScanDesc pg_relation_scan;
    HeapTuple   htup;
    Buffer	buffer;
    Form_pg_relation pgRelP;

    static ScanKeyEntryData key[1] = {
	{ 0, ObjectIdAttributeNumber, ObjectIdEqualRegProcedure }
    };

    Assert(issystem(catName));

    /*
     * If we're bootstraping we don't have pg_class (or any indices).
     */
    if (IsBootstrapProcessingMode())
	return false;

    fmgr_info(ObjectIdEqualRegProcedure, &key[0].func, &key[0].nargs);

    pg_relation = heap_openr(Name_pg_relation);
    if (! RelationIsValid(pg_relation)) {
	elog(WARN, "CatalogHasIndex: could not open RELATION relation");
    }
    key[0].argument = ObjectIdGetDatum(catId);

    pg_relation_scan =
	heap_beginscan(pg_relation, 0, NowTimeQual, 1, (ScanKey) key);

    if (! HeapScanIsValid(pg_relation_scan)) {
	heap_close(pg_relation);
	elog(WARN, "CatalogHasIndex: cannot scan RELATION relation");
    }
    htup = heap_getnext(pg_relation_scan, 0, &buffer);
    heap_endscan(pg_relation_scan);

    if (! HeapTupleIsValid(htup)) {
	heap_close(pg_relation);
	elog(NOTICE, "CatalogHasIndex: no relation with oid %d", catId);
	return false;
    }
    pgRelP = (Form_pg_relation)GETSTRUCT(htup);

    return (pgRelP->relhasindex);
}


/*
 * The remainder of the file is for individual index scan routines.  There
 * is one per cat cache.  Each index should be scanned according to how it
 * was defined during bootstrap (e.g. functional or normal) and what arguments 
 * the cache lookup requires.  Each routine returns the heap tuple that
 * qualifies.
 */


HeapTuple
AttributeNameIndexScan(heapRelation, relid, attname)
    Relation heapRelation;
    ObjectId relid;
    char *attname;
{
    Relation idesc;
    IndexScanDesc sd;
    ScanKeyEntryData skey;
    GeneralRetrieveIndexResult indexRes;
    OidChar16 keyarg;
    HeapTuple tuple;
    Buffer buffer;

    keyarg = mkoidchar16(relid, attname);
    ScanKeyEntryInitialize(&skey,
			   (bits16)0x0,
			   (AttributeNumber)1,
			   (RegProcedure)OidChar16EqRegProcedure,
			   (Datum)keyarg);

    idesc = index_openr((Name)AttributeNameIndex);
    sd = index_beginscan(idesc, false, 1, (ScanKey)&skey);

    indexRes = AMgettuple(sd, ForwardScanDirection);
    if (GeneralRetrieveIndexResultIsValid(indexRes))
    {
	ItemPointer iptr;

	iptr = GeneralRetrieveIndexResultGetHeapItemPointer(indexRes);
	tuple = heap_fetch(heapRelation, NowTimeQual, iptr, &buffer);
	pfree(indexRes);
    }
    else
	tuple = (HeapTuple)NULL;

    if (HeapTupleIsValid(tuple)) {
	tuple = palloctup(tuple, buffer, heapRelation);
	ReleaseBuffer(buffer);
      }

    index_endscan(sd);
    pfree(keyarg);

    return tuple;
}

HeapTuple
AttributeNumIndexScan(heapRelation, relid, attnum)
    Relation heapRelation;
    ObjectId relid;
    AttributeNumber attnum;
{
    Relation idesc;
    IndexScanDesc sd;
    ScanKeyEntryData skey;
    GeneralRetrieveIndexResult indexRes;
    OidInt4 keyarg;
    HeapTuple tuple;
    Buffer buffer;

    keyarg = mkoidint4(relid, (int)attnum);
    ScanKeyEntryInitialize(&skey,
			   (bits16)0x0,
			   (AttributeNumber)1,
			   (RegProcedure)OidInt4EqRegProcedure,
			   (Datum)keyarg);

    idesc = index_openr((Name)AttributeNumIndex);
    sd = index_beginscan(idesc, false, 1, (ScanKey)&skey);

    indexRes = AMgettuple(sd, ForwardScanDirection);
    if (GeneralRetrieveIndexResultIsValid(indexRes))
    {
	ItemPointer iptr;

	iptr = GeneralRetrieveIndexResultGetHeapItemPointer(indexRes);
	tuple = heap_fetch(heapRelation, NowTimeQual, iptr, &buffer);
	pfree(indexRes);
    }
    else
	tuple = (HeapTuple)NULL;

    if (HeapTupleIsValid(tuple)) {
	tuple = palloctup(tuple, buffer, heapRelation);
	ReleaseBuffer(buffer);
      }

    index_endscan(sd);
    pfree(keyarg);

    return tuple;
}

HeapTuple
ProcedureOidIndexScan(heapRelation, procId)
    Relation heapRelation;
    ObjectId procId;
{
    Relation idesc;
    IndexScanDesc sd;
    ScanKeyEntryData skey;
    GeneralRetrieveIndexResult indexRes;
    HeapTuple tuple;
    Buffer buffer;

    ScanKeyEntryInitialize(&skey,
			   (bits16)0x0,
			   (AttributeNumber)1,
			   (RegProcedure)ObjectIdEqualRegProcedure,
			   (Datum)procId);

    idesc = index_openr((Name)ProcedureOidIndex);
    sd = index_beginscan(idesc, false, 1, (ScanKey)&skey);

    indexRes = AMgettuple(sd, ForwardScanDirection);
    if (GeneralRetrieveIndexResultIsValid(indexRes))
    {
	ItemPointer iptr;

	iptr = GeneralRetrieveIndexResultGetHeapItemPointer(indexRes);
	tuple = heap_fetch(heapRelation, NowTimeQual, iptr, &buffer);
	pfree(indexRes);
    }
    else
	tuple = (HeapTuple)NULL;

    if (HeapTupleIsValid(tuple)) {
	tuple = palloctup(tuple, buffer, heapRelation);
	ReleaseBuffer(buffer);
      }

    index_endscan(sd);

    return tuple;
}

HeapTuple
ProcedureNameIndexScan(heapRelation, procName)
    Relation heapRelation;
    char     *procName;
{
    Relation idesc;
    IndexScanDesc sd;
    ScanKeyEntryData skey;
    GeneralRetrieveIndexResult indexRes;
    HeapTuple tuple;
    Buffer buffer;

    ScanKeyEntryInitialize(&skey,
			   (bits16)0x0,
			   (AttributeNumber)1,
			   (RegProcedure)NameEqualRegProcedure,
			   (Datum)procName);

    idesc = index_openr((Name)ProcedureNameIndex);
    sd = index_beginscan(idesc, false, 1, (ScanKey)&skey);

    indexRes = AMgettuple(sd, ForwardScanDirection);
    if (GeneralRetrieveIndexResultIsValid(indexRes))
    {
	ItemPointer iptr;

	iptr = GeneralRetrieveIndexResultGetHeapItemPointer(indexRes);
	tuple = heap_fetch(heapRelation, NowTimeQual, iptr, &buffer);
	pfree(indexRes);
    }
    else
	tuple = (HeapTuple)NULL;

    if (HeapTupleIsValid(tuple)) {
	tuple = palloctup(tuple, buffer, heapRelation);
	ReleaseBuffer(buffer);
      }

    index_endscan(sd);

    return tuple;
}

HeapTuple
TypeOidIndexScan(heapRelation, typeId)
    Relation heapRelation;
    ObjectId typeId;
{
    Relation idesc;
    IndexScanDesc sd;
    ScanKeyEntryData skey;
    GeneralRetrieveIndexResult indexRes;
    HeapTuple tuple;
    Buffer buffer;

    ScanKeyEntryInitialize(&skey,
			   (bits16)0x0,
			   (AttributeNumber)1,
			   (RegProcedure)ObjectIdEqualRegProcedure,
			   (Datum)typeId);

    idesc = index_openr((Name)TypeOidIndex);
    sd = index_beginscan(idesc, false, 1, (ScanKey)&skey);

    indexRes = AMgettuple(sd, ForwardScanDirection);
    if (GeneralRetrieveIndexResultIsValid(indexRes))
    {
	ItemPointer iptr;

	iptr = GeneralRetrieveIndexResultGetHeapItemPointer(indexRes);
	tuple = heap_fetch(heapRelation, NowTimeQual, iptr, &buffer);
	pfree(indexRes);
    }
    else
	tuple = (HeapTuple)NULL;

    if (HeapTupleIsValid(tuple)) {
	tuple = palloctup(tuple, buffer, heapRelation);
	ReleaseBuffer(buffer);
      }

    index_endscan(sd);

    return tuple;
}

HeapTuple
TypeNameIndexScan(heapRelation, typeName)
    Relation heapRelation;
    char     *typeName;
{
    Relation idesc;
    IndexScanDesc sd;
    ScanKeyEntryData skey;
    GeneralRetrieveIndexResult indexRes;
    HeapTuple tuple;
    Buffer buffer;

    ScanKeyEntryInitialize(&skey,
			   (bits16)0x0,
			   (AttributeNumber)1,
			   (RegProcedure)NameEqualRegProcedure,
			   (Datum)typeName);

    idesc = index_openr((Name)TypeNameIndex);
    sd = index_beginscan(idesc, false, 1, (ScanKey)&skey);

    indexRes = AMgettuple(sd, ForwardScanDirection);
    if (GeneralRetrieveIndexResultIsValid(indexRes))
    {
	ItemPointer iptr;

	iptr = GeneralRetrieveIndexResultGetHeapItemPointer(indexRes);
	tuple = heap_fetch(heapRelation, NowTimeQual, iptr, &buffer);
	pfree(indexRes);
    }
    else
	tuple = (HeapTuple)NULL;

    if (HeapTupleIsValid(tuple)) {
	tuple = palloctup(tuple, buffer, heapRelation);
	ReleaseBuffer(buffer);
      }

    index_endscan(sd);

    return tuple;
}

HeapTuple
NamingNameIndexScan(heapRelation, parentid, filename)
    Relation heapRelation;
    ObjectId parentid;
    char *filename;
{
    Relation idesc;
    IndexScanDesc sd;
    ScanKeyEntryData skey;
    GeneralRetrieveIndexResult indexRes;
    OidChar16 keyarg;
    HeapTuple tuple;
    Buffer buffer;

    keyarg = mkoidchar16(parentid, filename);
    ScanKeyEntryInitialize(&skey,
			   (bits16)0x0,
			   (AttributeNumber)1,
			   (RegProcedure)OidChar16EqRegProcedure,
			   (Datum)keyarg);

    idesc = index_openr((Name)NamingNameIndex);
    sd = index_beginscan(idesc, false, 1, (ScanKey)&skey);

    indexRes = AMgettuple(sd, ForwardScanDirection);
    if (GeneralRetrieveIndexResultIsValid(indexRes))
    {
	ItemPointer iptr;

	iptr = GeneralRetrieveIndexResultGetHeapItemPointer(indexRes);
	tuple = heap_fetch(heapRelation, NowTimeQual, iptr, &buffer);
	pfree(indexRes);
    }
    else
	tuple = (HeapTuple)NULL;

    if (HeapTupleIsValid(tuple)) {
	tuple = palloctup(tuple, buffer, heapRelation);
	ReleaseBuffer(buffer);
      }

    index_endscan(sd);
    pfree(keyarg);

    return tuple;
}
