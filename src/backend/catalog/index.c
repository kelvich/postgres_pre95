/* ----------------------------------------------------------------
 *   FILE
 *	index.c
 *	
 *   DESCRIPTION
 *	code to create and destroy POSTGRES index relations
 *
 *   INTERFACE ROUTINES
 *	index_create()		- Create a cataloged index relation
 *	index_destroy()		- Removes index relation from catalogs
 *
 *   NOTES
 *	Much of this code uses hardcoded sequential heap relation scans
 *	to fetch information from the catalogs.  These should all be
 *	rewritten to use the system caches lookup routines like
 *	SearchSysCacheTuple, which can do efficient lookup and
 *	caching.
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#include "tmp/postgres.h"

RcsId("$Header$");

#include "access/attnum.h"
#include "access/ftup.h"
#include "access/genam.h"
#include "access/heapam.h"
#include "access/itup.h"
#include "access/newam.h"
#include "access/relscan.h"
#include "access/skey.h"
#include "access/tqual.h"
#include "access/tupdesc.h"
#include "access/funcindex.h"

#include "storage/form.h"
#include "storage/smgr.h"
#include "tmp/miscadmin.h"
#include "utils/mcxt.h"
#include "utils/palloc.h"
#include "utils/rel.h"
#include "utils/log.h"

#include "catalog/catname.h"
#include "catalog/syscache.h"
#include "catalog/pg_attribute.h"
#include "catalog/pg_index.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_relation.h"
#include "catalog/pg_type.h"
#include "catalog/indexing.h"

#include "lib/heap.h"

#include "nodes/execnodes.h"
#include "nodes/plannodes.h"

#include "executor/x_qual.h"
#include "executor/x_tuples.h"
#include "executor/tuptable.h"

#include "machine.h"

extern ExprContext RMakeExprContext();

void	 index_build();

/*
 * macros used in guessing how many tuples are on a page.
 */
#define AVG_TUPLE_SIZE 8
#define NTUPLES_PER_PAGE(natts) (BLCKSZ/((natts)*AVG_TUPLE_SIZE))

/* ----------------------------------------------------------------
 *    sysatts is a structure containing attribute tuple forms
 *    for system attributes (numbered -1, -2, ...).  This really
 *    should be generated or eliminated or moved elsewhere. -cim 1/19/91
 *
 * typedef struct AttributeTupleFormData {
 *	ObjectId	attrelid;
 *	NameData	attname;
 *	ObjectId	atttypid;
 *	ObjectId	attdefrel;
 *	uint32		attnvals;
 *	ObjectId	atttyparg;	type arg for arrays/spquel/procs
 *	int16		attlen;
 *	AttributeNumber	attnum;
 *	uint16		attbound;
 *	Boolean		attbyval;
 *	Boolean		attcanindex;
 *	OID		attproc;	spquel?
 * } AttributeTupleFormData;
 *
 *    The data in this table was taken from local1_template.ami
 *    but tmin and tmax were switched because local1 was incorrect.
 * ----------------------------------------------------------------
 */
static AttributeTupleFormData	sysatts[] = {
   { 0l, "ctid",   27l,  0l, 0l, 0l,  6,  -1, 0,   '\0', '\001', 0l },
   { 0l, "lock",   31l,  0l, 0l, 0l, -1,  -2, 0,   '\0', '\001', 0l },
   { 0l, "oid",    26l,  0l, 0l, 0l,  4,  -3, 0, '\001', '\001', 0l },
   { 0l, "xmin",   28l,  0l, 0l, 0l,  5,  -4, 0,   '\0', '\001', 0l },
   { 0l, "cmin",   29l,  0l, 0l, 0l,  1,  -5, 0, '\001', '\001', 0l },
   { 0l, "xmax",   28l,  0l, 0l, 0l,  5,  -6, 0,   '\0', '\001', 0l },
   { 0l, "cmax",   29l,  0l, 0l, 0l,  1,  -7, 0, '\001', '\001', 0l },
   { 0l, "chain",  27l,  0l, 0l, 0l,  6,  -8, 0,   '\0', '\001', 0l },
   { 0l, "anchor", 27l,  0l, 0l, 0l,  6,  -9, 0,   '\0', '\001', 0l },
   { 0l, "tmin",   20l,  0l, 0l, 0l,  4, -10, 0, '\001', '\001', 0l },
   { 0l, "tmax",   20l,  0l, 0l, 0l,  4, -11, 0, '\001', '\001', 0l },
   { 0l, "vtype",  18l,  0l, 0l, 0l,  1, -12, 0, '\001', '\001', 0l },
};

/* ----------------------------------------------------------------
 *	FindIndexNAtt
 * ----------------------------------------------------------------
 */
int
FindIndexNAtt(first, indrelid, isarchival)
    int32       first;
    ObjectId    indrelid;
    Boolean     isarchival;
{
    HeapTuple               indexTuple;
    Relation                indexRelation;
    int                     nattr=0;
    static Relation         pg_index = (Relation) NULL;
    static HeapScanDesc     pg_index_scan = (HeapScanDesc) NULL;

    /*
     * XXX Can this use the syscache?
     * XXX Potential invalidation problems with use of static relation, scan.
     */
    static ScanKeyEntryData indexKey[1] = {
	{ 0, IndexHeapRelationIdAttributeNumber, ObjectIdEqualRegProcedure }
    };

	fmgr_info(ObjectIdEqualRegProcedure, &indexKey[0].func, &indexKey[0].nargs);

    /* Find an index on the given relation */
    if (first == 0) {
	if (RelationIsValid(pg_index))
	    heap_close(pg_index);
	
	if (HeapScanIsValid(pg_index))
	    heap_endscan(pg_index);
	
	indexKey[0].argument = ObjectIdGetDatum(indrelid);
	pg_index =
	    heap_openr(IndexRelationName);
	
	pg_index_scan =
	    heap_beginscan(pg_index, 0, NowTimeQual, 1, (ScanKey)indexKey);
    }

    if (! HeapScanIsValid(pg_index_scan))
	elog(WARN, "FindIndexNAtt: scan not started");

    while (HeapTupleIsValid( heap_getnext(pg_index_scan, 0, (Buffer *) NULL))) 
	nattr++;

    /* no more tuples */
    heap_endscan(pg_index_scan);
    heap_close(pg_index);
    pg_index_scan = (HeapScanDesc) NULL;
    pg_index = 	    (Relation)     NULL;
    
    return(nattr);
}

