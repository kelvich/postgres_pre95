/* ----------------------------------------------------------------
 *   FILE
 *	command.c
 *	
 *   DESCRIPTION
 *	random postgres portal and utility support code
 *
 *   INTERFACE ROUTINES
 *	PortalCleanup
 *	PerformPortalFetch
 *	PerformPortalClose
 *	PerformAddAttribute
 *
 *   NOTES
 *	The PortalExecutorHeapMemory crap needs to be eliminated
 *	by designing a better executor / portal processing memory
 *	interface.
 *	
 *	The PerformAddAttribute() code, like most of the relation
 *	manipulating code in the commands/ directory, should go
 *	someplace closer to the lib/catalog code.
 *	
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#include <strings.h>
#include "tmp/postgres.h"

RcsId("$Header$");

/* ----------------
 *	FILE INCLUDE ORDER GUIDELINES
 *
 *	1) postgres.h
 *	2) various support files ("everything else")
 *	3) node files
 *	4) catalog/ files
 *	5) execdefs.h and execmisc.h, if necessary.
 *	6) extern files come last.
 * ----------------
 */
#include "access/attnum.h"
#include "access/ftup.h"
#include "access/heapam.h"
#include "access/htup.h"
#include "access/relscan.h"
#include "access/skey.h"
#include "access/tqual.h"

#include "commands/copy.h"
#include "storage/buf.h"
#include "storage/itemptr.h"
#include "tmp/miscadmin.h"
#include "tmp/portal.h"
#include "utils/excid.h"
#include "utils/log.h"
#include "utils/mcxt.h"
#include "utils/palloc.h"
#include "utils/rel.h"

#include "nodes/pg_lisp.h"
#include "nodes/primnodes.h"
#include "tcop/dest.h"
#include "commands/command.h"

#include "catalog/catname.h"
#include "catalog/syscache.h"
#include "catalog/pg_attribute.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_relation.h"
#include "catalog/pg_type.h"
#include "catalog/indexing.h"

#include "executor/execdefs.h"
#include "executor/execdesc.h"

#include "planner/internal.h"
#include "planner/prepunion.h"	/* for find_all_inheritors */

#ifndef NO_SECURITY
#include "tmp/miscadmin.h"
#include "tmp/acl.h"
#include "catalog/syscache.h"
#endif /* !NO_SECURITY */

extern List MakeList();

/* ----------------
 * 	PortalExecutorHeapMemory stuff
 *
 *	This is where the XXXSuperDuperHacky code was. -cim 3/15/90
 * ----------------
 */
MemoryContext PortalExecutorHeapMemory  = NULL;

/* --------------------------------
 * 	PortalCleanup
 * --------------------------------
 */
void
PortalCleanup(portal)
    Portal	portal;
{
    LispValue	feature;
    MemoryContext	context;

    /* ----------------
     *	sanity checks
     * ----------------
     */
    AssertArg(PortalIsValid(portal));
    AssertArg((Pointer)portal->cleanup == (Pointer)PortalCleanup);

    /* ----------------
     *	set proper portal-executor context before calling ExecMain.
     * ----------------
     */
    context = MemoryContextSwitchTo((MemoryContext) PortalGetHeapMemory(portal));
    PortalExecutorHeapMemory = (MemoryContext)
	PortalGetHeapMemory(portal);

    /* ----------------
     *	tell the executor to shutdown the query
     * ----------------
     */
    feature = MakeList(lispInteger(EXEC_END), -1);

    ExecMain(PortalGetQueryDesc(portal), PortalGetState(portal), feature);
    
    /* ----------------
     *	switch back to previous context
     * ----------------
     */
    (void) MemoryContextSwitchTo(context);
    PortalExecutorHeapMemory = (MemoryContext) NULL;
}

/* --------------------------------
 * 	PerformPortalFetch
 * --------------------------------
 */
