/* ----------------------------------------------------------------
 *   FILE
 *	tupdesc.c
 *	
 *   DESCRIPTION
 *	POSTGRES tuple descriptor support code
 *
 *   INTERFACE ROUTINES
 *	TupleDescIsValid	   - just PointerIsValid()
 *	CreateTemplateTupleDesc	   - just palloc() + bzero().
 *	TupleDescInitEntry	   - inits a single attr in a tupdesc
 *	TupleDescMakeSelfReference - initializes self reference attribute
 *	BuildDesc		   - builds tup desc from lisp list
 *	BuildDescForRelation	   - same as BuildDesc but handles self refs
 *
 *   NOTES
 *	some of the executor utility code such as "ExecTypeFromTL" should be
 *	moved here.
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#include "tmp/postgres.h"

RcsId("$Header$");

#include "nodes/pg_lisp.h"

#include "access/att.h"
#include "access/attnum.h"
#include "access/htup.h"
#include "access/tupdesc.h"

#include "utils/log.h"		/* XXX generate exceptions instead */
#include "utils/palloc.h"

#include "catalog/syscache.h"
#include "catalog/pg_type.h"

extern ObjectId TypeShellMake();

/* ----------------------------------------------------------------
 *	TupleDescIsValid
 *
 *	XXX this should either do something real or go away.
 * ----------------------------------------------------------------
 */
bool
TupleDescIsValid(desc)
    TupleDesc	desc;
{
    return(PointerIsValid(desc));
}

/* ----------------------------------------------------------------
 *	CreateTemplateTupleDesc
 *
 *	This function allocates and zeros a tuple descriptor structure.
 * ----------------------------------------------------------------
 */
TupleDesc
CreateTemplateTupleDesc(natts)
    AttributeNumber	natts;
{
    uint32	size;
    TupleDesc	desc;
    
    /* ----------------
     *	sanity checks
     * ----------------
     */
    AssertArg(natts >= 1);
    
    /* ----------------
     *  allocate enough memory for the tuple descriptor and
     *  zero it as TupleDescInitEntry assumes that the descriptor
     *  is filled with NULL pointers.
     * ----------------
     */
    size = natts * sizeof (TupleDescD);
    desc = (TupleDesc) palloc(size);
    bzero((Pointer)desc, size);

    return (desc);
}

/* ----------------------------------------------------------------
 *	TupleDescInitEntry
 *
 *	This function initializes a single attribute structure in
 *	a preallocated tuple descriptor.
 * ----------------------------------------------------------------
 */
bool
TupleDescInitEntry(desc, attributeNumber, attributeName, typeName)
    TupleDesc		desc;
    AttributeNumber	attributeNumber;
    Name		attributeName;
    Name		typeName;
{
    HeapTuple		tuple;
    TypeTupleForm	typeForm;
    Attribute		att;
    
    /* ----------------
     *	sanity checks
     * ----------------
     */
    AssertArg(TupleDescIsValid(desc));
    AssertArg(attributeNumber >= 1);
    AssertArg(NameIsValid(attributeName));
    AssertArg(NameIsValid(typeName));

    AssertArg(!AttributeIsValid(desc->data[attributeNumber - 1]));

    /* ----------------
     *	allocate storage for this attribute
     * ----------------
     */
    att = (Attribute) palloc(sizeof *desc->data[0]);
    desc->data[attributeNumber - 1] = att;

    /* ----------------
     *	initialize some of the attribute fields
     * ----------------
     */
    strncpy(&(att->attname), attributeName, 16);
    att->attnum = attributeNumber;

    /* ----------------
     *	search the system cache for the type tuple of the attribute
     *  we are creating so that we can get the typeid and some other
     *  stuff.
     *
     *  Note: in the special case of 
     *
     *	    create EMP (name = char16, manager = EMP)
     *
     *  RelationNameCreateHeapRelation() calls BuildDesc() which
     *  calls this routine and since EMP does not exist yet, the
     *  system cache lookup below fails.  That's fine, but rather
     *  then doing a elog(WARN) we just leave that information
     *  uninitialized, return false, then fix things up later.
     *  -cim 6/14/90
     * ----------------
     */
    tuple = SearchSysCacheTuple(TYPNAME, typeName);
    if (! HeapTupleIsValid(tuple)) {
	/* ----------------
	 *   here type info does not exist yet so we just fill
	 *   the attribute with dummy information and return false.
	 * ----------------
	 */
	att->atttypid = InvalidObjectId;
	att->attlen   = (int16)	0;
	att->attbyval = (Boolean) 0;
	return false;
    }
    
    /* ----------------
     *	type info exists so we initialize our attribute
     *  information from the type tuple we found..
     * ----------------
     */
    typeForm = (TypeTupleForm) GETSTRUCT(tuple);
    
    att->atttypid = tuple->t_oid;
    att->attlen   = typeForm->typlen;
    att->attbyval = typeForm->typbyval;

    return true;
}