/* ----------------------------------------------------------------
 * RelationNameGetObjectId --
 *	Returns the object identifier for a relation given its name.
 *
 * >	The HASINDEX attribute for the relation with this name will
 * >	be set if it exists and if it is indicated by the call argument.
 * What a load of bull.  This setHasIndexAttribute is totally ignored.
 * This is yet another silly routine to scan the catalogs which should
 * probably be replaced by SearchSysCacheTuple. -cim 1/19/91
 *
 * Note:
 *	Assumes relation name is valid.
 *	Assumes relation descriptor is valid.
 * ----------------------------------------------------------------
 */

ObjectId
RelationNameGetObjectId(relationName, pg_relation, setHasIndexAttribute)
    Name	relationName;
    Relation	pg_relation;
    bool	setHasIndexAttribute;
{	
    HeapScanDesc	pg_relation_scan;
    HeapTuple		pg_relation_tuple;
    ObjectId		relationObjectId;
    Buffer		buffer;
    ScanKeyData		key[1];

    /*
     *  If this isn't bootstrap time, we can use the system catalogs to
     *  speed this up.
     */

    if (!IsBootstrapProcessingMode()) {
	pg_relation_tuple = ClassNameIndexScan(pg_relation, relationName);
	if (HeapTupleIsValid(pg_relation_tuple)) {
	    relationObjectId = pg_relation_tuple->t_oid;
	    pfree(pg_relation_tuple);
	} else
	    relationObjectId = InvalidObjectId;

	return (relationObjectId);
    }

    /* ----------------
     *  Bootstrap time, do this the hard way.
     *	begin a scan of pg_relation for the named relation
     * ----------------
     */
    ScanKeyEntryInitialize(&key[0].data[0], 0, RelationNameAttributeNumber,
					       Character16EqualRegProcedure,
					       NameGetDatum(relationName));

    pg_relation_scan = heap_beginscan(pg_relation, 0, NowTimeQual, 1, &key[0]);

    /* ----------------
     *	if we find the named relation, fetch its relation id
     *  (the oid of the tuple we found). 
     * ----------------
     */
    pg_relation_tuple = heap_getnext(pg_relation_scan, 0, &buffer);

    if (! HeapTupleIsValid(pg_relation_tuple)) {
	relationObjectId = InvalidObjectId;
    } else {
	relationObjectId = pg_relation_tuple->t_oid;
	ReleaseBuffer(buffer);
    }

    /* ----------------
     *	cleanup and return results
     * ----------------
     */
    heap_endscan(pg_relation_scan);

    return
	relationObjectId;
}


/* ----------------------------------------------------------------
 *	GetHeapRelationOid
 * ----------------------------------------------------------------
 */
GetHeapRelationOid(heapRelationName, indexRelationName)
    Name	heapRelationName;
    Name	indexRelationName;
{
    Relation	pg_relation;
    ObjectId 	indoid;
    ObjectId 	heapoid;

    /* ----------------
     *	XXX ADD INDEXING HERE
     * ----------------
     */
    /* ----------------
     *	open pg_relation and get the oid of the relation
     *  corresponding to the name of the index relation.
     * ----------------
     */
    pg_relation = heap_openr(RelationRelationName);

    indoid = RelationNameGetObjectId(indexRelationName,
				     pg_relation,
				     false);
      
    if (ObjectIdIsValid(indoid))
	elog(WARN, "Cannot create index: '%s' already exists",
		    indexRelationName);

    /* ----------------
     *	get the object id of the heap relation
     * ----------------
     */
    heapoid = RelationNameGetObjectId(heapRelationName,
				      pg_relation,
				      true);
 
    /* ----------------
     *    check that the heap relation exists..
     * ----------------
     */
    if (! ObjectIdIsValid(heapoid))
	elog(WARN, "Cannot create index on '%s': relation does not exist",
		    heapRelationName);

    /* ----------------
     *    close pg_relation and return the heap relation oid
     * ----------------
     */
    heap_close(pg_relation);

    return heapoid;
}

#define AFSIZE 	sizeof(AttributeTupleFormData)
    
TupleDescriptor
BuildFuncTupleDesc(funcInfo)
    FuncIndexInfo 	*funcInfo;
{
    HeapTuple 		tuple, SearchSysCacheTuple();
    TupleDescriptor 	funcTupDesc;
    ObjectId  		retType;
    int4 		nArgs;

    /*
     * Allocate and zero a tuple descriptor.
     */
    funcTupDesc = (TupleDescriptor) palloc(sizeof(*funcTupDesc));
    funcTupDesc->data[0] = (AttributeTupleForm) palloc(AFSIZE);
    bzero(funcTupDesc->data[0], AFSIZE);

    /*
     * Lookup the function for the return type and number of args.
     */
    tuple = SearchSysCacheTuple(PRONAME,FIgetname(funcInfo),0,0,0);
    if (!HeapTupleIsValid(tuple))
	elog(WARN, "Function name %s does not exist", FIgetname(funcInfo));

    retType = ((Form_pg_proc)GETSTRUCT(tuple))->prorettype;
    nArgs = ((Form_pg_proc)GETSTRUCT(tuple))->pronargs;

    /*
     * verify arg count
     */
    if (nArgs != FIgetnArgs(funcInfo))
    {
	elog(WARN, 
	     "Defined functional index with an incorrect number of arguments");
    }

    /*
     * Look up the return type in pg_type for the type length.
     */
    tuple = SearchSysCacheTuple(TYPOID,retType,0,0,0);
    if (!HeapTupleIsValid(tuple))
	elog(WARN,"Function %s return type does not exist",FIgetname(funcInfo));

    /*
     * Assign some of the attributes values. Leave the rest as 0.
     */
    funcTupDesc->data[0]->attlen = ((Form_pg_type)GETSTRUCT(tuple))->typlen;
    funcTupDesc->data[0]->atttypid = retType;
    funcTupDesc->data[0]->attnum = 1;
    funcTupDesc->data[0]->attbyval = ((Form_pg_type)GETSTRUCT(tuple))->typbyval;
    funcTupDesc->data[0]->attcanindex = 'f';

    /*
     * make the attributes name the same as the functions
     */
    strncpy(&funcTupDesc->data[0]->attname, 
	    FIgetname(funcInfo), 
	    sizeof(funcTupDesc->data[0]->attname));

    return (funcTupDesc);
}

