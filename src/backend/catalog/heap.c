/* ----------------------------------------------------------------
 *   FILE
 *	heap.c
 *	
 *   DESCRIPTION
 *	code to create and destroy POSTGRES heap relations
 *
 *   INTERFACE ROUTINES
 *	heap_creatr()		- Create an uncataloged heap relation
 *	heap_create()		- Create a cataloged relation
 *	heap_destroy()		- Removes named relation from catalogs
 *
 *   NOTES
 *	this code taken from access/heap/create.c, which contains
 *	the old amcreatr, amcreate, and amdestroy.  those routines
 *	will soon call these routines using the function manager,
 *	just like the poorly named "NewXXX" routines do.  The
 *	"New" routines are all going to die soon, once and for all!
 *	-cim 1/13/91
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#include <sys/file.h>
#include <strings.h>

#include "tmp/postgres.h"

RcsId("$Header$");

#include "access/btree.h"			/* XXX */
#include "access/heapam.h"
#include "access/hrnd.h"
#include "access/htup.h"
#include "access/istrat.h"
#include "access/relscan.h"
#include "access/skey.h"
#include "access/tqual.h"	/* for NowTimeQual */
#include "rules/rlock.h"
#include "storage/buf.h"
#include "storage/itemptr.h"
#include "tmp/hasht.h"
#include "tmp/miscadmin.h"
#include "utils/fmgr.h"
#include "utils/log.h"			/* XXX */
#include "utils/mcxt.h"
#include "utils/rel.h"
#include "utils/relcache.h"

#include "catalog/catname.h"
#include "catalog/pg_relation.h"
#include "catalog/pg_attribute.h"
#include "catalog/pg_index.h"
#include "catalog/pg_inherits.h"
#include "catalog/pg_ipl.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_type.h"
#include "catalog/indexing.h"

#include "lib/catalog.h"

#ifdef sprite
#include "sprite_file.h"
#endif /* sprite */

#ifndef	private
#define private	/* public */
#endif

/* ----------------------------------------------------------------
 *		XXX UGLY HARD CODED BADNESS FOLLOWS XXX
 *
 *	these should all be moved to someplace in the lib/catalog
 *	module, if not obliterated first.
 * ----------------------------------------------------------------
 */


/*
 * Note:
 *	Should the executor special case these attributes in the future?
 *	Advantage:  consume 1/2 the space in the ATTRIBUTE relation.
 *	Disadvantage:  having rules to compute values in these tuples may
 *		be more difficult if not impossible.
 */

static	struct	attribute a1 = {
    -1l, "ctid", 27l, 0l, 0l, 0l, sizeof (ItemPointerData),
    SelfItemPointerAttributeNumber, 0, '\0', '\001', 0l
    };

static	struct	attribute a2 = {
    -1l, "lock", 31l, 0l, 0l, 0l, -1,
    RuleLockAttributeNumber, 0, '\0', '\001', 0l
    };

static	struct	attribute a3 = {
    -1l, "oid", 26l, 0l, 0l, 0l, sizeof (OID),
    ObjectIdAttributeNumber, 0, '\001', '\001', 0l
    };

static	struct	attribute a4 = {
    -1l, "xmin", 28l, 0l, 0l, 0l, sizeof (XID),
    MinTransactionIdAttributeNumber, 0, '\0', '\001', 0l
    };

static	struct	attribute a5 = {
    -1l, "cmin", 29l, 0l, 0l, 0l, sizeof (CID),
    MinCommandIdAttributeNumber, 0, '\001', '\001', 0l
    };

static	struct	attribute a6 = {
    -1l, "xmax", 28l, 0l, 0l, 0l, sizeof (XID),
    MaxTransactionIdAttributeNumber, 0, '\0', '\001', 0l
    };

static	struct	attribute a7 = {
    -1l, "cmax", 29l, 0l, 0l, 0l, sizeof (CID),
    MaxCommandIdAttributeNumber, 0, '\001', '\001', 0l
    };

static	struct	attribute a8 = {
    -1l, "chain", 27l, 0l, 0l, 0l, sizeof (ItemPointerData),
    ChainItemPointerAttributeNumber, 0, '\0', '\001', 0l
    };

static	struct	attribute a9 = {
    -1l, "anchor", 27l, 0l, 0l, 0l, sizeof (ItemPointerData),
    AnchorItemPointerAttributeNumber, 0, '\0', '\001', 0l
    };

static	struct	attribute a10 = {
    -1l, "tmin", 20l, 0l, 0l, 0l, sizeof (ABSTIME),
    MinAbsoluteTimeAttributeNumber, 0, '\001', '\001', 0l
    };

