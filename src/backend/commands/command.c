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
 *	PerformRelationFilter
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

#include "commands/command.h"
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

#include "catalog/catname.h"
#include "catalog/syscache.h"
#include "catalog/pg_attribute.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_relation.h"
#include "catalog/pg_type.h"

#include "executor/execdefs.h"
#include "executor/execdesc.h"

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
    context = MemoryContextSwitchTo(PortalGetHeapMemory(portal));
    PortalExecutorHeapMemory = (MemoryContext)
	PortalGetHeapMemory(portal);

    /* ----------------
     *	tell the executor to shutdown the query
     * ----------------
     */
    feature = lispCons(lispInteger(EXEC_END), LispNil);

    ExecMain(PortalGetQueryDesc(portal), PortalGetState(portal), feature);
    
    /* ----------------
     *	switch back to previous context
     * ----------------
     */
    (void) MemoryContextSwitchTo(context);
}

/* --------------------------------
 * 	PerformPortalFetch
 * --------------------------------
 */
void
PerformPortalFetch(name, forward, count, dest)
    String	name;
    bool	forward;
    Count	count;
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
    context = MemoryContextSwitchTo(PortalGetHeapMemory(portal));

    AssertState(context == (MemoryContext) \
		PortalGetHeapMemory(GetPortalByName(NULL)));

    /* ----------------
     *  setup "feature" to tell the executor what direction and
     *  how many tuples to fetch.
     * ----------------
     */
    if (forward) {
	feature = lispInteger(EXEC_FOR);
    } else {
	feature = lispInteger(EXEC_BACK);
    }
    feature = lispCons(feature, lispCons(lispInteger(count), LispNil));

    /* ----------------
     *	tell the destination to prepare to recieve some tuples
     * ----------------
     */
    queryDesc = PortalGetQueryDesc(portal);
    BeginCommand(name,
		 QueryDescGetTypeInfo(queryDesc),
		 CAtom(GetOperation(queryDesc)),
		 false,		/* portal fetches don't end up in relations */
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
    (void) MemoryContextSwitchTo(PortalGetHeapMemory(GetPortalByName(NULL)));
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
    
    if (null(domains)) {
	return (LispNil);
    }
    
    first = CAR(domains);
    fixedFirst = LispNil;
    
    if (lispStringp(first)) {
	fixedFirst = lispCons(first,
		       lispCons(lispInteger(-1),
			 lispCons(lispInteger(strlen(CString(first))),
				  LispNil)));
    } else {
	int len;
	
	len = length(first);
	switch (len) {
	case 1:
	    fixedFirst = lispCons(CAR(first),
			   lispCons(lispInteger(0),
			     lispCons(lispInteger(-1), LispNil)));
	    break;
	case 2:
	    fixedFirst = lispCons(CAR(first),
			   lispCons(CADR(first),
			     lispCons(lispInteger(-1), LispNil)));
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
    if (!null(fixedFirst)) {
	result = lispCons(fixedFirst, result);
    }
    return (result);
}

/* --------------------------------
 * 	PerformRelationFilter
 * --------------------------------
 */
void
PerformRelationFilter(relationName, isBinary, noNulls, isFrom, fileName,
		mapName, domains)
    Name	relationName;
    bool	isBinary;
    bool	noNulls;
    bool	isFrom;
    String	fileName;
    Name	mapName;
    List	domains;
{
    AttributeNumber	numberOfAttributes;
    Domain		domainDesc;
    Count		domainCount;
    
    numberOfAttributes = length(domains);
    domains = FixDomainList(domains);
    domainDesc = createdomains(relationName, isBinary, noNulls, isFrom,
			       domains, &domainCount);
    
    copyrel(relationName, isFrom, fileName, mapName, domainCount, domainDesc);
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
    AttributeNumber	newAttributes;
    
    Relation			relrdesc, attrdesc;
    HeapScanDesc			relsdesc, attsdesc;
    HeapTuple			reltup;
    HeapTuple		attributeTuple;
    AttributeTupleForm	attribute;
    AttributeTupleFormD	attributeD;
    struct attribute		*ap, *attp, **app;
    int				i;
    int				minattnum, maxatts;
    HeapTuple			tup;
    struct	skey			key[2];	/* static better? [?] */
    ItemPointerData			oldTID;
    HeapTuple		palloctup();
    
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
    
    relrdesc = amopenr(RelationRelationName);
    key[0].sk_flags = NULL;
    key[0].sk_attnum = RelationNameAttributeNumber;
    key[0].sk_opr = Character16EqualRegProcedure;	/* XXX name= */
    key[0].sk_data = (DATUM)relationName;
    relsdesc = ambeginscan(relrdesc, 0, NowTimeQual, 1, key);
    reltup = amgetnext(relsdesc, 0, (Buffer *) NULL);
    if (!PointerIsValid(reltup)) {
	amendscan(relsdesc);
	amclose(relrdesc);
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
    reltup = palloctup(reltup, InvalidBuffer, relrdesc);
    amendscan(relsdesc);
    
    minattnum = ((struct relation *) GETSTRUCT(reltup))->relnatts;
    maxatts = minattnum + newAttributes;
    if (maxatts > MaxHeapAttributeNumber) {
	pfree((char *) reltup);			/* XXX temp */
	amclose(relrdesc);			/* XXX temp */
	elog(WARN, "addattribute: relations limited to %d attributes",
	     MaxHeapAttributeNumber);
	return;
    }
    
    attrdesc = amopenr(AttributeRelationName);
    key[0].sk_flags = NULL;
    key[0].sk_attnum = AttributeRelationIdAttributeNumber;
    key[0].sk_opr = ObjectIdEqualRegProcedure;
    key[0].sk_data = (DATUM) reltup->t_oid;
    key[1].sk_flags = NULL;
    key[1].sk_attnum = AttributeNameAttributeNumber;
    key[1].sk_opr = Character16EqualRegProcedure;	/* XXX name= */
    key[1].sk_data = (DATUM) NULL;	/* set in each iteration below */
    
    attributeD.attrelid = reltup->t_oid;
    attributeD.attdefrel = InvalidObjectId;	/* XXX temporary */
    attributeD.attnvals = 1;		/* XXX temporary */
    attributeD.atttyparg = InvalidObjectId;	/* XXX temporary */
    attributeD.attbound = 0;		/* XXX temporary */
    attributeD.attcanindex = 0;		/* XXX need this info */
    attributeD.attproc = InvalidObjectId;	/* XXX tempoirary */
    
    attributeTuple = addtupleheader(AttributeRelationNumberOfAttributes,
				    sizeof attributeD, (Pointer)&attributeD);
    
    attribute = (AttributeTupleForm)GETSTRUCT(attributeTuple);
    
    i = 1 + minattnum;
    foreach (element, schema) {
	HeapTuple	typeTuple;
	TypeTupleForm	form;
	
	/*
	 * XXX use syscache here as an optimization
	 */
	key[1].sk_data = (DATUM)CString(CAR(CAR(element)));
	attsdesc = ambeginscan(attrdesc, 0, NowTimeQual, 2, key);
	tup = amgetnext(attsdesc, 0, (Buffer *) NULL);
	if (HeapTupleIsValid(tup)) {
	    pfree((char *) reltup);		/* XXX temp */
	    amendscan(attsdesc);		/* XXX temp */
	    amclose(attrdesc);		/* XXX temp */
	    amclose(relrdesc);		/* XXX temp */
	    elog(WARN, "addattribute: attribute \"%s\" exists",
		 key[1].sk_data);
	    return;
	}
	amendscan(attsdesc);
	
	typeTuple = SearchSysCacheTuple(TYPNAME,
					CString(CADR(CAR(element))));
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
	
	RelationInsertHeapTuple(attrdesc, attributeTuple,
				(double *)NULL);
	i += 1;
    }
    
    amclose(attrdesc);
    ((struct relation *) GETSTRUCT(reltup))->relnatts = maxatts;
    oldTID = reltup->t_ctid;
    amreplace(relrdesc, &oldTID, reltup);
    pfree((char *) reltup);
    amclose(relrdesc);
}