/* ----------------------------------------------------------------
 *	ConstructTupleDescriptor
 * ----------------------------------------------------------------
 */

TupleDescriptor
ConstructTupleDescriptor(heapoid, heapRelation, numatts, attNums)
    ObjectId		heapoid;
    Relation		heapRelation;
    AttributeNumber	numatts;
    AttributeNumber	attNums[];
{
    TupleDescriptor 	heapTupDesc;
    TupleDescriptor 	indexTupDesc;
    AttributeNumber 	atnum;		/* attributeNumber[attributeOffset] */
    AttributeNumber 	atind;
    AttributeNumber 	natts;		/* RelationTupleForm->relnatts */
    char 		*from;		/* used to simplify memcpy below */
    char 		*to;		/* used to simplify memcpy below */
    AttributeOffset	i;

    /* ----------------
     *	allocate the new tuple descriptor
     * ----------------
     */
    indexTupDesc = (TupleDescriptor)
	palloc(numatts * sizeof(*indexTupDesc));

    /* ----------------
     *	
     * ----------------
     */
    natts = RelationGetRelationTupleForm(heapRelation)->relnatts;

    /* ----------------
     *    for each attribute we are indexing, obtain its attribute
     *    tuple form from either the static table of system attribute
     *    tuple forms or the relation tuple descriptor
     * ----------------
     */
    for (i = 0; i < numatts; i += 1) {

	/* ----------------
	 *   get the attribute number and make sure it's valid
	 * ----------------
	 */
	atnum = attNums[i];
	if (atnum > natts)
	    elog(WARN, "Cannot create index: attribute %d does not exist",
			atnum);
	
	indexTupDesc->data[i] = (AttributeTupleForm) palloc(AFSIZE);

	/* ----------------
	 *   determine which tuple descriptor to copy
	 * ----------------
	 */
	if (! AttributeNumberIsForUserDefinedAttribute(atnum)) {

	    /* ----------------
	     *    here we are indexing on a system attribute (-1...-12)
	     *    so we convert atnum into a usable index 0...11 so we can
	     *    use it to dereference the array sysatts[] which stores
	     *    tuple descriptor information for system attributes.
	     * ----------------
	     */
	    if (atnum <= FirstLowInvalidHeapAttributeNumber || atnum >= 0 )
		elog(WARN, "Cannot create index on system attribute: attribute number out of range (%d)", atnum);
	    atind = (-atnum) - 1;

	    from = (char *) (& sysatts[atind]);

	} else {
	    /* ----------------
	     *    here we are indexing on a normal attribute (1...n)
	     * ----------------
	     */

	    heapTupDesc = RelationGetTupleDescriptor(heapRelation);
	    atind = 	  AttributeNumberGetAttributeOffset(atnum);

	    from = (char *) (heapTupDesc->data[ atind ]);
	}

	/* ----------------
	 *   now that we've determined the "from", let's copy
	 *   the tuple desc data...
	 * ----------------
	 */

	to =   (char *) (indexTupDesc->data[ i ]);
	memcpy(to, from, AFSIZE);
	       
	/* ----------------
	 *    now we have to drop in the proper relation descriptor
	 *    into the copied tuple form's attrelid and we should be
	 *    all set.
	 * ----------------
	 */
	((AttributeTupleForm) to)->attrelid = heapoid;
    }

    return indexTupDesc;
}

/* ----------------------------------------------------------------
 * AccessMethodObjectIdGetAccessMethodTupleForm --
 *	Returns the formated access method tuple given its object identifier.
 *
 * XXX ADD INDEXING
 *
 * Note:
 *	Assumes object identifier is valid.
 * ----------------------------------------------------------------
 */
AccessMethodTupleForm
AccessMethodObjectIdGetAccessMethodTupleForm(accessMethodObjectId)
    ObjectId	accessMethodObjectId;

{
    Relation		pg_am_desc;
    HeapScanDesc	pg_am_scan;
    HeapTuple		pg_am_tuple;
    ScanKeyData		key[1];
    AccessMethodTupleForm form;

    /* ----------------
     *	form a scan key for the pg_am relation
     * ----------------
     */
    ScanKeyEntryInitialize(&key[0].data[0], 0, ObjectIdAttributeNumber,
			   ObjectIdEqualRegProcedure,
			   ObjectIdGetDatum(accessMethodObjectId));

    /* ----------------
     *	fetch the desired access method tuple
     * ----------------
     */
    pg_am_desc = heap_openr(AccessMethodRelationName);
    pg_am_scan = heap_beginscan(pg_am_desc, 0, NowTimeQual, 1, &key[0]);

    pg_am_tuple = heap_getnext(pg_am_scan, 0, (Buffer *)NULL);

    /* ----------------
     *	return NULL if not found
     * ----------------
     */
    if (! HeapTupleIsValid(pg_am_tuple)) {
	heap_endscan(pg_am_scan);
	heap_close(pg_am_desc);
	return (NULL);
    }

    /* ----------------
     *	if found am tuple, then copy the form and return the copy
     * ----------------
     */
    form = (AccessMethodTupleForm)
	palloc(sizeof *form);
    memcpy(form, getstruct(pg_am_tuple), sizeof *form);

    heap_endscan(pg_am_scan);
    heap_close(pg_am_desc);

    return (form);
}

/* ----------------------------------------------------------------
 *	ConstructIndexReldesc
 * ----------------------------------------------------------------
 */
void
ConstructIndexReldesc(indexRelation, amoid)
    Relation		indexRelation;
    ObjectId		amoid;
{
    extern GlobalMemory  CacheCxt;
    MemoryContext   	 oldcxt;

    /* ----------------
     *    here we make certain to allocate the access method
     *    tuple within the cache context lest it vanish when the
     *    context changes
     * ----------------
     */
    if (!CacheCxt)
	CacheCxt = CreateGlobalMemory("Cache");

    oldcxt = MemoryContextSwitchTo((MemoryContext)CacheCxt);
   
    indexRelation->rd_am =
	AccessMethodObjectIdGetAccessMethodTupleForm(amoid);
   
    MemoryContextSwitchTo(oldcxt);

    /* ----------------
     *   XXX missing the initialization of some other fields 
     * ----------------
     */
        
    indexRelation->rd_rel->relowner = 	GetUserId();
    
    indexRelation->rd_rel->relam = 		amoid;
    indexRelation->rd_rel->reltuples = 		1;		/* XXX */
    indexRelation->rd_rel->relexpires = 	0;		/* XXX */
    indexRelation->rd_rel->relpreserved = 	0;		/* XXX */
    indexRelation->rd_rel->relkind = 		'i';
    indexRelation->rd_rel->relarch = 		'n';		/* XXX */
}
    