static	struct	attribute a11 = {
    -1l, "tmax", 20l, 0l, 0l, 0l, sizeof (ABSTIME),
    MaxAbsoluteTimeAttributeNumber, 0, '\001', '\001', 0l
    };

static	struct	attribute a12 = {
    -1l, "vtype", 18l, 0l, 0l, 0l, sizeof (char),
    VersionTypeAttributeNumber, 0, '\001', '\001', 0l
    };

static	struct	attribute *HeapAtt[] =
{ &a1, &a2, &a3, &a4, &a5, &a6, &a7, &a8, &a9, &a10, &a11, &a12 };

/* ----------------------------------------------------------------
 *		XXX END OF UGLY HARD CODED BADNESS XXX
 * ----------------------------------------------------------------
 */


/* ----------------------------------------------------------------
 *	heap_creatr	- Create an uncataloged heap relation
 *
 *	Fields relpages, reltuples, reltuples, relkeys, relhistory,
 *	relisindexed, and relkind of rdesc->rd_rel are initialized
 *	to all zeros, as are rd_last and rd_hook.  Rd_refcnt is set to 1.
 *
 *	Remove the system relation specific code to elsewhere eventually.
 *
 *	Eventually, must place information about this temporary relation
 *	into the transaction context block.
 * ----------------------------------------------------------------
 */
Relation
heap_creatr(relname, natts, smgr, att)
    char	relname[];
    unsigned	natts;
    unsigned	smgr;
    struct	attribute *att[];		/* XXX */
{
    register int	i;
    ObjectId		relid;
    Relation		rdesc;
    int			len;
    bool		nailme = false;

    File		fd;

    extern GlobalMemory	CacheCxt;
    MemoryContext	oldcxt;

    /* int			issystem(); */
    File		smgrcreate();		/* XXX */
    /* OID			newoid(); */
    extern		bcopy(), bzero();

    /* ----------------
     *	sanity checks
     * ----------------
     */
    if (!natts)
	elog(WARN, "heap_creatr(%s): called with 0 natts", relname);

    if (issystem(relname) && IsNormalProcessingMode())
    {
	elog(WARN, 
	    "Illegal class name: %s -- pg_ is reserved for system catalogs",
	    relname);
    }

    /* ----------------
     *	switch to the cache context so that we don't lose
     *  allocations at the end of this transaction, I guess.
     *  -cim 6/14/90
     * ----------------
     */
    if (!CacheCxt)
	CacheCxt = CreateGlobalMemory("Cache");
	
    oldcxt = MemoryContextSwitchTo((MemoryContext)CacheCxt);

    /* ----------------
     *	real ugly stuff to assign the proper relid in the relation
     *  descriptor follows.
     * ----------------
     */
    if (! strncmp(relname, RelationRelationName, 16))
    {
	relid = RelOid_pg_relation;
	nailme = true;
    }
    else if (! strncmp(relname, AttributeRelationName, 16))
    {
	relid = RelOid_pg_attribute;
	nailme = true;
    }
    else if (! strncmp(relname, ProcedureRelationName, 16))
    {
	relid = RelOid_pg_proc;
	nailme = true;
    }
    else if (! strncmp(relname, TypeRelationName, 16))
    {
	relid = RelOid_pg_type;
	nailme = true;
    }
    else  {
	relid = newoid();
	if (! strncmp(relname, "temp_", 5))
	    sprintf(relname, "temp_%d", relid);
	  };

    /* ----------------
     *	allocate a new relation descriptor.
     *
     * 	XXX the length computation may be incorrect, handle elsewhere
     * ----------------
     */
    len = sizeof *rdesc + (int)(natts - 1) * sizeof rdesc->rd_att;
    len += sizeof (IndexStrategy) + sizeof(RegProcedure *);

    rdesc = (Relation) palloc(len);
    bzero((char *)rdesc, len);

    /* ----------------
     *	initialize the fields of our new relation descriptor
     * ----------------
     */
    rdesc->rd_fd = fd;

    /* ----------------
     *  nail the reldesc if this is a bootstrap create reln and
     *  we may need it in the cache later on in the bootstrap
     *  process so we don't ever want it kicked out.  e.g. pg_attribute!!!
     * ----------------
     */
    if (nailme)
	rdesc->rd_isnailed = true;

    RelationSetReferenceCount(rdesc, 1);

    rdesc->rd_rel = (RelationTupleForm)
	palloc(sizeof (RuleLock) + sizeof *rdesc->rd_rel);

    bzero((char *)rdesc->rd_rel, sizeof *rdesc->rd_rel + sizeof (RuleLock));
    strncpy((char *)&rdesc->rd_rel->relname, relname, sizeof(NameData));
    rdesc->rd_rel->relkind = 'u';
    rdesc->rd_rel->relnatts = natts;
    rdesc->rd_rel->relsmgr = smgr;