/* ----------------------------------------------------------------
 *	TupleDescMakeSelfReference
 *
 *	This function initializes a "self-referencial" attribute.
 *	It calls TypeShellMake() which inserts a "shell" type
 *	tuple into pg_type.  
 * ----------------------------------------------------------------
 */

void
TupleDescMakeSelfReference(desc, attnum, relname)
    TupleDesc		desc;
    AttributeNumber	attnum;
    Name 		relname;
{
    Attribute att = desc->data[attnum - 1];

    att->atttypid = TypeShellMake(relname);
    att->attlen   = -1;			/* for now, relation-types are */
    att->attbyval = (Boolean) 0; 	/* actually "text" internally. */
}

/* ----------------------------------------------------------------
 *	BuildDesc
 *
 *	This is a general purpose function which takes a list of
 *	the form:
 *
 *		(("att1" "type1") ("att2" "type2") ... )
 *
 *	and returns a tuple descriptor.
 * ----------------------------------------------------------------
 */
TupleDesc
BuildDesc(schema)
    List schema;
{
    AttributeNumber	natts;
    AttributeNumber	attnum;
    List		p;
    List		entry;
    TupleDesc		desc;
    char 		*attname;
    char 		*typename;
    
    /* ----------------
     *	allocate a new tuple descriptor
     * ----------------
     */
    natts = 	length(schema);
    desc = 	CreateTemplateTupleDesc(natts);

    attnum = 0;
    
    foreach(p, schema) {
	/* ----------------
	 *	for each entry in the list, get the name and type
	 *      information from the list and have TupleDescInitEntry
	 *	fill in the attribute information we need.
	 * ----------------
	 */	
	attnum++;
	
	entry = 	CAR(p);
	attname = 	CString(CAR(entry));
	typename = 	CString(CADR(entry));

	if (!TupleDescInitEntry(desc, attnum, attname, typename)) {
	    /* ----------------
	     *	if TupleDescInitEntry() fails, it means there is
	     *  no type in the system catalogs, so we signal an
	     *  elog(WARN) which aborts the transaction.
	     * ----------------
	     */
	    elog(WARN, "BuildDesc: no such type %s", typename);
	}
    }

    return desc;
}
    
/* ----------------------------------------------------------------
 *	BuildDescForRelation
 *
 *	This is a general purpose function identical to BuildDesc
 *	but is used by the DefineRelation() code to catch the
 *	special case where you
 *
 *		create FOO ( ..., x = FOO )
 *
 *	here, the initial type lookup for "x = FOO" will fail
 *	because FOO isn't in the catalogs yet.  But since we
 *	are creating FOO, instead of doing an elog() we add
 *	a shell type tuple to pg_type and fix things later
 *	in amcreate().
 * ----------------------------------------------------------------
 */
TupleDesc
BuildDescForRelation(schema, relname)
    List schema;
    Name relname;
{
    AttributeNumber	natts;
    AttributeNumber	attnum;
    List		p;
    List		entry;
    TupleDesc		desc;
    char 		*attname;
    char 		*typename;
    
    /* ----------------
     *	allocate a new tuple descriptor
     * ----------------
     */
    natts = 	length(schema);
    desc = 	CreateTemplateTupleDesc(natts);

    attnum = 0;
    
    foreach(p, schema) {
	/* ----------------
	 *	for each entry in the list, get the name and type
	 *      information from the list and have TupleDescInitEntry
	 *	fill in the attribute information we need.
	 * ----------------
	 */	
	attnum++;
	
	entry = 	CAR(p);
	attname = 	CString(CAR(entry));
	typename = 	CString(CADR(entry));

	if (! TupleDescInitEntry(desc, attnum, attname, typename)) {
	    /* ----------------
	     *	if TupleDescInitEntry() fails, it means there is
	     *  no type in the system catalogs.  So now we check if
	     *  the type name equals the relation name.  If so we
	     *  have a self reference, otherwise it's an error.
	     * ----------------
	     */
	    if (!strcmp(typename, relname)) {
		TupleDescMakeSelfReference(desc, attnum, relname);
	    } else
		elog(WARN, "DefineRelation: no such type %s", typename);
	}
    }

    return desc;
}