/* ----------------------------------------------------------------
 *	UpdateRelationRelation
 * ----------------------------------------------------------------
 */
ObjectId
UpdateRelationRelation(indexRelation)
    Relation	indexRelation;
{
    Relation	pg_relation;
    HeapTuple	tuple;
    ObjectId	tupleOid;
    Relation	idescs[Num_pg_class_indices];
    
    pg_relation = heap_openr(RelationRelationName);
   
    tuple = addtupleheader(RelationRelationNumberOfAttributes,
			   sizeof(*indexRelation->rd_rel),
			   (char *) indexRelation->rd_rel);

    /* ----------------
     *  the new tuple must have the same oid as the relcache entry for the
     *  index.  sure would be embarassing to do this sort of thing in polite
     *  company.
     * ----------------
     */
    tuple->t_oid = indexRelation->rd_id;
    heap_insert(pg_relation, tuple, (double *)NULL);
    
    /*
     *  During normal processing, we need to make sure that the system
     *  catalog indices are correct.  Bootstrap (initdb) time doesn't
     *  require this, because we make sure that the indices are correct
     *  just before exiting.
     */

    if (!IsBootstrapProcessingMode()) {
	CatalogOpenIndices(Num_pg_class_indices, Name_pg_class_indices, idescs);
	CatalogIndexInsert(idescs, Num_pg_class_indices, pg_relation, tuple);
	CatalogCloseIndices(Num_pg_class_indices, idescs);
    }

    tupleOid = tuple->t_oid;
    pfree((Pointer)tuple);
    heap_close(pg_relation);

    return(tupleOid);
}

/* ----------------------------------------------------------------
 *	InitializeAttributeOids
 * ----------------------------------------------------------------
 */
void
InitializeAttributeOids(indexRelation, numatts, indexoid)
    Relation		indexRelation;
    AttributeNumber	numatts;
    ObjectId		indexoid;
{
    TupleDescriptor	tupleDescriptor;
    AttributeOffset	i;
      
    tupleDescriptor = RelationGetTupleDescriptor(indexRelation);
      
    for (i = 0; i < numatts; i += 1)
	tupleDescriptor->data[i]->attrelid = indexoid;
}

/* ----------------------------------------------------------------
 *	AppendAttributeTuples
 *
 *    	XXX For now, only change the ATTNUM attribute value
 * ----------------------------------------------------------------
 */
void
AppendAttributeTuples(indexRelation, numatts)
    Relation		indexRelation;
    AttributeNumber	numatts;
{
    Relation		pg_attribute;
    HeapTuple		tuple;
    HeapTuple		newtuple;
    bool		hasind;
    Relation		idescs[Num_pg_attr_indices];
    
    Datum		value[ AttributeRelationNumberOfAttributes ];
    char		nullv[ AttributeRelationNumberOfAttributes ];
    char		replace[ AttributeRelationNumberOfAttributes ];

    TupleDescriptor	indexTupDesc;
    AttributeOffset	i;
    
    /* ----------------
     *	open the attribute relation
     *  XXX ADD INDEXING
     * ----------------
     */
    pg_attribute = heap_openr(AttributeRelationName);
   
    /* ----------------
     *	initialize null[], replace[] and value[]
     * ----------------
     */
    (void) memset(nullv, ' ',    AttributeRelationNumberOfAttributes);
    (void) memset(replace, ' ', AttributeRelationNumberOfAttributes);

    /* ----------------
     *  create the first attribute tuple.
     *	XXX For now, only change the ATTNUM attribute value
     * ----------------
     */
    replace[ AttributeNumberAttributeNumber - 1 ] = 'r';

    value[ AttributeNumberAttributeNumber - 1 ] = Int16GetDatum(1);

    tuple = addtupleheader(AttributeRelationNumberOfAttributes,
			   sizeof *indexRelation->rd_att.data[0],
			   (char *)indexRelation->rd_att.data[0]);

    hasind = false;
    if (!IsBootstrapProcessingMode() && pg_attribute->rd_rel->relhasindex) {
	hasind = true;
	CatalogOpenIndices(Num_pg_attr_indices, Name_pg_attr_indices, idescs);
    }

    /* ----------------
     *  insert the first attribute tuple.
     * ----------------
     */
    tuple = ModifyHeapTuple(tuple,
			    InvalidBuffer,
			    pg_attribute,
			    value,
			    nullv,
			    replace);
    
    heap_insert(pg_attribute, tuple, (double *)NULL);
    if (hasind)
	CatalogIndexInsert(idescs, Num_pg_attr_indices, pg_attribute, tuple);

    /* ----------------
     *	now we use the information in the index tuple
     *  descriptor to form the remaining attribute tuples.
     * ----------------
     */
    indexTupDesc = RelationGetTupleDescriptor(indexRelation);
      
    for (i = 1; i < numatts; i += 1) {
	/* ----------------
	 *  process the remaining attributes...
	 * ----------------
	 */
	memcpy(getstruct(tuple), 		/* to */
	       (char *)indexTupDesc->data[i],   /* from */
	       sizeof (AttributeTupleForm));    /* size */
	 
	value[ AttributeNumberAttributeNumber - 1 ] = Int16GetDatum(i + 1);

	newtuple = ModifyHeapTuple(tuple,
				   InvalidBuffer,
				   pg_attribute,
				   value,
				   nullv,
				   replace);
	 
	heap_insert(pg_attribute, newtuple, (double *)NULL);
	if (hasind)
	    CatalogIndexInsert(idescs, Num_pg_attr_indices, pg_attribute, newtuple);

	/* ----------------
	 *  ModifyHeapTuple returns a new copy of a tuple
	 *  so we free the original and use the copy..
	 * ----------------
	 */
	pfree((Pointer)tuple);
	tuple = newtuple;
    }

    /* ----------------
     *	close the attribute relation and free the tuple
     * ----------------
     */
    heap_close(pg_attribute);

    if (hasind)
	CatalogCloseIndices(Num_pg_attr_indices, idescs);

    pfree((Pointer)tuple);
}