    for (i = 0; i < natts; i++) {
	rdesc->rd_att.data[i] = (AttributeTupleForm)
	    palloc(sizeof *att[0] + sizeof (RuleLock));

	bcopy((char *)att[i], (char *)rdesc->rd_att.data[i], sizeof *att[0]);
	bzero((char *)(rdesc->rd_att.data[i] + 1), sizeof (RuleLock));
	rdesc->rd_att.data[i]->attrelid = relid;
    }

    rdesc->rd_id = relid;

    /* ----------------
     *	have the storage manager create the relation.
     * ----------------
     */

    rdesc->rd_fd = smgrcreate(smgr, rdesc);

    RelationRegisterRelation(rdesc);

    MemoryContextSwitchTo(oldcxt);

    return (rdesc);
}


/* ----------------------------------------------------------------
 *	heap_create	- Create a cataloged relation
 *
 *	this is done in 6 steps:
 *
 *	1) CheckAttributeNames() is used to make certain the tuple
 *	   descriptor contains a valid set of attribute names
 *
 *	2) pg_relation is opened and RelationAlreadyExists()
 *	   preforms a scan to ensure that no relation with the
 *         same name already exists.
 *
 *	3) amcreatr() is called to create the new relation on
 *	   disk.
 *
 *	4) TypeDefine() is called to define a new type corresponding
 *	   to the new relation.
 *
 *	5) AddNewAttributeTuples() is called to register the
 *	   new relation's schema in pg_attribute.
 *
 *	6) AddPgRelationTuple() is called to register the
 *	   relation itself in the catalogs.
 *
 *	7) the relations are closed and the new relation's oid
 *	   is returned.
 *
 * old comments:
 *	A new relation is inserted into the RELATION relation
 *	with the specified attribute(s) (newly inserted into
 *	the ATTRIBUTE relation).  How does concurrency control
 *	work?  Is it automatic now?  Expects the caller to have
 *	attname, atttypid, atttyparg, attproc, and attlen domains filled.
 *	Create fills the attnum domains sequentually from zero,
 *	fills the attnvals domains with zeros, and fills the
 *	attrelid fields with the relid.
 *
 *	scan relation catalog for name conflict
 *	scan type catalog for typids (if not arg)
 *	create and insert attribute(s) into attribute catalog
 *	create new relation
 *	insert new relation into attribute catalog
 *
 *	Should coordinate with amcreatr().  Either it should
 *	not be called or there should be a way to prevent
 *	the relation from being removed at the end of the
 *	transaction if it is successful ('u'/'r' may be enough).
 *	Also, if the transaction does not commit, then the
 *	relation should be removed.
 *
 *	XXX amcreate ignores "off" when inserting (for now).
 *	XXX amcreate (like the other utilities) needs to understand indexes.
 *	
 * ----------------------------------------------------------------
 */

/* --------------------------------
 *	CheckAttributeNames
 *
 *	this is used to make certain the tuple descriptor contains a
 *	valid set of attribute names.  a problem simply generates
 *	elog(WARN) which aborts the current transaction.
 * --------------------------------
 */
void
CheckAttributeNames(natts, tupdesc)
    unsigned		natts;
    struct attribute 	*tupdesc[];
{
    int	i;
    int	j;

    /* ----------------
     *	first check for collision with system attribute names
     * ----------------
     */
    for (i = 0; i < natts; i += 1) {
	for (j = 0; j < sizeof HeapAtt / sizeof HeapAtt[0]; j += 1) {
	    if (NameIsEqual((Name)(HeapAtt[j]->attname),
			    (Name)(tupdesc[i]->attname))) {
		elog(WARN,
		     "create: system attribute named \"%s\"",
		     HeapAtt[j]->attname);
	    }
	}
    }

    /* ----------------
     *	next check for repeated attribute names
     * ----------------
     */
    for (i = 1; i < natts; i += 1) {
	for (j = 0; j < i; j += 1) {
	    if (NameIsEqual((Name)(tupdesc[j]->attname),
			    (Name)(tupdesc[i]->attname))) {
		elog(WARN,
		     "create: repeated attribute \"%s\"",
		     tupdesc[j]->attname);
	    }
	}
    }
}

/* --------------------------------
 *	RelationAlreadyExists
 *
 *	this preforms a scan of pg_relation to ensure that
 *	no relation with the same name already exists.  The caller
 *	has to open pg_relation and pass an open descriptor.
 * --------------------------------
 */
