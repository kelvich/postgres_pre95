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

#include <ctype.h>

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

#include "nodes/primnodes.h"

#include "parser/catalog_utils.h"

extern ObjectId TypeShellMake();

/*
 *	TupleDescIsValid is now a macro in tupdesc.h -cim 4/27/91
 */

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
TupleDescInitEntry(desc, attributeNumber, attributeName, 
		   typeName, attdim, attisset)
    TupleDesc		desc;
    AttributeNumber	attributeNumber;
    Name		attributeName;
    Name		typeName;
    int			attdim;
    bool                attisset;
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
    att->attrelid  = 0;				/* dummy value */
    
    strncpy(&(att->attname), attributeName, 16);
    
    att->attdefrel = 	0;			/* dummy value */
    att->attnvals  = 	0;			/* dummy value */
    att->atttyparg = 	0;			/* dummy value */
    att->attbound = 	0;			/* dummy value */
    att->attcanindex = 	0;			/* dummy value */
    att->attproc = 	0;			/* dummy value */
    att->attcacheoff = 	-1;
    
    att->attnum = attributeNumber;
    att->attnelems = attdim;
    att->attisset = attisset;

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

    /* ------------------------
       If this attribute is a set, what is really stored in the
       attribute is the OID of a tuple in the pg_proc catalog.
       The pg_proc tuple contains the query string which defines
       this set - i.e., the query to run to get the set.
       So the atttypid (just assigned above) refers to the type returned
       by this query, but the actual length of this attribute is the
       length (size) of an OID.

       Why not just make the atttypid point to the OID type, instead
       of the type the query returns?  Because the executor uses the atttypid
       to tell the front end what type will be returned (in BeginCommand),
       and in the end the type returned will be the result of the query, not
       an OID.

       Why not wait until the return type of the set is known (i.e., the
       recursive call to the executor to execute the set has returned) 
       before telling the front end what the return type will be?  Because
       the executor is a delicate thing, and making sure that the correct
       order of front-end commands is maintained is messy, especially 
       considering that target lists may change as inherited attributes
       are considered, etc.  Ugh.
       -----------------------------------------
    */
    if (attisset) {
	 Type t = type("oid");
	 att->attlen = tlen(t);
	 att->attbyval = tbyval(t);
    } else {
	 att->attlen   = typeForm->typlen;
	 att->attbyval = typeForm->typbyval;
    }


    return true;
}


/* ----------------------------------------------------------------
 *	TupleDescMakeSelfReference
 *
 *	This function initializes a "self-referential" attribute like
 *      manager in "create EMP (name=text, manager = EMP)".
 *	It calls TypeShellMake() which inserts a "shell" type
 *	tuple into pg_type.  A self-reference is one kind of set, so
 *      its size and byval are the same as for a set.  See the comments
 *      above in TupleDescInitEntry.
 * ----------------------------------------------------------------
 */

void
TupleDescMakeSelfReference(desc, attnum, relname)
    TupleDesc		desc;
    AttributeNumber	attnum;
    Name 		relname;
{
    Attribute att = desc->data[attnum - 1];
    Type t = type("oid");

    att->atttypid = TypeShellMake(relname);
    att->attlen   = tlen(t);
    att->attbyval = tbyval(t);
    att->attnelems = 0;
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
#if 0
TupleDesc
BuildDesc(schema)
    List schema;
{
    AttributeNumber	natts;
    AttributeNumber	attnum;
    List		p;
    List		entry;
    TupleDesc		desc;
    Name 		attname;
    Name 		typename;
    Array		arry;
    int			attdim;
    
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
	attname = 	(Name) CString(CAR(entry));
	typename = 	(Name) CString(CADR(entry));
	arry =		(Array) CDR(CDR(entry));

	if (arry != (Array) NULL) {
	    char buf[20];

	    attdim = get_arrayndim(arry);

	    /* array of XXX is _XXX (inherited from release 3) */
	    sprintf(buf, "_%.*s", NAMEDATALEN, typename);
	    strncpy(typename, buf, NAMEDATALEN);
	}

	if (!TupleDescInitEntry(desc, attnum, attname, typename, attdim)) {
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
#endif
    
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
    Array		arry;
    TupleDesc		desc;
    Name 		attname;
    Name 		typename;
    char		typename2[16];
    char		*pp, *pp2;
    int			attdim;
    bool                attisset;

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
	attname = 	(Name) CString(CAR(entry));

	/*
	 * What these CADR's look like:
	 * for a base type like int4:  ("int4"), or "int4" if it's an
	 * an inherited attr.  (This seems like a bug, but deal with
	 * it later.  Maybe it's a flag for later code.)
	 * for an array of int4:  ("int4" . #S(array ....))
	 * for a set of int4: ((setof . "int4"))
	 *
	 */
	if (!IsA(CADR(entry),LispList)) {
	     /* is a base type, format "int4" */
	     typename = (Name) CString(CADR(entry));
	     arry = (Array) NULL;
	     attisset = false;
	} else if (IsA(CAR(CADR(entry)),LispList)) {
	     /* is a set */
	     typename = (Name) CString(CDR(CAR(CADR(entry))));
	     arry = (Array) NULL;
	     attisset = true;
	} else if (lispNullp(CDR(CADR(entry)))) {
	     /* is a base type, format ("int4") */
	     typename = (Name) CString(CAR(CADR(entry)));
	     arry = (Array) NULL;
	     attisset = false;
	} else {
	     /* is an array */
	     typename = (Name) CString(CAR(CADR(entry)));
	     arry = (Array) CDR(CADR(entry));
	     attisset = false;
	}

	if (arry != (Array) NULL) {
	    char buf[20];

	    attdim = get_arrayndim(arry);

	    /* array of XXX is _XXX (inherited from release 3) */
	    sprintf(buf, "_%.*s", NAMEDATALEN, typename);
	    strncpy(typename, buf, NAMEDATALEN);
	}
	else
	    attdim = 0;

	if (! TupleDescInitEntry(desc, attnum, attname, 
				 typename, attdim, attisset)) {
	    /* ----------------
	     *	if TupleDescInitEntry() fails, it means there is
	     *  no type in the system catalogs.  So now we check if
	     *  the type name equals the relation name.  If so we
	     *  have a self reference, otherwise it's an error.
	     * ----------------
	     */
	    if (!strncmp(typename, relname, NAMEDATALEN)) {
		TupleDescMakeSelfReference(desc, attnum, relname);
	    } else
		elog(WARN, "DefineRelation: no such type %.*s", 
			NAMEDATALEN, typename);
	}
    }
    return desc;
}