/* ----------------------------------------------------------------
 *	UpdateIndexRelation
 * ----------------------------------------------------------------
 */
void
UpdateIndexRelation(indexoid, heapoid, funcInfo, natts, attNums, classOids)
    ObjectId		indexoid;
    ObjectId		heapoid;
    FuncIndexInfo	*funcInfo;
    AttributeNumber	natts;
    AttributeNumber	attNums[];
    ObjectId		classOids[];
{
    IndexTupleFormData	indexForm;
    Relation		pg_index;
    HeapTuple		tuple;
    AttributeOffset	i;
    
    /* ----------------
     *	store the oid information into the index tuple form
     * ----------------
     */
    indexForm.indrelid =   heapoid;
    indexForm.indexrelid = indexoid;
    indexForm.indproc = (PointerIsValid(funcInfo)) ?
	FIgetProcOid(funcInfo) : InvalidObjectId;
   
    memset((char *)& indexForm.indkey[0], 0, sizeof indexForm.indkey);
    memset((char *)& indexForm.indclass[0], 0, sizeof indexForm.indclass);

    /* ----------------
     *	copy index key and op class information
     * ----------------
     */
    for (i = 0; i < natts; i += 1) {
	indexForm.indkey[i] =   attNums[i];
	indexForm.indclass[i] = classOids[i];
    }
    /*
     * If we have a functional index, add all attribute arguments
     */
    if (PointerIsValid(funcInfo))
    {
	for (i=1; i < FIgetnArgs(funcInfo); i++)
	    indexForm.indkey[i] =   attNums[i];
    }
   
    indexForm.indisclustered = '\0';		/* XXX constant */
    indexForm.indisarchived = '\0';		/* XXX constant */

    /* ----------------
     *	open the system catalog index relation
     * ----------------
     */
    pg_index = heap_openr(IndexRelationName);
    
    /* ----------------
     *	form a tuple to insert into pg_index
     * ----------------
     */
    tuple = addtupleheader(IndexRelationNumberOfAttributes,
			   sizeof(indexForm),
			   (char *)&indexForm);

    /* ----------------
     *	insert the tuple into the pg_index
     *  XXX ADD INDEX TUPLES TOO
     * ----------------
     */
    heap_insert(pg_index, tuple, (double *)NULL);

    /* ----------------
     *	close the relation and free the tuple
     * ----------------
     */
    heap_close(pg_index);
    pfree((Pointer)tuple);
}
 
/* ----------------------------------------------------------------
 *	InitIndexStrategy
 * ----------------------------------------------------------------
 */
void
InitIndexStrategy(numatts, indexRelation, accessMethodObjectId)
    AttributeNumber	numatts;
    Relation		indexRelation;
    ObjectId		accessMethodObjectId;
{
    IndexStrategy	strategy;
    RegProcedure	*support;
    uint16		amstrategies;
    uint16		amsupport;
    ObjectId		attrelid;
    Size 		strsize;
    extern GlobalMemory CacheCxt;

    /* ----------------
     *	get information from the index relation descriptor
     * ----------------
     */
    attrelid = 	   indexRelation->rd_att.data[0]->attrelid;
    amstrategies = indexRelation->rd_am->amstrategies;
    amsupport =    indexRelation->rd_am->amsupport;

    /* ----------------
     *	get the size of the strategy
     * ----------------
     */
    strsize =  AttributeNumberGetIndexStrategySize(numatts, amstrategies);
    
    /* ----------------
     *  allocate the new index strategy structure
     *
     *	the index strategy has to be allocated in the same
     *	context as the relation descriptor cache or else
     *	it will be lost at the end of the transaction.
     * ----------------
     */
    if (!CacheCxt)
	CacheCxt = CreateGlobalMemory("Cache");

    strategy = (IndexStrategy)
	MemoryContextAlloc((MemoryContext)CacheCxt, strsize);

    if (amsupport > 0) {
        strsize = numatts * (amsupport * sizeof(RegProcedure));
        support = (RegProcedure *) MemoryContextAlloc((MemoryContext)CacheCxt,
						      strsize);
    } else {
	support = (RegProcedure *) NULL;
    }

    /* ----------------
     *	fill in the index strategy structure with information
     *  from the catalogs.  Note: we use heap override mode
     *  in order to be allowed to see the correct information in the
     *  catalogs, even though our transaction has not yet committed.
     * ----------------
     */
    setheapoverride(1);

    IndexSupportInitialize(strategy, support,
			    attrelid, accessMethodObjectId,
			    amstrategies, amsupport, numatts);

    setheapoverride(0);

    /* ----------------
     *	store the strategy information in the index reldesc
     * ----------------
     */
    RelationSetIndexSupport(indexRelation, strategy, support);
}


/* ----------------------------------------------------------------
 *    	index_create
 * ----------------------------------------------------------------
 */