int
RelationAlreadyExists(pg_relation_desc, relname)
    Relation		pg_relation_desc;
    char		relname[];
{
    ScanKeyEntryData	key;
    HeapScanDesc	pg_relation_scan;
    HeapTuple		tup;

    /* ----------------
     *	form the scan key
     * ----------------
     */
	ScanKeyEntryInitialize(&key,
			       0,
			       (AttributeNumber)Anum_pg_relation_relname,
			       (RegProcedure)F_CHAR16EQ,
			       (Datum) relname);

    /* ----------------
     *	begin the scan
     * ----------------
     */
    pg_relation_scan = ambeginscan(pg_relation_desc,
				   0,
				   NowTimeQual,
				   1,
				   &key);

    /* ----------------
     *	get a tuple.  if the tuple is NULL then it means we
     *  didn't find an existing relation.
     * ----------------
     */
    tup = amgetnext(pg_relation_scan, 0, (Buffer *)NULL);

    /* ----------------
     *	end the scan and return existance of relation.
     * ----------------
     */
    amendscan(pg_relation_scan);

    return
	(PointerIsValid(tup) == true);
}

/* --------------------------------
 *	AddNewAttributeTuples
 *
 *	this registers the new relation's schema by adding
 *	tuples to pg_attribute.
 * --------------------------------
 */
void
AddNewAttributeTuples(new_rel_oid, new_type_oid, natts, tupdesc)
    ObjectId		new_rel_oid;
    ObjectId		new_type_oid; /* might not be needed. */
    unsigned		natts;
    struct attribute 	*tupdesc[];
{
    register struct attribute **dpp;		/* XXX */

    HeapTuple	tup;
    Relation	rdesc;
    int		i;
    bool	hasindex;
    Relation	idescs[Num_pg_attr_indices];

    /* ----------------
     *	open pg_attribute
     * ----------------
     */
    rdesc = amopenr(AttributeRelationName);

    /* -----------------
     * Check if we have any indices defined on pg_attribute.
     * -----------------
     */
    Assert(rdesc);
    Assert(rdesc->rd_rel);
    if (hasindex = RelationGetRelationTupleForm(rdesc)->relhasindex)
	CatalogOpenIndices(Num_pg_attr_indices, Name_pg_attr_indices, idescs);

    /* ----------------
     *	initialize tuple descriptor.  Note we use setheapoverride()
     *  so that we can see the effects of our TypeDefine() done
     *  previously.
     * ----------------
     */
    setheapoverride(true);
    fillatt(natts, (AttributeTupleForm*)tupdesc);
    setheapoverride(false);

    /* ----------------
     *	make sure all the tuples are added together.
     * ----------------
     */
    setclusteredappend(true);

    /* ----------------
     *  first we add the user attributes..
     * ----------------
     */
    dpp = tupdesc;
    for (i = 0; i < natts; i++) {
	(*dpp)->attrelid = new_rel_oid;
	(*dpp)->attnvals = 0l;

	tup = addtupleheader(Natts_pg_attribute,
			     sizeof (struct attribute),
			     (char *) *dpp);
	
	aminsert(rdesc, tup, (double *)NULL);

	if (hasindex)
	    CatalogIndexInsert(idescs, Num_pg_attr_indices, rdesc, tup);
	
	pfree((char *)tup);
	dpp++;
    }

    /* ----------------
     *	next we add the system attributes..
     * ----------------
     */
    dpp = HeapAtt;
    for (i = 0; i < -1 - FirstLowInvalidHeapAttributeNumber; i++) {
	(*dpp)->attrelid = new_rel_oid;
     /*	(*dpp)->attnvals = 0l;	unneeded */
	
	tup = addtupleheader(Natts_pg_attribute,
			     sizeof (struct attribute),
			     (char *)*dpp);

	aminsert(rdesc, tup, (double *)NULL);
	
	if (hasindex)
	    CatalogIndexInsert(idescs, Num_pg_attr_indices, rdesc, tup);

	pfree((char *)tup);
	dpp++;
    }

    /* ----------------
     *	all done with pg_attribute now, so turn off
     *  "clusterd append mode" and close our relation.
     * ----------------
     */
    setclusteredappend(false);
    amclose(rdesc);
    /*
     * close pg_attribute indices
     */
    if (hasindex)
	CatalogCloseIndices(Num_pg_attr_indices, idescs);
}

/* --------------------------------
 *	AddPgRelationTuple
 *
 *	this registers the new relation in the catalogs by
 *	adding a tuple to pg_relation.
 * --------------------------------
 */
