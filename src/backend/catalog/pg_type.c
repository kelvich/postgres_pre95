/* ----------------------------------------------------------------
 *   FILE
 *	pg_type.c
 *	
 *   DESCRIPTION
 *	routines to support manipulation of the pg_type relation
 *
 *   INTERFACE ROUTINES
 *
 *   NOTES
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#include "tmp/postgres.h"

 RcsId("$Header$");

#include "access/ftup.h"
#include "access/heapam.h"
#include "access/relscan.h"
#include "access/skey.h"
#include "utils/rel.h"
#include "utils/fmgr.h"
#include "utils/log.h"

#include "catalog/catname.h"
#include "catalog/syscache.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_type.h"
#include "catalog/indexing.h"

/* ----------------------------------------------------------------
 * 	TypeGetWithOpenRelation
 *
 *	preforms a scan on pg_type for a type tuple with the
 *	given type name.
 * ----------------------------------------------------------------
 */
ObjectId
TypeGetWithOpenRelation(pg_type_desc, typeName, defined)
    Relation	pg_type_desc;	/* reldesc for pg_type */
    Name 	typeName; 	/* name of type to be fetched */
    bool 	*defined; 	/* has the type been defined? */
{
    HeapScanDesc	scan;
    HeapTuple		tup;

    static ScanKeyEntryData	typeKey[1] = {
	{ 0, TypeNameAttributeNumber, NameEqualRegProcedure }
    };

    /* ----------------
     *	initialize the scan key and begin a scan of pg_type
     * ----------------
     */
    fmgr_info(NameEqualRegProcedure, &typeKey[0].func, &typeKey[0].nargs);
    typeKey[0].argument = NameGetDatum(typeName);
    
    scan = heap_beginscan(pg_type_desc,
			  0,
			  SelfTimeQual,
			  1,
			  (ScanKey) typeKey);
    
    /* ----------------
     *	get the type tuple, if it exists.
     * ----------------
     */
    tup = heap_getnext(scan, 0, (Buffer *) 0);

    /* ----------------
     *	if no type tuple exists for the given type name, then
     *  end the scan and return appropriate information.
     * ----------------
     */
    if (! HeapTupleIsValid(tup)) {
	heap_endscan(scan);
	*defined = false;
	return
	    InvalidObjectId;
    }

    /* ----------------
     *	here, the type tuple does exist so we pull information from
     *  the typisdefined field of the tuple and return the tuple's
     *  oid, which is the oid of the type.
     * ----------------
     */
    heap_endscan(scan);
    *defined = (bool) ((TypeTupleForm) GETSTRUCT(tup))->typisdefined;
    
    return
	tup->t_oid;
}

/* ----------------------------------------------------------------
 * 	TypeGet
 *
 *	Finds the ObjectId of a type, even if uncommitted; "defined"
 *	is only set if the type has actually been defined, i.e., if
 *	the type tuple is not a shell.
 *
 *	Note: the meat of this function is now in the function
 *	      TypeGetWithOpenRelation().  -cim 6/15/90
 *
 *	Also called from util/remove.c
 * ----------------------------------------------------------------
 */
ObjectId
TypeGet(typeName, defined)
    Name 	typeName; 	/* name of type to be fetched */
    bool 	*defined; 	/* has the type been defined? */
{
    Relation		pg_type_desc;
    ObjectId		typeoid;
    
    /* ----------------
     *	sanity checks
     * ----------------
     */
    Assert(NameIsValid(typeName));
    Assert(PointerIsValid(defined));

    /* ----------------
     *	open the pg_type relation
     * ----------------
     */
    pg_type_desc = heap_openr(TypeRelationName);

    /* ----------------
     *	scan the type relation for the information we want
     * ----------------
     */
    typeoid = TypeGetWithOpenRelation(pg_type_desc,
				      typeName,
				      defined);

    /* ----------------
     *	close the type relation and return the type oid.
     * ----------------
     */
    heap_close(pg_type_desc);
    
    return
	typeoid;
}

/* ----------------------------------------------------------------
 *	TypeShellMakeWithOpenRelation
 *
 * ----------------------------------------------------------------
 */