void
index_create(heapRelationName, indexRelationName, funcInfo,
	     accessMethodObjectId, numatts, attNums,
	     classObjectId, parameterCount, parameter, predicate)
    Name		heapRelationName;
    Name		indexRelationName;
    FuncIndexInfo	*funcInfo;
    ObjectId		accessMethodObjectId;
    AttributeNumber	numatts;
    AttributeNumber	attNums[];
    ObjectId		classObjectId[];
    uint16		parameterCount;
    Datum		parameter[];
    LispValue		predicate;
{
    Relation		 heapRelation;
    Relation		 indexRelation;
    HeapTuple		 tuple;
    TupleDescriptor	 indexTupDesc;
    IndexStrategy	 strategy;
    AttributeOffset	 attributeOffset;
    ObjectId		 heapoid;
    ObjectId		 indexoid;
    ObjectId		 indproc;

    extern GlobalMemory  CacheCxt;
    MemoryContext   	 oldcxt;
    
    /* ----------------
     *	check parameters
     * ----------------
     */
    if (numatts < 1)
	elog(WARN, "must index at least one attribute");

    /* ----------------
     *    get heap relation oid and open the heap relation
     *	  XXX ADD INDEXING
     * ----------------
     */
    heapoid = GetHeapRelationOid(heapRelationName, indexRelationName);
   
    heapRelation = heap_open(heapoid);

    /* ----------------
     * write lock heap to guarantee exclusive access
     * ---------------- 
     */

    RelationSetLockForWrite(heapRelation);

    /* ----------------
     *    construct new tuple descriptor
     * ----------------
     */
    if (PointerIsValid(funcInfo))
    	indexTupDesc = BuildFuncTupleDesc(funcInfo);
    else
	indexTupDesc = ConstructTupleDescriptor(heapoid,
						heapRelation,
						numatts,
						attNums);

    /* ----------------
     *	create the index relation
     * ----------------
     */
    indexRelation = heap_creatr((char*)indexRelationName,
				numatts,
				DEFAULT_SMGR,
				(struct attribute **)(indexTupDesc));

    /* ----------------
     *    construct the index relation descriptor
     *
     *    XXX should have a proper way to create cataloged relations
     * ----------------
     */
    ConstructIndexReldesc(indexRelation, accessMethodObjectId);

    /* ----------------
     *    add index to catalogs
     *    (append RELATION tuple)
     * ----------------
     */
    indexoid = UpdateRelationRelation(indexRelation);

    /* ----------------
     * Now get the index procedure (only relevant for functional indices).
     * ----------------
     */

    if (PointerIsValid(funcInfo))
    {
	HeapTuple proc_tup, SearchSysCacheTuple();
	
	proc_tup = SearchSysCacheTuple(PRONAME,FIgetname(funcInfo),0,0,0);

	if (!HeapTupleIsValid(proc_tup))
	     elog (WARN, "function named %s does not exist",
		   FIgetname(funcInfo));
	FIgetProcOid(funcInfo) = proc_tup->t_oid;
    }

    /* ----------------
     *	now update the object id's of all the attribute
     *  tuple forms in the index relation's tuple descriptor
     * ----------------
     */
    InitializeAttributeOids(indexRelation, numatts, indexoid);
  
    /* ----------------
     *    append ATTRIBUTE tuples
     * ----------------
     */
    AppendAttributeTuples(indexRelation, numatts);

    /* ----------------
     *    update pg_index
     *    (append INDEX tuple)
     *
     *    Should stow away a representation of "predicate" here(?)
     *    (Or, could define a rule to maintain the predicate) --Nels, Feb '92
     * ----------------
     */
    UpdateIndexRelation(indexoid, heapoid, funcInfo,
			numatts, attNums, classObjectId);

    /* ----------------
     *    initialize the index strategy
     * ----------------
     */
    InitIndexStrategy(numatts, indexRelation, accessMethodObjectId);

    /*
     *  If this is bootstrap (initdb) time, then we don't actually
     *  fill in the index yet.  We'll be creating more indices and classes
     *  later, so we delay filling them in until just before we're done
     *  with bootstrapping.  Otherwise, we call the routine that constructs
     *  the index.  The heap and index relations are closed by index_build().
     */
    if (IsBootstrapProcessingMode()) {
	index_register(heapRelationName, indexRelationName, numatts, attNums,
			parameterCount, parameter, funcInfo, predicate);
    } else {
	heapRelation = heap_openr(heapRelationName);
	index_build(heapRelation, indexRelation, numatts, attNums,
			parameterCount, parameter, funcInfo, predicate);
    }
}

/* ----------------------------------------------------------------
 *	index_destroy
 *
 *	XXX break into modules like index_create
 * ----------------------------------------------------------------
 */
void
index_destroy(indexId)
    ObjectId	indexId;
{
    Relation		indexRelation;
    Relation		catalogRelation;
    HeapTuple		tuple;
    HeapScanDesc	scan;
    ScanKeyEntryData	entry[1];
    char		*relpath();

    Assert(ObjectIdIsValid(indexId));

    indexRelation = ObjectIdOpenIndexRelation(indexId);

    /* ----------------
     * fix RELATION relation
     * ----------------
     */
    catalogRelation = heap_openr(RelationRelationName);

	ScanKeyEntryInitialize(&entry[0], 0x0, ObjectIdAttributeNumber, 
						   ObjectIdEqualRegProcedure, 
						   ObjectIdGetDatum(indexId));;

    scan = heap_beginscan(catalogRelation, 0, NowTimeQual, 1, entry);
    tuple = heap_getnext(scan, 0, (Buffer *)NULL);

    AssertState(HeapTupleIsValid(tuple));

    heap_delete(catalogRelation, &tuple->t_ctid);
    heap_endscan(scan);
    heap_close(catalogRelation);

    /* ----------------
     * fix ATTRIBUTE relation
     * ----------------
     */
    catalogRelation = heap_openr(AttributeRelationName);

    entry[0].attributeNumber = AttributeRelationIdAttributeNumber;

    scan = heap_beginscan(catalogRelation, 0, NowTimeQual, 1, entry);
	
    while (tuple = heap_getnext(scan, 0, (Buffer *)NULL),
	   HeapTupleIsValid(tuple)) {

	heap_delete(catalogRelation, &tuple->t_ctid);
    }
    heap_endscan(scan);
    heap_close(catalogRelation);

    /* ----------------
     * fix INDEX relation
     * ----------------
     */
    catalogRelation = heap_openr(IndexRelationName);

    entry[0].attributeNumber = IndexRelationIdAttributeNumber;

    scan = heap_beginscan(catalogRelation, 0, NowTimeQual, 1,  entry);
    tuple = heap_getnext(scan, 0, (Buffer *)NULL);
    if (! HeapTupleIsValid(tuple)) {
	elog(NOTICE, "IndexRelationDestroy: %s's INDEX tuple missing",
	     RelationGetRelationName(indexRelation));
    }
    heap_delete(catalogRelation, &tuple->t_ctid);
    heap_endscan(scan);
    heap_close(catalogRelation);

    /*
     * physically remove the file
     */
    if (FileNameUnlink(relpath(&indexRelation->rd_rel->relname)) < 0)
	elog(WARN, "amdestroyr: unlink: %m");
	
    index_close(indexRelation);
}

/* ----------------------------------------------------------------
 *			index_build support
 * ----------------------------------------------------------------
 */
/* ----------------
 *	FormIndexDatum
 * ----------------
 */