void
AddPgRelationTuple(pg_relation_desc, new_rel_desc, new_rel_oid, arch, natts)
    Relation		pg_relation_desc;
    Relation		new_rel_desc;
    ObjectId		new_rel_oid;
    int			arch;
    unsigned		natts;
{
    RelationTupleForm	new_rel_reltup;
    HeapTuple		tup;
    bool		isBootstrap;

    /* ----------------
     *	first we munge some of the information in our
     *  uncataloged relation's relation descriptor.
     * ----------------
     */
    new_rel_reltup = new_rel_desc->rd_rel;

/* CHECK should get new_rel_oid first via an insert then use XXX */
/*   new_rel_reltup->reltuples = 1; */ /* XXX */

    new_rel_reltup->relowner = GetUserId();
    new_rel_reltup->relkind = 'r';
    new_rel_reltup->relarch = arch;
    new_rel_reltup->relnatts = natts;

    /* ----------------
     *	now form a tuple to add to pg_relation
     * ----------------
     */
    tup = addtupleheader(Natts_pg_relation,
			 sizeof (struct relation),
			 (char *) new_rel_reltup);
    tup->t_oid = new_rel_oid;

    /* ----------------
     *  finally insert the new tuple and free it.
     *
     *  Note: I have no idea why we do a
     *		SetProcessingMode(BootstrapProcessing);
     *        here -cim 6/14/90
     * ----------------
     */
    isBootstrap = IsBootstrapProcessingMode() ? true : false;

    SetProcessingMode(BootstrapProcessing);

    aminsert(pg_relation_desc, tup, (double *)NULL);

    if (! isBootstrap)
	SetProcessingMode(NormalProcessing);

    pfree((char *)tup);
}


/* --------------------------------
 *	heap_create	
 *
 *	creates a new cataloged relation.  see comments above.
 * --------------------------------
 */
ObjectId
heap_create(relname, arch, natts, smgr, tupdesc)
    char		relname[];
    int			arch;
    unsigned		natts;
    unsigned		smgr;
    struct attribute 	*tupdesc[];
{
    Relation		pg_relation_desc;
    Relation		new_rel_desc;
    ObjectId		new_rel_oid;
    ObjectId 		new_type_oid;
    int			i;

    /* ----------------
     *	sanity checks
     * ----------------
     */
    AssertState(IsNormalProcessingMode() || IsBootstrapProcessingMode());
    if (natts == 0 || natts > MaxHeapAttributeNumber)
	elog(WARN, "amcreate: from 1 to %d attributes must be specified",
	     MaxHeapAttributeNumber);

    CheckAttributeNames(natts, tupdesc);

    /* ----------------
     *	open pg_relation and see that the relation doesn't
     *  already exist.
     * ----------------
     */
    pg_relation_desc = amopenr(RelationRelationName);

    if (RelationAlreadyExists(pg_relation_desc, relname)) {
	amclose(pg_relation_desc);
	elog(WARN, "amcreate: %s relation already exists", relname);
    }

    /* ----------------
     *  ok, relation does not already exist so now we
     *	create an uncataloged relation and pull its relation oid
     *  from the newly formed relation descriptor.
     *
     *  Note: The call to amcreatr() does all the "real" work
     *  of creating the disk file for the relation.
     * ----------------
     */
    new_rel_desc = amcreatr(relname, natts, smgr, tupdesc);
    new_rel_oid  = new_rel_desc->rd_att.data[0]->attrelid;

    /* ----------------
     *  since defining a relation also defines a complex type,
     *	we add a new system type corresponding to the new relation.
     * ----------------
     */
    new_type_oid = TypeDefine(relname,		/* type name */
			      new_rel_oid,      /* relation oid */
			      -1,		/* internal size (varlena) */
			      -1,		/* external size (varlena) */
			      'c', 		/* type-type (catalog) */
				  ',',		/* default array delimiter */
			      "textin",		/* input procedure */
			      "textout",	/* output procedure */
			      "textin",		/* send procedure */
			      "textout",	/* recieve procedure */
				  NULL,     /* array element type - irrelevent */
			      "-",		/* default type value */
			      (Boolean) 0);	/* passed by value */

    /* ----------------
     *	now add tuples to pg_attribute for the attributes in
     *  our new relation.
     * ----------------
     */
    AddNewAttributeTuples(new_rel_oid,
			  new_type_oid,
			  natts,
			  tupdesc);

    /* ----------------
     *	now update the information in pg_relation.
     * ----------------
     */
    AddPgRelationTuple(pg_relation_desc,
		       new_rel_desc,
		       new_rel_oid,
		       arch,
		       natts);

    /* ----------------
     *	ok, the relation has been cataloged, so close our relations
     *  and return the oid of the newly created relation.
     *
     *	SOMEDAY: fill the STATISTIC relation properly.
     * ----------------
     */
    amclose(new_rel_desc);
    amclose(pg_relation_desc);

    return new_rel_oid;
}