void
PerformPortalFetch(name, forward, count, tag, dest)
    String	name;
    bool	forward;
    Count	count;
    String	tag;
    CommandDest dest;
{
    Portal		portal;
    LispValue		feature;
    List		queryDesc;
    extern List		QueryDescGetTypeInfo();		/* XXX style */
    MemoryContext	context;

    /* ----------------
     *	sanity checks
     * ----------------
     */
    if (name == NULL) {
	elog(NOTICE, "PerformPortalFetch: blank portal unsupported");
	return;
    }
    
    /* ----------------
     *	get the portal from the portal name
     * ----------------
     */
    portal = GetPortalByName(name);
    if (! PortalIsValid(portal)) {
	elog(NOTICE, "PerformPortalFetch: portal \"%-.*s\" not found",
	     NAMEDATALEN, name);
	return;
    }
    
    /* ----------------
     * 	switch into the portal context
     * ----------------
     */
    context = MemoryContextSwitchTo((MemoryContext)PortalGetHeapMemory(portal));

    AssertState(context == (MemoryContext) PortalGetHeapMemory(GetPortalByName(NULL)));

    /* ----------------
     *  setup "feature" to tell the executor what direction and
     *  how many tuples to fetch.
     * ----------------
     */
    if (forward)
	feature = MakeList(lispInteger(EXEC_FOR), lispInteger(count), -1);
    else
	feature = MakeList(lispInteger(EXEC_BACK), lispInteger(count), -1);

    /* ----------------
     *	tell the destination to prepare to recieve some tuples
     * ----------------
     */
    queryDesc = PortalGetQueryDesc(portal);
    BeginCommand(name,
		 CAtom(GetOperation(queryDesc)),
		 QueryDescGetTypeInfo(queryDesc),
		 false,	/* portal fetches don't end up in relations */
		 false,	/* this is a portal fetch, not a "retrieve portal" */
		 tag,
		 dest);

    /* ----------------
     *	execute the portal fetch operation
     * ----------------
     */
    PortalExecutorHeapMemory = (MemoryContext)
	PortalGetHeapMemory(portal);
    
    ExecMain(queryDesc, PortalGetState(portal), feature);

    /* ----------------
     * Note: the "end-of-command" tag is returned by higher-level
     *	     utility code
     *
     * Return blank portal for now.
     * Otherwise, this named portal will be cleaned.
     * Note: portals will only be supported within a BEGIN...END
     * block in the near future.  Later, someone will fix it to
     * do what is possible across transaction boundries.
     * ----------------
     */
    (void) MemoryContextSwitchTo((MemoryContext)PortalGetHeapMemory(GetPortalByName(NULL)));
}

/* --------------------------------
 * 	PerformPortalClose
 * --------------------------------
 */
void
PerformPortalClose(name, dest)
    String	name;
    CommandDest dest;
{
    Portal	portal;

    /* ----------------
     *	sanity checks
     * ----------------
     */
    if (name == NULL) {
	elog(NOTICE, "PerformPortalClose: blank portal unsupported");
	return;
    }

    /* ----------------
     *	get the portal from the portal name
     * ----------------
     */
    portal = GetPortalByName(name);
    if (! PortalIsValid(portal)) {
	elog(NOTICE, "PerformPortalClose: portal \"%-.*s\" not found",
	     NAMEDATALEN, name);
	return;
    }

    /* ----------------
     *	Note: PortalCleanup is called as a side-effect
     * ----------------
     */
    PortalDestroy(&portal);
}

/* --------------------------------
 * 	FixDomainList
 * --------------------------------
 */