void
FormIndexDatum(numberOfAttributes, attributeNumber, heapTuple, heapDescriptor,
	       buffer, datum, nullv, fInfo)
    AttributeNumber	numberOfAttributes;
    AttributeNumber	attributeNumber[];
    HeapTuple		heapTuple;
    TupleDescriptor	heapDescriptor;
    Buffer		buffer;
    Datum		*datum;
    char		*nullv;
    FuncIndexInfoPtr	fInfo;
{
    AttributeNumber	i;
    AttributeOffset	offset;
    Boolean		isNull;

    /* ----------------
     *	for each attribute we need from the heap tuple,
     *  get the attribute and stick it into the datum and
     *  null arrays.
     * ----------------
     */

    for (i = 1; i <= numberOfAttributes; i += 1) {
	offset = AttributeNumberGetAttributeOffset(i);

	datum[ offset ] =
	    PointerGetDatum( GetIndexValue(heapTuple,
					   heapDescriptor,
					   offset,
					   attributeNumber,
					   fInfo,
					   &isNull,
					   buffer) );

	nullv[ offset ] = (isNull) ? 'n' : ' ';
    }
}


/* ----------------
 *	UpdateStats
 * ----------------
 */
UpdateStats(relid, reltuples, hasindex)
    ObjectId	relid;
    long 	reltuples;
    bool	hasindex;
{
    Relation 	whichRel;
    Relation 	pg_relation;
    HeapScanDesc pg_relation_scan;
    HeapTuple 	htup;
    HeapTuple 	newtup;
    long 	relpages;
    long 	*val;
    Boolean 	isNull = '\0';
    Buffer 	buffer;
    int 	i;
    Relation	idescs[Num_pg_class_indices];
    
    static ScanKeyEntryData key[1] = {
	{ 0, ObjectIdAttributeNumber, ObjectIdEqualRegProcedure }
    };
    Datum 	values[Natts_pg_relation];
    char 	nulls[Natts_pg_relation];
    char 	replace[Natts_pg_relation];

    fmgr_info(ObjectIdEqualRegProcedure, &key[0].func, &key[0].nargs);

    /* ----------------
     * This routine handles updates for both the heap and index relation
     * statistics.  In order to guarantee that we're able to *see* the index
     * relation tuple, we bump the command counter id here.  The index
     * relation tuple was created in the current transaction.
     * ----------------
     */
    CommandCounterIncrement();

    /* ----------------
     * CommandCounterIncrement() flushes invalid cache entries, including
     * those for the heap and index relations for which we're updating
     * statistics.  Now that the cache is flushed, it's safe to open the
     * relation again.  We need the relation open in order to figure out
     * how many blocks it contains.
     * ----------------
     */

    whichRel = (Relation) RelationIdGetRelation(relid);

    if (!RelationIsValid(whichRel))
	elog(WARN, "UpdateStats: cannot open relation id %d", relid);

    /* ----------------
     * Find the RELATION relation tuple for the given relation.
     * ----------------
     */
    pg_relation = heap_openr(Name_pg_relation);
    if (! RelationIsValid(pg_relation)) {
	elog(WARN, "UpdateStats: could not open RELATION relation");
    }
    key[0].argument = ObjectIdGetDatum(relid);

    pg_relation_scan =
	heap_beginscan(pg_relation, 0, NowTimeQual, 1, (ScanKey) key);
    
    if (! HeapScanIsValid(pg_relation_scan)) {
	heap_close(pg_relation);
	elog(WARN, "UpdateStats: cannot scan RELATION relation");
    }

    /* if the heap_open above succeeded, then so will this heap_getnext() */
    htup = heap_getnext(pg_relation_scan, 0, &buffer);
    heap_endscan(pg_relation_scan);

    /* ----------------
     *	update statistics
     * ----------------
     */
    relpages = RelationGetNumberOfBlocks(whichRel);

    /*
     *  We shouldn't have to do this, but we do...  Modify the reldesc
     *  in place with the new values so that the cache contains the
     *  latest copy.
     */

    whichRel->rd_rel->relhasindex = hasindex;
    whichRel->rd_rel->relpages = relpages;
    whichRel->rd_rel->reltuples = reltuples;

    for (i = 0; i < Natts_pg_relation; i++) {
	nulls[i] = ' ';
	replace[i] = ' ';
	values[i] = (Datum) NULL;
    }

    replace[Anum_pg_relation_relpages - 1] = 'r';
    values[Anum_pg_relation_relpages - 1] = (Datum)relpages;

    /*
     * If reltuples wasn't supplied take an educated guess.
     */
    if (reltuples == 0)
	reltuples = relpages*NTUPLES_PER_PAGE(whichRel->rd_rel->relnatts);

    replace[Anum_pg_relation_reltuples - 1] = 'r';
    values[Anum_pg_relation_reltuples - 1] = (Datum)reltuples;
    replace[Anum_pg_relation_relhasindex - 1] = 'r';
    values[Anum_pg_relation_relhasindex - 1] = CharGetDatum(hasindex);

    newtup = ModifyHeapTuple(htup, buffer, pg_relation, values,
			     nulls, replace);

    if (IsBootstrapProcessingMode())
	bcopy(newtup, htup, PSIZE(newtup));
    else {
	heap_replace(pg_relation, &(newtup->t_ctid), newtup);
	CatalogOpenIndices(Num_pg_class_indices, Name_pg_class_indices, idescs);
	CatalogIndexInsert(idescs, Num_pg_class_indices, pg_relation, newtup);
	CatalogCloseIndices(Num_pg_class_indices, idescs);
    }

    heap_close(pg_relation);
    heap_close(whichRel);
}


/* -------------------------
 *	FillDummyExprContext
 *	    Sets up dummy ExprContext and TupleTableSlot objects for use
 *	    with ExecQual.
 * -------------------------
 */
void
FillDummyExprContext(econtext, slot, tupdesc, buffer)
    ExprContext econtext;
    TupleTableSlot slot;
    TupleDescriptor tupdesc;
    Buffer buffer;
{
    set_ecxt_scantuple(econtext, slot);
    set_ecxt_innertuple(econtext, NULL);
    set_ecxt_outertuple(econtext, NULL);
    set_ecxt_param_list_info(econtext, NULL);
    set_ecxt_range_table(econtext, NULL);

    SetSlotTupleDescriptor(slot, tupdesc);
    SetSlotBuffer(slot, buffer);
    SetSlotShouldFree(slot, false);
}


/* ----------------
 *	DefaultBuild
 * ----------------
 */