/* ----------------------------------------------------------------
 *	heap_destroy	- removes all record of named relation from catalogs
 *
 *	1)  open relation, check for existence, etc.
 *	2)  remove inheritance information
 *	3)  remove indexes
 *	4)  remove pg_relation tuple
 *	5)  remove pg_attribute tuples
 *	6)  remove pg_type tuples
 *	7)  unlink relation
 *
 * old comments
 *	Except for vital relations, removes relation from
 *	relation catalog, and related attributes from
 *	attribute catalog (needed?).  (Anything else???)
 *
 *	get proper relation from relation catalog (if not arg)
 *	check if relation is vital (strcmp()/reltype???)
 *	scan attribute catalog deleting attributes of reldesc
 *		(necessary?)
 *	delete relation from relation catalog
 *	(How are the tuples of the relation discarded???)
 *
 *	XXX Must fix to work with indexes.
 *	There may be a better order for doing things.
 *	Problems with destroying a deleted database--cannot create
 *	a struct reldesc without having an open file descriptor.
 * ----------------------------------------------------------------
 */

/* --------------------------------
 *	RelationRemoveInheritance
 *
 * 	Note: for now, we cause an exception if relation is a
 *	superclass.  Someday, we may want to allow this and merge
 *	the type info into subclass procedures....  this seems like
 *	lots of work.
 * --------------------------------
 */
void
RelationRemoveInheritance(relation)
    Relation	relation;
{
    Relation		catalogRelation;
    Relation		iplRelation;
    HeapTuple		tuple;
    HeapScanDesc	scan;
    ScanKeyEntryData	entry[1];

    /* ----------------
     *	open pg_inherits
     * ----------------
     */
    catalogRelation = RelationNameOpenHeapRelation(InheritsRelationName);

    /* ----------------
     *	form a scan key for the subclasses of this class
     *  and begin scanning
     * ----------------
     */
	ScanKeyEntryInitialize(&entry[0], 0x0, InheritsParentAttributeNumber,
						   ObjectIdEqualRegProcedure,
						   ObjectIdGetDatum(RelationGetRelationId(relation)));

    scan = RelationBeginHeapScan(catalogRelation,
				 false,
				 NowTimeQual,
				 1,
				 entry);

    /* ----------------
     *	if any subclasses exist, then we disallow the deletion.
     * ----------------
     */
    tuple = HeapScanGetNextTuple(scan, false, (Buffer *)NULL);
    if (HeapTupleIsValid(tuple)) {
	HeapScanEnd(scan);
	RelationCloseHeapRelation(catalogRelation);

	elog(WARN, "relation <%d> inherits \"%s\"",
	     ((InheritsTupleForm) GETSTRUCT(tuple))->inhrel,
	     RelationGetRelationName(relation));
    }

    /* ----------------
     *	If we get here, it means the relation has no subclasses
     *  so we can trash it.  First we remove dead INHERITS tuples.
     * ----------------
     */
    entry[0].attributeNumber = InheritsRelationIdAttributeNumber;

    scan = RelationBeginHeapScan(catalogRelation,
				 false,
				 NowTimeQual,
				 1,
				 entry);

    for (;;) {
	tuple = HeapScanGetNextTuple(scan, false, (Buffer *)NULL);
	if (!HeapTupleIsValid(tuple)) {
	    break;
	}
	RelationDeleteHeapTuple(catalogRelation, &tuple->t_ctid);
    }

    HeapScanEnd(scan);
    RelationCloseHeapRelation(catalogRelation);

    /* ----------------
     *	now remove dead IPL tuples
     * ----------------
     */
    catalogRelation =
	RelationNameOpenHeapRelation(InheritancePrecidenceListRelationName);

    entry[0].attributeNumber =
	InheritancePrecidenceListRelationIdAttributeNumber;

    scan = RelationBeginHeapScan(catalogRelation,
				 false,
				 NowTimeQual,
				 1,
				 entry);

    for (;;) {
	tuple = HeapScanGetNextTuple(scan, false, (Buffer *)NULL);
	if (!HeapTupleIsValid(tuple)) {
	    break;
	}
	RelationDeleteHeapTuple(catalogRelation, &tuple->t_ctid);
    }

    HeapScanEnd(scan);
    RelationCloseHeapRelation(catalogRelation);
}

/* --------------------------------
 *	RelationRemoveIndexes
 *	
 * --------------------------------
 */