static List
FixDomainList(domains)
    List	domains;
{
    LispValue	first;
    List		fixedFirst;
    List		result;
    
    if (null(domains))
	return (LispNil);
    
    first = CAR(domains);
    fixedFirst = LispNil;
    
    if (lispStringp(first)) {
	fixedFirst = MakeList(first,
			      lispInteger(-1),
			      lispInteger(strlen(CString(first))),
			      -1);
    } else {
	int len;
	
	len = length(first);
	switch (len) {
	case 1:
	    fixedFirst = MakeList(CAR(first),
				  lispInteger(0),
				  lispInteger(-1),
				  -1);
	    break;
	case 2:
	    fixedFirst = MakeList(CAR(first),
				  CADR(first),
				  lispInteger(-1)
				  -1);
	    break;
	case 3:
	    fixedFirst = first;
	    break;
	default:
	    ; /* this cannot happen */
	}
    }
    
    /*
     * return result of processing entire list
     */
    result = FixDomainList(CDR(domains));
    if (! null(fixedFirst)) {
	result = lispCons(fixedFirst, result);
    }
    
    return (result);
}

/* ----------------
 *	PerformAddAttribute
 *
 *	adds an additional attribute to a relation
 *
 *	Adds attribute field(s) to a relation.  Each new attribute
 *	is given attnums in sequential order and is added to the
 *	ATTRIBUTE relation.  If the AMI fails, defunct tuples will
 *	remain in the ATTRIBUTE relation for later vacuuming.
 *	Later, there may be some reserved attribute names???
 *
 *	(If needed, can instead use elog to handle exceptions.)
 *
 *	Note:
 *		Initial idea of ordering the tuple attributes so that all
 *	the variable length domains occured last was scratched.  Doing
 *	so would not speed access too much (in general) and would create
 *	many complications in formtuple, amgetattr, and addattribute.
 *
 *	scan attribute catalog for name conflict (within rel)
 *	scan type catalog for absence of data type (if not arg)
 *	create attnum magically???
 *	create attribute tuple
 *	insert attribute in attribute catalog
 *	modify reldesc
 *	create new relation tuple
 *	insert new relation in relation catalog
 *	delete original relation from relation catalog
 * ----------------
 */
