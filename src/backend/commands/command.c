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
#include "tcop/dest.h"
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
#include "commands/command.h"

#include "catalog/catname.h"
#include "catalog/syscache.h"
#include "catalog/pg_attribute.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_relation.h"
#include "catalog/pg_type.h"

#include "executor/execdefs.h"
#include "executor/execdesc.h"

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
	elog(NOTICE, "PerformPortalFetch: %s not found", name);
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
	elog(NOTICE, "PerformPortalClose: %s not found", name);
	return;
    }

    /* ----------------
     *	Note: PortalCleanup is called as a side-effect
     * ----------------
     */
    PortalDestroy(portal);
}

/* --------------------------------
 * 	FixDomainList
 * --------------------------------
 */
List
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
PerformAddAttribute(relationName, schema)
    Name	relationName;
    List	schema;
{	
    List		element;
    Buffer		buf;
    AttributeNumber	newAttributes;
    
    Relation		relrdesc, attrdesc;
    HeapScanDesc	relsdesc, attsdesc;
    HeapTuple		reltup;
    HeapTuple		attributeTuple;
    AttributeTupleForm	attribute;
    AttributeTupleFormD	attributeD;
    struct attribute	*ap, *attp, **app;
    int			i;
    int			minattnum, maxatts;
    HeapTuple		tup;
    struct	skey	key[2];	/* static better? [?] */
    ItemPointerData	oldTID;
	int att_nvals;
    
    if (issystem(relationName)) {
	elog(WARN, "Add: system relation \"%s\" unchanged",
	     relationName);
	return;
    }

    /*
     * verify that no attributes are repeated in the list
     */
    foreach (element, schema) {
	List	rest;
	
	foreach (rest, CDR(element)) {
	    if (equal(CAR(CAR(element)), CAR(CAR(rest)))) {
		elog(WARN, "Add: \"%s\" repeated",
		     CString(CAR(CAR(element))));
	    }
	}
    }
    
    newAttributes = length(schema);
    
    relrdesc = heap_openr(RelationRelationName);
	ScanKeyEntryInitialize((ScanKeyEntry) &key[0],
			       (bits16) NULL,
			       (AttributeNumber) RelationNameAttributeNumber,
			       (RegProcedure) Character16EqualRegProcedure,
			       (Datum)relationName);
    relsdesc = heap_beginscan(relrdesc, 0, NowTimeQual, 1, key);
    reltup = heap_getnext(relsdesc, 0, &buf);
    if (!PointerIsValid(reltup)) {
	heap_endscan(relsdesc);
	heap_close(relrdesc);
	elog(WARN, "addattribute: relation \"%s\" not found",
	     relationName);
	return;
    }
    /*
     * XXX is the following check sufficient?
     */
    if (((struct relation *) GETSTRUCT(reltup))->relkind == 'i') {
	elog(WARN, "addattribute: index relation \"%s\" not changed",
	     relationName);
	return;
    }
    reltup = palloctup(reltup, buf, relrdesc);
    heap_endscan(relsdesc);
    
    minattnum = ((struct relation *) GETSTRUCT(reltup))->relnatts;
    maxatts = minattnum + newAttributes;
    if (maxatts > MaxHeapAttributeNumber) {
	pfree((char *) reltup);			/* XXX temp */
	heap_close(relrdesc);			/* XXX temp */
	elog(WARN, "addattribute: relations limited to %d attributes",
	     MaxHeapAttributeNumber);
	return;
    }
    
    attrdesc = heap_openr(AttributeRelationName);
    
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
    attributeD.attnvals = 1;		/* XXX temporary */
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
	char *p, *q, r[16];
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
	    elog(WARN, "addattribute: attribute \"%s\" exists",
		 key[1].sk_data);
	    return;
	}
	heap_endscan(attsdesc);
	
	/*
	 * check to see if it is an array attribute.
	 */

	p = CString(CADR(CAR(element)));

	q = index(p, '[');

	if (q != NULL)
	{
		*q = '\0'; q++;
		attnelems = atoi(q);
		sprintf(r, "_%s", p);
		p = &r[0];
	}
	else
		attnelems = 1;

	typeTuple = SearchSysCacheTuple(TYPNAME,p);
	form = (TypeTupleForm)GETSTRUCT(typeTuple);
	
	if (!HeapTupleIsValid(typeTuple)) {
	    elog(WARN, "Add: type \"%s\" nonexistant",
		 CString(CADR(CAR(element))));
	}
	/*
	 * Note: structure assignment
	 */
	attribute->attname = *((Name)key[1].sk_data);
	attribute->atttypid = typeTuple->t_oid;
	attribute->attlen = form->typlen;
	attribute->attnum = i;
	attribute->attbyval = form->typbyval;
	attribute->attnelems = attnelems;
	attribute->attcacheoff = -1;
	
	RelationInsertHeapTuple(attrdesc, attributeTuple,
				(double *)NULL);
	i += 1;
    }
    
    heap_close(attrdesc);
    ((struct relation *) GETSTRUCT(reltup))->relnatts = maxatts;
    oldTID = reltup->t_ctid;
    heap_replace(relrdesc, &oldTID, reltup);
    pfree((char *) reltup);
    heap_close(relrdesc);
}