void
RelationRemoveIndexes(relation)
    Relation	relation;
{
    ObjectId		indexId;
    Relation		indexRelation;
    HeapTuple		tuple;
    HeapScanDesc	scan;
    ScanKeyEntryData	entry[1];

    indexRelation = RelationNameOpenHeapRelation(IndexRelationName);

	ScanKeyEntryInitialize(&entry[0], 0x0, IndexHeapRelationIdAttributeNumber,
						   ObjectIdEqualRegProcedure,
						   ObjectIdGetDatum(RelationGetRelationId(relation)));

    scan = RelationBeginHeapScan(indexRelation,
				 false,
				 NowTimeQual,
				 1,
				 entry);

    for (;;) {
	tuple = HeapScanGetNextTuple(scan, false, (Buffer *)NULL);
	if (!HeapTupleIsValid(tuple)) {
	    break;
	}

	DestroyIndexRelationById(
		((IndexTupleForm)GETSTRUCT(tuple))->indexrelid);
    }

    HeapScanEnd(scan);
    RelationCloseHeapRelation(indexRelation);
}

/* --------------------------------
 *	DeletePgRelationTuple
 *
 * --------------------------------
 */
void
DeletePgRelationTuple(rdesc)
    Relation		rdesc;
{
    Relation		pg_relation_desc;
    HeapScanDesc	pg_relation_scan;
    ScanKeyEntryData	key;
    HeapTuple		tup;

    /* ----------------
     *	open pg_relation
     * ----------------
     */
    pg_relation_desc = amopenr(RelationRelationName);

    /* ----------------
     *	create a scan key to locate the relation oid of the
     *  relation to delete
     * ----------------
     */
	ScanKeyEntryInitialize(&key, NULL, ObjectIdAttributeNumber,
						   F_INT4EQ, rdesc->rd_att.data[0]->attrelid);

    pg_relation_scan =  ambeginscan(pg_relation_desc,
				    0,
				    NowTimeQual,
				    1,
				    &key);

    /* ----------------
     *	use amgetnext() to fetch the pg_relation tuple.  If this
     *  tuple is not valid then something's wrong.
     * ----------------
     */
    tup = amgetnext(pg_relation_scan, 0, (Buffer *) NULL);

    if (! PointerIsValid(tup)) {
	amendscan(pg_relation_scan);
	amclose(pg_relation_desc);
	elog(WARN, "DeletePgRelationTuple: %s relation nonexistent",
	     &rdesc->rd_rel->relname);
    }

    /* ----------------
     *	delete the relation tuple from pg_relation, and finish up.
     * ----------------
     */
    amendscan(pg_relation_scan);
    amdelete(pg_relation_desc, &tup->t_ctid);

    amclose(pg_relation_desc);
}

/* --------------------------------
 *	DeletePgAttributeTuples
 *
 * --------------------------------
 */
void
DeletePgAttributeTuples(rdesc)
    Relation	rdesc;
{
    Relation		pg_attribute_desc;
    HeapScanDesc	pg_attribute_scan;
    ScanKeyEntryData	key;
    HeapTuple		tup;

    /* ----------------
     *	open pg_attribute
     * ----------------
     */
    pg_attribute_desc = amopenr(AttributeRelationName);

    /* ----------------
     *	create a scan key to locate the attribute tuples to delete
     *  and begin the scan.
     * ----------------
     */
    ScanKeyEntryInitialize(&key, NULL, Anum_pg_attribute_attrelid,
						   F_INT4EQ, rdesc->rd_att.data[0]->attrelid);

    /* -----------------
     * Get a write lock _before_ getting the read lock in the scan
     * ----------------
     */
    RelationSetLockForWrite(pg_attribute_desc);

    pg_attribute_scan = ambeginscan(pg_attribute_desc,
				    0,
				    NowTimeQual,
				    1,
				    &key);

    /* ----------------
     *	use amgetnext() / amdelete() until all attribute tuples
     *  have been deleted.
     * ----------------
     */
    while (tup = amgetnext(pg_attribute_scan, 0, (Buffer *)NULL),
	   PointerIsValid(tup)) {

	amdelete(pg_attribute_desc, &tup->t_ctid);
    }

    /* ----------------
     *	finish up.
     * ----------------
     */
    amendscan(pg_attribute_scan);

    /* ----------------
     * Release the write lock 
     * ----------------
     */
    RelationUnsetLockForWrite(pg_attribute_desc);
    amclose(pg_attribute_desc);
}


/* --------------------------------
 *	DeletePgTypeTuple
 *
 *	If the user attempts to destroy a relation and there
 *	exists attributes in other relations of type
 *	"relation we are deleting", then we have to do something
 *	special.  presently we disallow the destroy.
 * --------------------------------
 */