void
PerformAddAttribute(relationName, userName, schema)
    Name	relationName;
    Name	userName;
    List	schema;
{	
    List		element;
    AttributeNumber	newAttributes;
    
    Relation		relrdesc, attrdesc;
    HeapScanDesc	attsdesc;
    HeapTuple		reltup;
    HeapTuple		attributeTuple;
    AttributeTupleForm	attribute;
    AttributeTupleFormD	attributeD;
    struct attribute	*ap, *attp, **app;
    int			i;
    int			minattnum, maxatts;
    HeapTuple		tup;
    struct	skey	key[2];	/* static better?  static not better --mao */
    ItemPointerData	oldTID;
    int			att_nvals;
    Relation		idescs[Num_pg_attr_indices];
    Relation		ridescs[Num_pg_class_indices];
    bool		hasindex;
    LispValue		first;
    
    /*
     * permissions checking.  this would normally be done in utility.c,
     * but this particular routine is recursive.
     *
     * normally, only the owner of a class can change its schema.
     */
    if (NameIsSystemRelationName(relationName))
	elog(WARN, "PerformAddAttribute: class \"%-.*s\" is a system catalog",
	     NAMEDATALEN, relationName);
#ifndef NO_SECURITY
    if (!pg_ownercheck(userName->data, relationName->data, RELNAME))
	elog(WARN, "PerformAddAttribute: you do not own class \"%-.*s\"",
	     NAMEDATALEN, relationName);
#endif

    /*
     * if the first element in the 'schema' list is a "*" then we are
     * supposed to add this attribute to all classes that inherit from
     * 'relationName' (as well as to 'relationName').
     *
     * any permissions or problems with duplicate attributes will cause
     * the whole transaction to abort, which is what we want -- all or
     * nothing.
     */
    if (schema != LispNil) {
	first = CAR(schema);
	if (lispStringp(first) && !strcmp(CString(first), "*")) {
	    ObjectId myrelid, childrelid;
	    LispValue child, children;
	    
	    relrdesc = heap_openr(relationName);
	    if (!RelationIsValid(relrdesc)) {
		elog(WARN, "PerformAddAttribute: unknown relation: \"%-.*s\"",
		     NAMEDATALEN, relationName);
	    }
	    myrelid = relrdesc->rd_id;
	    heap_close(relrdesc);
	
	    /* this routine is actually in the planner */
	    children = find_all_inheritors(lispCons(lispInteger(myrelid),
						    LispNil),
					   LispNil);

	    /*
	     * find_all_inheritors does the recursive search of the
	     * inheritance hierarchy, so all we have to do is process
	     * all of the relids in the list that it returns.
	     */
	    schema = CDR(schema);
	    foreach (child, children) {
		NameData childname;

		childrelid = CInteger(CAR(child));
		if (childrelid == myrelid)
		    continue;
		relrdesc = heap_open(childrelid);
		if (!RelationIsValid(relrdesc)) {
		    elog(WARN, "PerformAddAttribute: can't find catalog entry for inheriting class with oid %d",
			 childrelid);
		}
		namecpy(&childname, &(relrdesc->rd_rel->relname));
		heap_close(relrdesc);

		PerformAddAttribute(&childname, userName, schema);
	    }
	}
    }

    /*
     * verify that no attributes are repeated in the list
     */
    foreach (element, schema) {
	List	rest;
	
	foreach (rest, CDR(element)) {
	    if (equal((Node)CAR(CAR(element)), (Node)CAR(CAR(rest)))) {
		elog(WARN, "PerformAddAttribute: \"%s\" repeated",
		     CString(CAR(CAR(element))));
	    }
	}
    }
    
    newAttributes = length(schema);
    
    relrdesc = heap_openr(RelationRelationName);
    reltup = ClassNameIndexScan(relrdesc, (char *) relationName);

    if (!PointerIsValid(reltup)) {
	heap_close(relrdesc);
	elog(WARN, "PerformAddAttribute: relation \"%-.*s\" not found",
	     NAMEDATALEN, relationName);
    }
    /*
     * XXX is the following check sufficient?
     */
    if (((struct relation *) GETSTRUCT(reltup))->relkind == 'i') {
	elog(WARN, "PerformAddAttribute: index relation \"%-.*s\" not changed",
	     NAMEDATALEN, relationName);
	return;
    }

    minattnum = ((struct relation *) GETSTRUCT(reltup))->relnatts;
    maxatts = minattnum + newAttributes;
    if (maxatts > MaxHeapAttributeNumber) {
	pfree((char *) reltup);			/* XXX temp */
	heap_close(relrdesc);			/* XXX temp */
	elog(WARN, "PerformAddAttribute: relations limited to %d attributes",
	     MaxHeapAttributeNumber);
	return;
    }

    attrdesc = heap_openr(AttributeRelationName);

    Assert(attrdesc);
    Assert(RelationGetRelationTupleForm(attrdesc));

    /*
     * Open all (if any) pg_attribute indices
     */
    if (hasindex = RelationGetRelationTupleForm(attrdesc)->relhasindex)
	CatalogOpenIndices(Num_pg_attr_indices, Name_pg_attr_indices, idescs);
    
    ScanKeyEntryInitialize((ScanKeyEntry)&key[0], 
			   (bits16) NULL,
			   (AttributeNumber) AttributeRelationIdAttributeNumber,
			   (RegProcedure)ObjectIdEqualRegProcedure,
			   (Datum) reltup->t_oid);
    
    ScanKeyEntryInitialize((ScanKeyEntry) &key[1],
			   (bits16) NULL,
			   (AttributeNumber) AttributeNameAttributeNumber,
                           (RegProcedure)Character16EqualRegProcedure,
			   (Datum) NULL);
    
    attributeD.attrelid = reltup->t_oid;
    attributeD.attdefrel = InvalidObjectId;	/* XXX temporary */
    attributeD.attnvals = 0;		/* XXX temporary */
    attributeD.atttyparg = InvalidObjectId;	/* XXX temporary */
    attributeD.attbound = 0;		/* XXX temporary */
    attributeD.attcanindex = 0;		/* XXX need this info */
    attributeD.attproc = InvalidObjectId;	/* XXX tempoirary */
    attributeD.attcacheoff = -1;
    
    attributeTuple = addtupleheader(AttributeRelationNumberOfAttributes,
				    sizeof attributeD, (Pointer)&attributeD);
    
    attribute = (AttributeTupleForm)GETSTRUCT(attributeTuple);
    
    i = 1 + minattnum;
    foreach (element, schema) {
	HeapTuple	typeTuple;
	TypeTupleForm	form;
	char *p, *q;
	NameData r;
	int attnelems;
	
	/*
	 * XXX use syscache here as an optimization
	 */

	

	key[1].sk_data = (DATUM)CString(CAR(CAR(element)));
	attsdesc = heap_beginscan(attrdesc, 0, NowTimeQual, 2, key);
	tup = heap_getnext(attsdesc, 0, (Buffer *) NULL);
	if (HeapTupleIsValid(tup)) {
	    pfree((char *) reltup);		/* XXX temp */
	    heap_endscan(attsdesc);		/* XXX temp */
	    heap_close(attrdesc);		/* XXX temp */
	    heap_close(relrdesc);		/* XXX temp */
	    elog(WARN, "PerformAddAttribute: attribute \"%-.*s\" already exists in class \"%-.*s\"",
		 NAMEDATALEN, key[1].sk_data,
		 NAMEDATALEN, relationName);
	    return;
	}
	heap_endscan(attsdesc);
	
	/*
	 * check to see if it is an array attribute.
	 */

	p = CString(CAR(CADR(CAR(element))));

	if (CDR(CADR(CAR(element))) && IsA(CDR(CADR(CAR(element))),Array))
	{
		Array array;

		array = (Array) CDR(CADR(CAR(element)));
		attnelems = get_arrayndim(array);
		(void) namestrcpy(&r, "_");
		(void) namestrcat(&r, p);
		p = (char *) &r;
	}
	else
		attnelems = 0;

	typeTuple = SearchSysCacheTuple(TYPNAME, (char *) p, (char *) NULL,
					(char *) NULL, (char *) NULL);
	form = (TypeTupleForm)GETSTRUCT(typeTuple);
	
	if (!HeapTupleIsValid(typeTuple)) {
	    elog(WARN, "Add: type \"%s\" nonexistant",
		 CString(CADR(CAR(element))));
	}
	namecpy(&(attribute->attname), (Name) key[1].sk_data);
	attribute->atttypid = typeTuple->t_oid;
	attribute->attlen = form->typlen;
	attribute->attnum = i;
	attribute->attbyval = form->typbyval;
	attribute->attnelems = attnelems;
	attribute->attcacheoff = -1;
	attribute->attisset = (bool) (form->typtype == 'c');
	
	RelationInsertHeapTuple(attrdesc, attributeTuple,
				(double *)NULL);
	if (hasindex)
	    CatalogIndexInsert(idescs,
			       Num_pg_attr_indices,
			       attrdesc,
			       attributeTuple);
	i += 1;
    }
    if (hasindex)
	CatalogCloseIndices(Num_pg_attr_indices, idescs);
    heap_close(attrdesc);

    ((struct relation *) GETSTRUCT(reltup))->relnatts = maxatts;
    oldTID = reltup->t_ctid;
    heap_replace(relrdesc, &oldTID, reltup);

    /* keep catalog indices current */
    CatalogOpenIndices(Num_pg_class_indices, Name_pg_class_indices, ridescs);
    CatalogIndexInsert(ridescs, Num_pg_class_indices, relrdesc, reltup);
    CatalogCloseIndices(Num_pg_class_indices, ridescs);

    pfree((char *) reltup);
    heap_close(relrdesc);
}