ObjectId
TypeShellMakeWithOpenRelation(pg_type_desc, typeName)
    Relation 	pg_type_desc;
    Name 	typeName;
{
    register int 	i;
    HeapTuple 		tup;
    char             	*values[ TypeRelationNumberOfAttributes ];
    char		nulls[ TypeRelationNumberOfAttributes ];
    ObjectId		typoid;

    /* ----------------
     *	initialize our nulls[] and values[] arrays
     * ----------------
     */
    for (i = 0; i < TypeRelationNumberOfAttributes; ++i) {
	nulls[i] = ' ';
	values[i] = (char *) NULL; 	/* redundant, but safe */
    }

    /* ----------------
     *	initialize values[] with the type name and 
     * ----------------
     */
    i = 0;
    values[i++] = (char *) typeName;
    values[i++] = (char *) InvalidObjectId;
    values[i++] = (char *) (int16) 0;
    values[i++] = (char *) (int16) 0;
    values[i++] = (char *) (Boolean) 0;
    values[i++] = (char *) (Boolean) 0;
    values[i++] = (char *) (Boolean) 0;
    values[i++] = (char *) (Boolean) 0;
    values[i++] = (char *) InvalidObjectId;
    values[i++] = (char *) InvalidObjectId;
    values[i++] = (char *) InvalidObjectId;
    values[i++] = (char *) InvalidObjectId;
    values[i++] = (char *) InvalidObjectId;
    values[i++] = (char *) InvalidObjectId;

    /*
     * ... and fill typdefault with a bogus value
     */
    values[i++] = fmgr(TextInRegProcedure, (char *) typeName);

    /* ----------------
     *	create a new type tuple with FormHeapTuple
     * ----------------
     */
    tup = FormHeapTuple(TypeRelationNumberOfAttributes,
			&pg_type_desc->rd_att,
			values,
			nulls);

    /* ----------------
     *	insert the tuple in the relation and get the tuple's oid.
     * ----------------
     */
    heap_insert(pg_type_desc, tup, (double *) NULL);
    typoid = tup->t_oid;

    if (RelationGetRelationTupleForm(pg_type_desc)->relhasindex)
    {
	Relation idescs[Num_pg_type_indices];

	CatalogOpenIndices(Num_pg_type_indices, Name_pg_type_indices, idescs);
	CatalogIndexInsert(idescs, Num_pg_type_indices, pg_type_desc, tup);
	CatalogCloseIndices(Num_pg_type_indices, idescs);
    }
    /* ----------------
     *	free the tuple and return the type-oid
     * ----------------
     */
    pfree(tup);

    return
	typoid;   
}

/* ----------------------------------------------------------------
 *	TypeShellMake
 *
 *	This procedure inserts a "shell" tuple into the type
 *	relation.  The type tuple inserted has invalid values
 *	and in particular, the "typisdefined" field is false.
 *
 *	This is used so that a tuple exists in the catalogs.
 *	The invalid fields should be fixed up sometime after
 *	this routine is called, and then the "typeisdefined"
 *	field is set to true. -cim 6/15/90
 * ----------------------------------------------------------------
 */
ObjectId
TypeShellMake(typeName)
    Name 	typeName;
{
    Relation 	pg_type_desc;
    ObjectId	typoid;

    Assert(PointerIsValid(typeName));

    /* ----------------
     *	open pg_type
     * ----------------
     */
    pg_type_desc = heap_openr(TypeRelationName);

    /* ----------------
     *	insert the shell tuple
     * ----------------
     */
    typoid = TypeShellMakeWithOpenRelation(pg_type_desc,
					   typeName);

    /* ----------------
     *	close pg_type and return the tuple's oid.
     * ----------------
     */
    heap_close(pg_type_desc);

    return
	typoid;
}

/* ----------------------------------------------------------------
 *	TypeDefine
 *
 *	This does all the necessary work needed to define a new type.
 * ----------------------------------------------------------------
 */