void
DefaultBuild(heapRelation, indexRelation, numberOfAttributes, attributeNumber,
		indexStrategy, parameterCount, parameter, funcInfo, predicate)
    Relation		heapRelation;
    Relation		indexRelation;
    AttributeNumber	numberOfAttributes;
    AttributeNumber	attributeNumber[];
    IndexStrategy	indexStrategy; 		/* not used */
    uint16		parameterCount;		/* not used */
    Datum		parameter[]; 		/* not used */
    FuncIndexInfoPtr       funcInfo;
    LispValue		predicate;
{
    HeapScanDesc		scan;
    HeapTuple			heapTuple;
    Buffer			buffer;

    IndexTuple			indexTuple;
    TupleDescriptor		heapDescriptor;
    TupleDescriptor		indexDescriptor;
    Datum			*datum;
    char			*nullv;
    long			reltuples, indtuples;
    ExprContext			econtext;
    TupleTable			tupleTable;
    TupleTableSlot		slot;

    GeneralInsertIndexResult	insertResult;

    /* ----------------
     *	more & better checking is needed
     * ----------------
     */
    Assert(ObjectIdIsValid(indexRelation->rd_rel->relam));	/* XXX */

    /* ----------------
     *	get the tuple descriptors from the relations so we know
     *  how to form the index tuples..
     * ----------------
     */
    heapDescriptor =  RelationGetTupleDescriptor(heapRelation);
    indexDescriptor = RelationGetTupleDescriptor(indexRelation);

    /* ----------------
     *	datum and null are arrays in which we collect the index attributes
     *  when forming a new index tuple.
     * ----------------
     */
    datum = (Datum *) palloc(numberOfAttributes * sizeof *datum);
    nullv =  (char *)  palloc(numberOfAttributes * sizeof *nullv);

    /*
     * If this is a predicate (partial) index, we will need to evaluate the
     * predicate using ExecQual, which requires the current tuple to be in a
     * slot of a TupleTable.  In addition, ExecQual must have an ExprContext
     * referring to that slot.  Here, we initialize dummy TupleTable and
     * ExprContext objects for this purpose. --Nels, Feb '92
     */
    if (predicate != LispNil) {
	tupleTable = ExecCreateTupleTable(1);
	slot = (TupleTableSlot)
	    ExecGetTableSlot(tupleTable, ExecAllocTableSlot(tupleTable));
	econtext = RMakeExprContext();
	FillDummyExprContext(econtext, slot, heapDescriptor, buffer);
    }

    /* ----------------
     *	Ok, begin our scan of the base relation.
     * ----------------
     */
    scan = heap_beginscan(heapRelation, 	/* relation */
			  0, 			/* start at end */
			  NowTimeQual,          /* time range */
			  0,			/* number of keys */
			  (ScanKey) NULL);	/* scan key */

    reltuples = indtuples = 0;

    /* ----------------
     *	for each tuple in the base relation, we create an index
     *  tuple and add it to the index relation.  We keep a running
     *  count of the number of tuples so that we can update pg_relation
     *  with correct statistics when we're done building the index.
     * ----------------
     */
    while (heapTuple = heap_getnext(scan, 0, &buffer),
	   HeapTupleIsValid(heapTuple)) {

	reltuples++;

	/* Skip this tuple if it doesn't satisfy the partial-index predicate */
	if (predicate != LispNil) {
	    SetSlotContents(slot, heapTuple);
	    if (ExecQual(predicate, econtext) == false)
		continue;
	}

	indtuples++;

	/* ----------------
	 *  FormIndexDatum fills in its datum and null parameters
	 *  with attribute information taken from the given heap tuple.
	 * ----------------
	 */
	FormIndexDatum(numberOfAttributes,  /* num attributes */
		       attributeNumber,	    /* array of att nums to extract */
		       heapTuple,	    /* tuple from base relation */
		       heapDescriptor,	    /* heap tuple's descriptor */
		       buffer,		    /* buffer used in the scan */
		       datum,		/* return: array of attributes */
		       nullv,		/* return: array of char's */
		       funcInfo);
		
	indexTuple = FormIndexTuple(numberOfAttributes,
				    indexDescriptor,
				    datum,
				    nullv);

	indexTuple->t_tid = heapTuple->t_ctid;

	insertResult = (GeneralInsertIndexResult)
	    index_insert(indexRelation,
			 indexTuple,
			 (double *) NULL);

	pfree((Pointer)indexTuple);
    }

    heap_endscan(scan);

    if (predicate != LispNil) {
	ExecDestroyTupleTable(tupleTable, false);
    }

    pfree(nullv);
    pfree((Pointer)datum);

    /*
     *  Okay, now update the reltuples and relpages statistics for both
     *  the heap relation and the index.  These statistics are used by
     *  the planner to choose a scan type.  They are maintained generally
     *  by the vacuum daemon, but we update them here to make the index
     *  useful as soon as possible.
     */
    UpdateStats(heapRelation, reltuples);
    UpdateStats(indexRelation, indtuples);
}

/* ----------------
 *	index_build
 * ----------------
 */
void
index_build(heapRelation, indexRelation,
	    numberOfAttributes, attributeNumber,
	    parameterCount, parameter, funcInfo, predicate)
    Relation		heapRelation;
    Relation		indexRelation;
    AttributeNumber	numberOfAttributes;
    AttributeNumber	attributeNumber[];
    uint16		parameterCount;
    Datum		parameter[];
    FuncIndexInfo	*funcInfo;
    LispValue		predicate;
{
    RegProcedure	procedure;

    /* ----------------
     *	sanity checks
     * ----------------
     */
    Assert(RelationIsValid(indexRelation));
    Assert(FormIsValid((Form)indexRelation->rd_am));

    procedure = indexRelation->rd_am->ambuild;

    /* ----------------
     *	use the access method build procedure if supplied..
     * ----------------
     */
    if (RegProcedureIsValid(procedure))
	(void) fmgr(procedure,
		    heapRelation,
		    indexRelation,
		    numberOfAttributes,
		    attributeNumber,
		    RelationGetIndexStrategy(indexRelation),
		    parameterCount,
		    parameter,
		    funcInfo,
		    predicate);
    else
	DefaultBuild(heapRelation,
		     indexRelation,
		     numberOfAttributes,
		     attributeNumber,
		     RelationGetIndexStrategy(indexRelation),
		     parameterCount,
		     parameter,
		     funcInfo,
		     predicate);
}