void
DeletePgTypeTuple(rdesc)
    Relation		rdesc;
{
    Relation		pg_type_desc;
    HeapScanDesc	pg_type_scan;
    Relation		pg_attribute_desc;
    HeapScanDesc	pg_attribute_scan;
    ScanKeyEntryData	key;
    ScanKeyEntryData	attkey;
    HeapTuple		tup;
    HeapTuple		atttup;
    ObjectId		typoid;

    /* ----------------
     *	open pg_type
     * ----------------
     */
    pg_type_desc = amopenr(TypeRelationName);

    /* ----------------
     *	create a scan key to locate the type tuple corresponding
     *  to this relation.
     * ----------------
     */
	ScanKeyEntryInitialize(&key, NULL, Anum_pg_type_typrelid, F_INT4EQ,
						   rdesc->rd_att.data[0]->attrelid);

    pg_type_scan =  ambeginscan(pg_type_desc,
				0,
				NowTimeQual,
				1,
				&key);

    /* ----------------
     *	use amgetnext() to fetch the pg_type tuple.  If this
     *  tuple is not valid then something's wrong.
     * ----------------
     */
    tup = amgetnext(pg_type_scan, 0, (Buffer *)NULL);

    if (! PointerIsValid(tup)) {
	amendscan(pg_type_scan);
	amclose(pg_type_desc);
	elog(WARN, "DeletePgTypeTuple: %s type nonexistent",
	     &rdesc->rd_rel->relname);
    }

    /* ----------------
     *	now scan pg_attribute.  if any other relations have
     *  attributes of the type of the relation we are deleteing
     *  then we have to disallow the deletion.  should talk to
     *  stonebraker about this.  -cim 6/19/90
     * ----------------
     */
    typoid = tup->t_oid;

    pg_attribute_desc = amopenr(AttributeRelationName);

	ScanKeyEntryInitialize(&attkey, NULL, Anum_pg_attribute_atttypid, F_INT4EQ,
						   typoid);

    pg_attribute_scan = ambeginscan(pg_attribute_desc,
				    0,
				    NowTimeQual,
				    1,
				    &attkey);

    /* ----------------
     *	try and get a pg_attribute tuple.  if we succeed it means
     *  we cant delete the relation because something depends on
     *  the schema.
     * ----------------
     */
    atttup = amgetnext(pg_attribute_scan, 0, (Buffer *)NULL);

    if (PointerIsValid(atttup)) {
	ObjectId relid = ((AttributeTupleForm) GETSTRUCT(atttup))->attrelid;
	
	amendscan(pg_type_scan);
	amclose(pg_type_desc);
	amendscan(pg_attribute_scan);
	amclose(pg_attribute_desc);
	
	elog(WARN, "DeletePgTypeTuple: att of type %s exists in relation %d",
	     &rdesc->rd_rel->relname, relid);	
    }
    amendscan(pg_attribute_scan);
    amclose(pg_attribute_desc);

    /* ----------------
     *  Ok, it's safe so we delete the relation tuple
     *  from pg_type and finish up.  But first end the scan so that
     *  we release the read lock on pg_type.  -mer 13 Aug 1991
     * ----------------
     */
    amendscan(pg_type_scan);
    amdelete(pg_type_desc, &tup->t_ctid);

    amclose(pg_type_desc);
}

/* --------------------------------
 *	heap_destroy
 *
 * --------------------------------
 */
void
heap_destroy(relname)
    char	relname[];
{
    Relation	rdesc;
/*     int		issystem(); */

    /* ----------------
     *	first open the relation.  if the relation does exist,
     *  amopenr() returns NULL.
     * ----------------
     */
    rdesc = amopenr(relname);
    if (rdesc == NULL)
	elog(WARN,"Relation %s Does Not Exist!", relname);

    /* ----------------
     *	prevent deletion of system relations
     * ----------------
     */
    if (issystem((char*)(RelationGetRelationName(rdesc))) || 0 /* is not owned etc. */)
	elog(WARN, "amdestroy: cannot destroy %s relation",
	     &rdesc->rd_rel->relname);

    /* ----------------
     *	remove inheritance information
     * ----------------
     */
    RelationRemoveInheritance(rdesc);

    /* ----------------
     *	remove indexes if necessary
     * ----------------
     */
    if (rdesc->rd_rel->relhasindex) {
	RelationRemoveIndexes(rdesc);
    }

    /* ----------------
     *	delete attribute tuples
     * ----------------
     */
    DeletePgAttributeTuples(rdesc);

    /* ----------------
     *	delete type tuple.  here we want to see the effects
     *  of the deletions we just did, so we use setheapoverride().
     * ----------------
     */
    setheapoverride(true);
    DeletePgTypeTuple(rdesc);
    setheapoverride(false);

    /* ----------------
     *	delete relation tuple
     * ----------------
     */
    DeletePgRelationTuple(rdesc);

    /* ----------------
     *	unlink the relation and finish up.
     * ----------------
     */
    (void) smgrunlink(rdesc->rd_rel->relsmgr, rdesc);
    amclose(rdesc);
}