ObjectId
TypeDefine(typeName, relationOid, internalSize, externalSize, typeType,
       typDelim, inputProcedure, outputProcedure, sendProcedure,
	   receiveProcedure, elementTypeName, defaultTypeValue, passedByValue)
    Name	typeName;
    ObjectId	relationOid;		/* only for 'c'atalog typeTypes */
    int16	internalSize;
    int16	externalSize;
    char	typeType;
	char	typDelim;
    Name	inputProcedure;
    Name	outputProcedure;
    Name	sendProcedure;
    Name	receiveProcedure;
    Name	elementTypeName;
    char	*defaultTypeValue;	/* internal rep */
    Boolean	passedByValue;
{
    register		i, j;
    Relation 		pg_type_desc;
    HeapScanDesc 	pg_type_scan;

    ObjectId            typeObjectId;
    ObjectId            elementObjectId = InvalidObjectId;
    
    HeapTuple 		tup;
    char 		nulls[TypeRelationNumberOfAttributes];
    char	 	replaces[TypeRelationNumberOfAttributes];
    char 		*values[TypeRelationNumberOfAttributes];
    
    Buffer		buffer;
    char 		procname[sizeof(NameData)+1];
    Name		procs[4];
    bool 		defined;
    ItemPointerData	itemPointerData;
    
    static ScanKeyEntryData	typeKey[1] = {
	{ 0, TypeNameAttributeNumber, NameEqualRegProcedure }
    };

    /* ----------------
     *	sanity checks
     * ----------------
     */
    Assert(NameIsValid(typeName));
    Assert(NameIsValid(inputProcedure) && NameIsValid(outputProcedure));

    fmgr_info(NameEqualRegProcedure, &typeKey[0].func, &typeKey[0].nargs);

    /* ----------------
     *	check that the type is not already defined.
     * ----------------
     */
    typeObjectId = TypeGet(typeName, &defined);
    if (ObjectIdIsValid(typeObjectId) && defined) {
	elog(WARN, "TypeDefine: type %s already defined", typeName);
    }

    /* ----------------
     *	if this type has an associated elementType, then we check that
     *  it is defined.
     * ----------------
     */
    if (elementTypeName != NULL) {
	elementObjectId = TypeGet(elementTypeName, &defined);
    	if (!defined) {
	    elog(WARN, "TypeDefine: type %s is not defined", elementTypeName);
	}
    }

    /* ----------------
     *	XXX comment me
     * ----------------
     */
    if (externalSize == 0) {
	externalSize = -1;		/* variable length */
    }

    /* ----------------
     *	initialize arrays needed by FormHeapTuple
     * ----------------
     */
    for (i = 0; i < TypeRelationNumberOfAttributes; ++i) {
	nulls[i] = ' ';
	replaces[i] = 'r';
	values[i] = (char *) NULL; 	/* redundant, but nice */
    }

	/*
	 * XXX
	 *
	 * Do this so that user-defined types have size -1 instead of zero if
	 * they are variable-length - this is so that everything else in the
	 * backend works.
	 */

	if (internalSize == 0) internalSize = -1; 

    /* ----------------
     *	initialize the values[] information
     * ----------------
     */
    i = 0;
    values[i++] = (char *) typeName;
    values[i++] = (char *) getuid();
    values[i++] = (char *) internalSize;
    values[i++] = (char *) externalSize;
    values[i++] = (char *) passedByValue;
    values[i++] = (char *) typeType;
    values[i++] = (char *) (Boolean) 1;
    values[i++] = (char *) typDelim;
    values[i++] = (char *) (typeType == 'c' ? relationOid : InvalidObjectId);
    values[i++] = (char *) elementObjectId;
    
    /* ----------------
     *	initialize the various procedure id's in value[]
     * ----------------
     */
    procname[sizeof(NameData)] = '\0';	/* XXX feh */
    
    procs[0] = inputProcedure;
    procs[1] = outputProcedure;
    procs[2] = NameIsValid(receiveProcedure)
	? receiveProcedure : inputProcedure;
    
    procs[3] = NameIsValid(sendProcedure)
	? sendProcedure : outputProcedure;

    for (j = 0; j < 4; ++j) {
	(void) strncpy(procname,
		       (char *) procs[j],
		       sizeof(NameData));
	
	tup = SearchSysCacheTuple(PRONAME,
				  procname,
				  (char *) NULL,
				  (char *) NULL,
				  (char *) NULL);
	
	if (!HeapTupleIsValid(tup))
	    elog(WARN, "TypeDefine: procedure %s nonexistent",
		 procname);
	
	values[i++] = (char *) tup->t_oid;
    }

    /* ----------------
     *	initialize the default value for this type.
     * ----------------
     */
    values[i] = fmgr(TextInRegProcedure,
		     PointerIsValid(defaultTypeValue)
		     ? defaultTypeValue : "-");	/* XXX default typdefault */

    /* ----------------
     *	open pg_type and begin a scan for the type name.
     * ----------------
     */
    pg_type_desc = heap_openr(TypeRelationName);

    /* -----------------
     * Set a write lock initially so as not upgrade a read to a write
     * when the heap_insert() or heap_replace() is called.
     * -----------------
     */
    RelationSetLockForWrite(pg_type_desc);

    typeKey[0].argument = NameGetDatum(typeName);
    pg_type_scan = heap_beginscan(pg_type_desc,
				  0,
				  SelfTimeQual,
				  1,
				  (ScanKey) typeKey);
    
    /* ----------------
     *  define the type either by adding a tuple to the type
     *  relation, or by updating the fields of the "shell" tuple
     *  already there.
     * ----------------
     */
    tup = heap_getnext(pg_type_scan, 0, &buffer);
    if (HeapTupleIsValid(tup)) {
	tup = heap_modifytuple(tup,
			       buffer,
			       pg_type_desc,
			       values,
			       nulls,
			       replaces);
	
	/* XXX may not be necessary */
	ItemPointerCopy(&tup->t_ctid, &itemPointerData);
	
	setheapoverride(true);
	heap_replace(pg_type_desc, &itemPointerData, tup);
	setheapoverride(false);

	typeObjectId = tup->t_oid;
    } else {
	tup = heap_formtuple(TypeRelationNumberOfAttributes,
			     &pg_type_desc->rd_att,
			     values,
			     nulls);
	
	heap_insert(pg_type_desc, tup, (double *) NULL);
	
	typeObjectId = tup->t_oid;
    }

    /* ----------------
     *	finish up
     * ----------------
     */
    heap_endscan(pg_type_scan);

    if (RelationGetRelationTupleForm(pg_type_desc)->relhasindex)
    {
	Relation idescs[Num_pg_type_indices];

	CatalogOpenIndices(Num_pg_type_indices, Name_pg_type_indices, idescs);
	CatalogIndexInsert(idescs, Num_pg_type_indices, pg_type_desc, tup);
	CatalogCloseIndices(Num_pg_type_indices, idescs);
    }
    RelationUnsetLockForWrite(pg_type_desc);
    heap_close(pg_type_desc);


    return
	typeObjectId;
}

/* ----------------------------------------------------------------
 *	TypeRename
 *
 *	This renames a type 
 * ----------------------------------------------------------------
 */
void
TypeRename(oldTypeName, newTypeName)
    Name	oldTypeName;
    Name	newTypeName;
{
    Relation 		pg_type_desc;
    Relation		idescs[Num_pg_type_indices];
    ObjectId            type_oid;
    HeapTuple 		tup;
    bool 		defined;
    ItemPointerData	itemPointerData;

    Assert(NameIsValid(oldTypeName));
    Assert(NameIsValid(newTypeName));

    /* check that that the new type is not already defined */
    type_oid = TypeGet(newTypeName, &defined);
    if (ObjectIdIsValid(type_oid) && defined) {
	elog(WARN, "TypeRename: type %s already defined", newTypeName);	
    }

    /* get the type tuple from the catalog index scan manager */
    pg_type_desc = heap_openr(TypeRelationName);
    tup = TypeNameIndexScan(pg_type_desc, oldTypeName);

    /* ----------------
     *  change the name of the type
     * ----------------
     */
    if (HeapTupleIsValid(tup)) {

	bcopy(newTypeName,
	      (char *) &(((Form_pg_type) GETSTRUCT(tup))->typname),
	      sizeof(NameData));

	ItemPointerCopy(&tup->t_ctid, &itemPointerData);

	setheapoverride(true);
	heap_replace(pg_type_desc, &itemPointerData, tup);
	setheapoverride(false);

	/* update the system catalog indices */
	CatalogOpenIndices(Num_pg_type_indices, Name_pg_type_indices, idescs);
	CatalogIndexInsert(idescs, Num_pg_type_indices, pg_type_desc, tup);
	CatalogCloseIndices(Num_pg_type_indices, idescs);

	/* all done */
	pfree(tup);

    } else {
	elog(WARN, "TypeRename: type %s not defined", oldTypeName);	
    }

    /* finish up */
    heap_close(pg_type_desc);
}
