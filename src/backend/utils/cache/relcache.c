/* ----------------------------------------------------------------
 *   FILE
 *	relcache.c 
 *	
 *   DESCRIPTION
 *	POSTGRES relation descriptor cache code
 *
 *   INTERFACE ROUTINES
 *	RelationInitialize		- initialize relcache
 *	RelationIdCacheGetRelation	- get a reldesc from the cache (id)
 *	RelationNameCacheGetRelation	- get a reldesc from the cache (name)
 *	RelationIdGetRelation		- get a reldesc by relation id
 *	RelationNameGetRelation		- get a reldesc by relation name
 *	RelationClose			- close an open relation
 *	RelationFlushRelation		- flush relation information
 *
 *   NOTES
 *	This file is in the process of being cleaned up
 *	before I add system attribute indexing.  -cim 1/13/91
 *
 *	The following code contains many undocumented hacks.  Please be
 *	careful....
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */
#include <errno.h>
#include <sys/file.h>
#include <strings.h>
 
/* XXX check above includes */
 
#include "tmp/postgres.h"
#include "tmp/miscadmin.h"

RcsId("$Header$");
 
#include "access/att.h"
#include "access/attnum.h"
#include "access/genam.h"
#include "access/heapam.h"
#include "access/htup.h"
#include "access/istrat.h"
#include "access/itup.h"
#include "access/skey.h"
#include "access/tqual.h"	/* for NowTimeQual */
#include "access/tupdesc.h"
 
#include "rules/rlock.h"
#include "storage/buf.h"
#include "storage/lmgr.h"
 
#include "tmp/hasht.h"
 
#include "utils/memutils.h"
#include "utils/log.h"
#include "utils/mcxt.h"
#include "utils/rel.h"
#include "utils/hsearch.h"
 
#include "catalog/catname.h"
#include "catalog/syscache.h"

#include "catalog/pg_attribute.h"
#include "catalog/pg_aggregate.h"
#include "catalog/pg_index.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_relation.h"
#include "catalog/pg_type.h"

#include "catalog/pg_variable.h"
#include "catalog/pg_log.h"
#include "catalog/pg_time.h"
#include "catalog/indexing.h"

#include "utils/relcache.h"

#ifdef sprite
#include "sprite_file.h"
#else
#include "storage/fd.h"
#endif /* sprite */
 
/* ----------------
 *	defines
 * ----------------
 */
#define private static
#define INIT_FILENAME	"pg_internal.init"

/* ----------------
 *	externs
 * ----------------
 */
extern bool	AMI_OVERRIDE;	/* XXX style */

/* ----------------
 *	hardcoded tuple descriptors.  see lib/H/catalog/pg_attribute.h
 * ----------------
 */
SCHEMA_DEF(pg_relation);
SCHEMA_DEF(pg_attribute);
SCHEMA_DEF(pg_proc);
SCHEMA_DEF(pg_type);
SCHEMA_DEF(pg_variable);
SCHEMA_DEF(pg_log);
SCHEMA_DEF(pg_time);
 
/* ----------------
 *	global variables
 *
 * 	Relations are cached two ways, by name and by id,
 *	thus there are two hash tables for referencing them. 
 * ----------------
 */
HTAB 	*RelationNameCache;
HTAB	*RelationIdCache;

/* ----------------
 *	RelationBuildDescInfo exists so code can be shared
 *      between RelationIdGetRelation() and RelationNameGetRelation()
 * ----------------
 */
typedef struct RelationBuildDescInfo {
    int infotype;		/* lookup by id or by name */
#define INFO_RELID 1
#define INFO_RELNAME 2
    union {
	ObjectId info_id;	/* relation object id */
	Name	 info_name;	/* relation name */
    } i;
} RelationBuildDescInfo;

typedef struct relidcacheent {
    ObjectId reloid;
    Relation reldesc;
} RelIdCacheEnt;
typedef struct relnamecacheent {
    NameData relname;
    Relation reldesc;
} RelNameCacheEnt;

void init_irels();
void write_irels();
HeapTuple scan_pg_rel_seq ARGS((RelationBuildDescInfo buildinfo));
HeapTuple scan_pg_rel_ind ARGS((RelationBuildDescInfo buildinfo));

char *BuildDescInfoError ARGS((RelationBuildDescInfo buildinfo ));
HeapTuple ScanPgRelation ARGS((RelationBuildDescInfo buildinfo ));
void RelationBuildTupleDesc ARGS((
	RelationBuildDescInfo buildinfo,
	Relation relation,
	AttributeTupleForm attp,
	u_int natts
));
void build_tupdesc_seq ARGS((
	RelationBuildDescInfo buildinfo,
	Relation relation,
	AttributeTupleForm attp,
	u_int natts
));
void build_tupdesc_ind ARGS((
	RelationBuildDescInfo buildinfo,
	Relation relation,
	AttributeTupleForm attp,
	u_int natts
));
Relation RelationBuildDesc ARGS((RelationBuildDescInfo buildinfo ));
private void formrdesc ARGS((
	char *relationName,
	u_int natts,
	AttributeTupleFormData att []
));
private void RelationFlushIndexes ARGS((
	Relation relation,
	ObjectId accessMethodId
));
 
/* -----------------
 *	macros to manipulate name cache and id cache
 * -----------------
 */
#define RelationCacheInsert(RELATION)	\
    {   RelIdCacheEnt *idhentry; RelNameCacheEnt *namehentry; \
	Name relname; ObjectId reloid; Boolean found; \
	relname = &(RELATION->rd_rel->relname); \
	namehentry = (RelNameCacheEnt*)hash_search(RelationNameCache, \
					           &relname->data[0], \
						   HASH_ENTER, \
						   &found); \
	if (namehentry == NULL) { \
	    elog(FATAL, "can't insert into relation descriptor cache"); \
	  } \
	if (found && !IsBootstrapProcessingMode()) { \
	    /* used to give notice -- now just keep quiet */ ; \
	  } \
	namehentry->reldesc = RELATION; \
	reloid = RELATION->rd_id; \
	idhentry = (RelIdCacheEnt*)hash_search(RelationIdCache, \
					       (char *)&reloid, \
					       HASH_ENTER, \
					       &found); \
	if (idhentry == NULL) { \
	    elog(FATAL, "can't insert into relation descriptor cache"); \
	  } \
	if (found && !IsBootstrapProcessingMode()) { \
	    /* used to give notice -- now just keep quiet */ ; \
	  } \
	idhentry->reldesc = RELATION; \
    }
#define RelationNameCacheLookup(NAME, RELATION)	\
    {   RelNameCacheEnt *hentry; Boolean found; \
	hentry = (RelNameCacheEnt*)hash_search(RelationNameCache, \
					       (char *)NAME,HASH_FIND,&found); \
	if (hentry == NULL) { \
	    elog(FATAL, "error in CACHE"); \
	  } \
	if (found) { \
	    RELATION = hentry->reldesc; \
	  } \
	else { \
	    RELATION = NULL; \
	  } \
    }
#define RelationIdCacheLookup(ID, RELATION)	\
    {   RelIdCacheEnt *hentry; Boolean found; \
	hentry = (RelIdCacheEnt*)hash_search(RelationIdCache, \
					     (char *)&(ID),HASH_FIND, &found); \
	if (hentry == NULL) { \
	    elog(FATAL, "error in CACHE"); \
	  } \
	if (found) { \
	    RELATION = hentry->reldesc; \
	  } \
	else { \
	    RELATION = NULL; \
	  } \
    }
#define RelationCacheDelete(RELATION)	\
    {   RelNameCacheEnt *namehentry; RelIdCacheEnt *idhentry; \
	Name relname; ObjectId reloid; Boolean found; \
	relname = &(RELATION->rd_rel->relname); \
	namehentry = (RelNameCacheEnt*)hash_search(RelationNameCache, \
					           &relname->data[0], \
						   HASH_REMOVE, \
						   &found); \
	if (namehentry == NULL) { \
	    elog(FATAL, "can't delete from relation descriptor cache"); \
	  } \
	if (!found) { \
	    elog(NOTICE, "trying to delete a reldesc that does not exist."); \
	  } \
	reloid = RELATION->rd_id; \
	idhentry = (RelIdCacheEnt*)hash_search(RelationIdCache, \
					       (char *)&reloid, \
					       HASH_REMOVE, &found); \
	if (idhentry == NULL) { \
	    elog(FATAL, "can't delete from relation descriptor cache"); \
	  } \
	if (!found) { \
	    elog(NOTICE, "trying to delete a reldesc that does not exist."); \
	  } \
    }

/* ----------------------------------------------------------------
 *	RelationIdGetRelation() and RelationNameGetRelation()
 *			support functions
 * ----------------------------------------------------------------
 */
 
/* --------------------------------
 *	BuildDescInfoError returns a string appropriate to
 *	the buildinfo passed to it
 * --------------------------------
 */
 
char *
BuildDescInfoError(buildinfo)
    RelationBuildDescInfo buildinfo;
{
    static char errBuf[64];

    bzero(errBuf, sizeof(errBuf));
    switch(buildinfo.infotype) {
    case INFO_RELID:
	sprintf(errBuf, "(relation id %d)", buildinfo.i.info_id);
	break;
    case INFO_RELNAME:
	sprintf(errBuf, "(relation name %s)", buildinfo.i.info_name);
	break;
    }

    return errBuf;
}
 
/* --------------------------------
 *	ScanPgRelation
 *
 *	this is used by RelationBuildDesc to find a pg_relation
 *	tuple matching either a relation name or a relation id
 *	as specified in buildinfo.
 * --------------------------------
 */
HeapTuple
ScanPgRelation(buildinfo)   
    RelationBuildDescInfo buildinfo;    
{
    /*
     *  If this is bootstrap time (initdb), then we can't use the system
     *  catalog indices, because they may not exist yet.  Otherwise, we
     *  can, and do.
     */

    if (IsBootstrapProcessingMode())
	return (scan_pg_rel_seq(buildinfo));
    else
	return (scan_pg_rel_ind(buildinfo));
}

HeapTuple
scan_pg_rel_seq(buildinfo)
    RelationBuildDescInfo buildinfo;
{
    HeapTuple	 pg_relation_tuple;
    HeapTuple	 return_tuple;
    Relation	 pg_relation_desc;
    HeapScanDesc pg_relation_scan;
    ScanKeyData	 key;
    Buffer	 buf;
    
    /* ----------------
     *	form a scan key
     * ----------------
     */
    switch (buildinfo.infotype) {
    case INFO_RELID:
	ScanKeyEntryInitialize(&key.data[0], 0,
						   ObjectIdAttributeNumber,
						   ObjectIdEqualRegProcedure,
						   ObjectIdGetDatum(buildinfo.i.info_id));
	break;

    case INFO_RELNAME:
	ScanKeyEntryInitialize(&key.data[0], 0,
	                       RelationNameAttributeNumber,
	                       Character16EqualRegProcedure,
	                       NameGetDatum(buildinfo.i.info_name));
	break;

    default:
	elog(WARN, "ScanPgRelation: bad buildinfo");
	return NULL;
	break;
    }
    
    /* ----------------
     *	open pg_relation and fetch a tuple
     * ----------------
     */
    pg_relation_desc =  heap_openr(RelationRelationName);
    if (!IsInitProcessingMode())
	RelationSetLockForRead(pg_relation_desc);
    pg_relation_scan =
	heap_beginscan(pg_relation_desc, 0, NowTimeQual, 1, &key);
    pg_relation_tuple = heap_getnext(pg_relation_scan, 0, &buf);

    /* ----------------
     *	get set to return tuple
     * ----------------
     */
    if (! HeapTupleIsValid(pg_relation_tuple)) {
	return_tuple = pg_relation_tuple;
    } else {
	/* ------------------
	 *  a satanic bug used to live here: pg_relation_tuple used to be
	 *  returned here without having the corresponding buffer pinned.
	 *  so when the buffer gets replaced, all hell breaks loose.
	 *  this bug is discovered and killed by wei on 9/27/91.
	 * -------------------
	 */
	return_tuple = (HeapTuple)palloc(pg_relation_tuple->t_len);
	bcopy(pg_relation_tuple, return_tuple, pg_relation_tuple->t_len);
	ReleaseBuffer(buf);
    }

    /* all done */
    heap_endscan(pg_relation_scan);
    if (!IsInitProcessingMode())
	RelationUnsetLockForRead(pg_relation_desc);
    heap_close(pg_relation_desc);

    return return_tuple;
}

HeapTuple
scan_pg_rel_ind(buildinfo)
    RelationBuildDescInfo buildinfo;
{
    Relation pg_class_desc;
    HeapTuple return_tuple;

    pg_class_desc = heap_openr(Name_pg_relation);
    if (!IsInitProcessingMode())
	RelationSetLockForRead(pg_class_desc);

    switch (buildinfo.infotype) {
    case INFO_RELID:
	return_tuple = ClassOidIndexScan(pg_class_desc, buildinfo.i.info_id);
	break;

    case INFO_RELNAME:
	return_tuple = ClassNameIndexScan(pg_class_desc, buildinfo.i.info_name);
	break;

    default:
	elog(WARN, "ScanPgRelation: bad buildinfo");
	/* NOTREACHED */
    }

    /* all done */
    if (!IsInitProcessingMode())
	RelationUnsetLockForRead(pg_class_desc);
    heap_close(pg_class_desc);

    return return_tuple;
}

/* ----------------
 *	AllocateRelationDesc
 *
 *	This is used to allocate memory for a new relation descriptor
 *	and initialize the rd_rel field.
 * ----------------
 */
Relation
AllocateRelationDesc(natts, relp)
    u_int		natts;
    RelationTupleForm	relp;
{
    Relation 		relation;
    int			len;
    RelationTupleForm   relationTupleForm;
    
    /* ----------------
     *  allocate space for the relation tuple form
     * ----------------
     */
    relationTupleForm = (RelationTupleForm)
	palloc(sizeof (RuleLock) + sizeof *relation->rd_rel);
    
    bcopy((char *)relp, (char *)relationTupleForm, sizeof *relp);
    
    /* ----------------
     *	allocate space for new relation descriptor, including
     *  the tuple descriptor and the index strategy and support pointers
     * ----------------
     */
    len = sizeof(RelationData) + 
	((int)natts - 1) * sizeof(relation->rd_att) + /* var len struct */
	    sizeof(IndexStrategy)
	    + sizeof(RegProcedure *) + 10;	/* + 10 is voodoo XXX mao */
    
    relation = (Relation) palloc(len);
    
    /* ----------------
     *	clear new reldesc and initialize relation tuple form
     * ----------------
     */
    bzero((char *)relation, len);
    relation->rd_rel = relationTupleForm;

    return relation;
}

/* --------------------------------
 *	RelationBuildTupleDesc
 *
 *	Form the relation's tuple descriptor from information in
 *	the pg_attribute system catalog.
 * --------------------------------
 */
void
RelationBuildTupleDesc(buildinfo, relation, attp, natts) 
    RelationBuildDescInfo buildinfo;    
    Relation		  relation;
    AttributeTupleForm	  attp;
    u_int		  natts;
{
    /*
     *  If this is bootstrap time (initdb), then we can't use the system
     *  catalog indices, because they may not exist yet.  Otherwise, we
     *  can, and do.
     */

    if (IsBootstrapProcessingMode())
	build_tupdesc_seq(buildinfo, relation, attp, natts);
    else
	build_tupdesc_ind(buildinfo, relation, attp, natts);
}

void
build_tupdesc_seq(buildinfo, relation, attp, natts) 
    RelationBuildDescInfo buildinfo;    
    Relation		  relation;
    AttributeTupleForm	  attp;
    u_int		  natts;
{
    HeapTuple    pg_attribute_tuple;
    Relation	 pg_attribute_desc;
    HeapScanDesc pg_attribute_scan;
    ScanKeyData	 key;
    int		 i;
    int		 need;

    /* ----------------
     *	form a scan key
     * ----------------
     */
    ScanKeyEntryInitialize(&key.data[0], 0, 
                           AttributeRelationIdAttributeNumber,
                           ObjectIdEqualRegProcedure,
                           ObjectIdGetDatum(relation->rd_id));
    
    /* ----------------
     *	open pg_attribute and begin a scan
     * ----------------
     */
    pg_attribute_desc = heap_openr(AttributeRelationName);
    pg_attribute_scan =
	heap_beginscan(pg_attribute_desc, 0, NowTimeQual, 1, &key);
    
    /* ----------------
     *	add attribute data to relation->rd_att
     * ----------------
     */
    need = natts;
    pg_attribute_tuple = heap_getnext(pg_attribute_scan, 0, (Buffer *) NULL);
    while (HeapTupleIsValid(pg_attribute_tuple) && need > 0) {

	attp = (AttributeTupleForm)
	    HeapTupleGetForm(pg_attribute_tuple);

	if (attp->attnum > 0) {

	    relation->rd_att.data[attp->attnum - 1] = (Attribute)
		palloc(sizeof (RuleLock) + sizeof *relation->rd_att.data[0]);

	    bcopy((char *) attp,
		  (char *) relation->rd_att.data[attp->attnum - 1],
		  sizeof *relation->rd_att.data[0]);

	    need--;
	}
	pg_attribute_tuple = heap_getnext(pg_attribute_scan,
					  0, (Buffer *) NULL);
    }

    if (need > 0)
	elog(WARN, "catalog is missing %d attribute%s for relid %d",
		need, (need == 1 ? "" : "s"), relation->rd_id);

    /* ----------------
     *	end the scan and close the attribute relation
     * ----------------
     */
    heap_endscan(pg_attribute_scan);
    heap_close(pg_attribute_desc);
}

void
build_tupdesc_ind(buildinfo, relation, attp, natts) 
    RelationBuildDescInfo buildinfo;    
    Relation		  relation;
    AttributeTupleForm	  attp;
    u_int		  natts;
{
    Relation attrel;
    HeapTuple atttup;
    int i;

    attrel = heap_openr(Name_pg_attribute);

    for (i = 1; i <= relation->rd_rel->relnatts; i++) {

	atttup = (HeapTuple) AttributeNumIndexScan(attrel, relation->rd_id, i);

	if (!HeapTupleIsValid(atttup))
	    elog(WARN, "cannot find attribute %d of relation %.16s", i,
			&(relation->rd_rel->relname.data[0]));

	attp = (AttributeTupleForm) HeapTupleGetForm(atttup);

	relation->rd_att.data[i - 1] = (Attribute)
	    palloc(sizeof (RuleLock) + sizeof *relation->rd_att.data[0]);

	bcopy((char *) attp,
	      (char *) relation->rd_att.data[i - 1],
	      sizeof *relation->rd_att.data[0]);
    }

    heap_close(attrel);
}

/* --------------------------------
 *	RelationBuildDesc
 *	
 *	To build a relation descriptor, we have to allocate space,
 *	open the underlying unix file and initialize the following
 *	fields:
 *
 *  File		   rd_fd;	 open file descriptor
 *  int                    rd_nblocks;   number of blocks in rel 
 *					 it will be set in ambeginscan()
 *  uint16		   rd_refcnt;	 reference count
 *  AccessMethodTupleForm  rd_am;	 AM tuple
 *  RelationTupleForm	   rd_rel;	 RELATION tuple
 *  ObjectId		   rd_id;	 relations's object id 
 *  Pointer		   lockInfo;	 ptr. to misc. info.
 *  TupleDescriptorData	   rd_att;	 tuple desciptor
 *
 *	Note: rd_ismem (rel is in-memory only) is currently unused
 *      by any part of the system.  someday this will indicate that
 *	the relation lives only in the main-memory buffer pool
 *	-cim 2/4/91
 * --------------------------------
 */
Relation
RelationBuildDesc(buildinfo)
    RelationBuildDescInfo buildinfo;
{
    File		fd;
    Relation		relation;
    u_int		natts;
    ObjectId		relid;
    ObjectId		relam;
    RelationTupleForm	relp;
    AttributeTupleForm	attp = NULL;
    ObjectId		attrioid; /* attribute relation index relation oid */

    extern GlobalMemory CacheCxt;
    MemoryContext	oldcxt;
    
    HeapTuple		pg_relation_tuple;
    
    oldcxt = MemoryContextSwitchTo((MemoryContext)CacheCxt);

    /* ----------------
     *	find the tuple in pg_relation corresponding to the given relation id
     * ----------------
     */
    pg_relation_tuple = ScanPgRelation(buildinfo);

    /* ----------------
     *	if no such tuple exists, return NULL
     * ----------------
     */
    if (! HeapTupleIsValid(pg_relation_tuple)) {
	
	MemoryContextSwitchTo(oldcxt); 
	 
	return NULL;
    }

    /* ----------------
     *	get information from the pg_relation_tuple
     * ----------------
     */
    relid = pg_relation_tuple->t_oid;
    relp = (RelationTupleForm) GETSTRUCT(pg_relation_tuple);
    natts = relp->relnatts;
    
    /* ----------------
     *	allocate storage for the relation descriptor,
     *  initialize relation->rd_rel and get the access method id.
     * ----------------
     */
    relation = AllocateRelationDesc(natts, relp);
    relam = relation->rd_rel->relam;
    
    /* ----------------
     *	initialize the relation's relation id (relation->rd_id)
     * ----------------
     */
    relation->rd_id = relid;

    /* ----------------
     *	initialize relation->rd_refcnt
     * ----------------
     */
    RelationSetReferenceCount(relation, 1);
    
    /* ----------------
     *   normal relations are not nailed into the cache
     * ----------------
     */
    relation->rd_isnailed = false;

    /* ----------------
     *	initialize the access method information (relation->rd_am)
     * ----------------
     */
    if (ObjectIdIsValid(relam)) {
	relation->rd_am = (AccessMethodTupleForm)
	    AccessMethodObjectIdGetAccessMethodTupleForm(relam);
    }

    /* ----------------
     *	initialize the tuple descriptor (relation->rd_att).
     *  remember, rd_att is an array of attribute pointers that lives
     *  off the end of the relation descriptor structure so space was
     *  already allocated for it by AllocateRelationDesc.
     * ----------------
     */
    RelationBuildTupleDesc(buildinfo, relation, attp, natts);

    /* ----------------
     *	initialize index strategy and support information for this relation
     * ----------------
     */
    if (ObjectIdIsValid(relam)) {
	IndexedAccessMethodInitialize(relation);
    }

    /* ----------------
     *	initialize the relation lock manager information
     * ----------------
     */
    RelationInitLockInfo(relation); /* see lmgr.c */
    
    /* ----------------
     *	open the relation and assign the file descriptor returned
     *  by the storage manager code to rd_fd.
     * ----------------
     */
    fd = smgropen(relp->relsmgr, relation);
    
    Assert (fd >= -1);
    if (fd == -1)
	elog(NOTICE, "RelationIdBuildRelation: smgropen(%s): %m",
	     &relp->relname);
    
    relation->rd_fd = fd;
    
    /* ----------------
     *	insert newly created relation into proper relcaches,
     *  restore memory context and return the new reldesc.
     * ----------------
     */
    
    RelationCacheInsert(relation);

    /* -------------------
     *  free the memory allocated for pg_relation_tuple
     * -------------------
     */
    pfree(pg_relation_tuple);

    MemoryContextSwitchTo(oldcxt);
    
    return relation;
}

IndexedAccessMethodInitialize(relation)
    Relation relation;
{
    IndexStrategy 	strategy;
    RegProcedure	*support;
    int			natts;
    int 		stratSize;
    int			supportSize;
    uint16 		relamstrategies;
    uint16		relamsupport;
    
    natts = relation->rd_rel->relnatts;
    relamstrategies = relation->rd_am->amstrategies;
    stratSize = AttributeNumberGetIndexStrategySize(natts, relamstrategies);
    strategy = (IndexStrategy) palloc(stratSize);
    relamsupport = relation->rd_am->amsupport;

    if (relamsupport > 0) {
	supportSize = natts * (relamsupport * sizeof (RegProcedure));
	support = (RegProcedure *) palloc(supportSize);
    } else {
	support = (RegProcedure *) NULL;
    }

    IndexSupportInitialize(strategy, support,
    			    relation->rd_att.data[0]->attrelid,
    			    relation->rd_rel->relam,
			    relamstrategies, relamsupport, natts);

    RelationSetIndexSupport(relation, strategy, support);
}

/* --------------------------------
 *	formrdesc
 *
 *	This is a special version of RelationBuildDesc()
 *	used by RelationInitialize() in initializing the
 *	relcache.  The system relation descriptors built
 *	here are all nailed in the descriptor caches, for
 *	bootstraping.
 * --------------------------------
 */
private void
formrdesc(relationName, natts, att)
    char		*relationName;
    u_int		natts;
    AttributeTupleFormData att[];
{
    Relation	relation;
    int		fd;
    int		len;
    int		i;
    char	*relpath();
    Relation	publicCopy;
    
    /* ----------------
     *	allocate new relation desc
     * ----------------
     */
    len = sizeof *relation + ((int)natts - 1) * sizeof relation->rd_att;
    relation = (Relation) palloc(len);
    bzero((char *)relation, len);
    
    /* ----------------
     *	don't open the unix file yet..
     * ----------------
     */
    relation->rd_fd = -1;
    
    /* ----------------
     *	initialize reference count
     * ----------------
     */
    RelationSetReferenceCount(relation, 1);

    /* ----------------
     *	initialize relation tuple form
     * ----------------
     */
    relation->rd_rel = (RelationTupleForm)
	palloc(sizeof (RuleLock) + sizeof *relation->rd_rel);

    bzero(relation->rd_rel, sizeof(struct RelationTupleFormD));

    strncpy(&relation->rd_rel->relname, relationName, sizeof(NameData));

    /*
     *  For debugging purposes, it's important to distinguish between
     *  shared and non-shared relations, even at bootstrap time.  There's
     *  code in the buffer manager that traces allocations that has to
     *  know about this.
     */

    if (issystem(relationName)) {
	relation->rd_rel->relowner = 6;			/* XXX use sym const */
	relation->rd_rel->relisshared =
		NameIsSharedSystemRelationName((Name)relationName);
    } else {
	relation->rd_rel->relowner = InvalidObjectId;	/* XXX incorrect*/
	relation->rd_rel->relisshared = false;
    }

    relation->rd_rel->relpages = 1;			/* XXX */
    relation->rd_rel->reltuples = 1;			/* XXX */
    relation->rd_rel->relkind = 'r';
    relation->rd_rel->relarch = 'n';
    relation->rd_rel->relnatts = (uint16) natts;
    relation->rd_isnailed = true;

    /* ----------------
     *	initialize tuple desc info
     * ----------------
     */
    for (i = 0; i < natts; i++) {
	relation->rd_att.data[i] = (Attribute)
	    palloc(sizeof (RuleLock) + sizeof *relation->rd_att.data[0]);
	
	bzero((char *)relation->rd_att.data[i], sizeof (RuleLock) +
	      sizeof *relation->rd_att.data[0]);
	bcopy((char *)&att[i], (char *)relation->rd_att.data[i],
	      sizeof *relation->rd_att.data[0]);
    }
    
    /* ----------------
     *	initialize relation id
     * ----------------
     */
    relation->rd_id = relation->rd_att.data[0]->attrelid;
    
    /* ----------------
     *	add new reldesc to relcache
     * ----------------
     */
    RelationCacheInsert(relation);
    /*
     * Determining this requires a scan on pg_relation, but to do the
     * scan the rdesc for pg_relation must already exist.  Therefore
     * we must do the check (and possible set) after cache insertion.
     */
    relation->rd_rel->relhasindex =
	CatalogHasIndex(relationName, relation->rd_id);
}
 

/* ----------------------------------------------------------------
 *		 Relation Descriptor Lookup Interface
 * ----------------------------------------------------------------
 */

/* --------------------------------
 *	RelationIdCacheGetRelation
 *
 *	only try to get the reldesc by looking up the cache
 *	do not go to the disk.  this is used by BlockPrepareFile()
 *	and RelationIdGetRelation below.
 * --------------------------------
 */
Relation
RelationIdCacheGetRelation(relationId)
    ObjectId	relationId;
{
    Relation	rd;

    RelationIdCacheLookup(relationId, rd);
    
    if (RelationIsValid(rd)) {
	if (rd->rd_fd == -1) {
	    rd->rd_fd = smgropen(rd->rd_rel->relsmgr, rd);
	    Assert(rd->rd_fd != -1);
	}
	
	RelationIncrementReferenceCount(rd);
	RelationSetLockForDescriptorOpen(rd);
	
    }
    
    return(rd);
}

/* --------------------------------
 *	RelationNameCacheGetRelation
 * --------------------------------
 */
Relation
RelationNameCacheGetRelation(relationName)
    Name	relationName;
{
    Relation	rd;
    NameData	name;

    /* make sure that the name key used for hash lookup is properly
       null-padded */
    bzero(&name, sizeof(NameData));
    strncpy(&name, relationName, sizeof(NameData));
    RelationNameCacheLookup(&name, rd);
    
    if (RelationIsValid(rd)) {
	if (rd->rd_fd == -1) {
	    rd->rd_fd = smgropen(rd->rd_rel->relsmgr, rd);
	    Assert(rd->rd_fd != -1);
	}
	
	RelationIncrementReferenceCount(rd);
	RelationSetLockForDescriptorOpen(rd);
	
    }
    
    return(rd);
}

/* --------------------------------
 *	RelationIdGetRelation
 *
 *	return a relation descriptor based on its id.
 *	return a cached value if possible
 * --------------------------------
 */
Relation
RelationIdGetRelation(relationId)
    ObjectId	relationId;
{
    Relation		  rd;
    RelationBuildDescInfo buildinfo;
    
    /* ----------------
     *	increment access statistics
     * ----------------
     */
    IncrHeapAccessStat(local_RelationIdGetRelation);
    IncrHeapAccessStat(global_RelationIdGetRelation);

    /* ----------------
     *	first try and get a reldesc from the cache
     * ----------------
     */
    rd = RelationIdCacheGetRelation(relationId);
    if (RelationIsValid(rd))
       return rd;
    
    /* ----------------
     *	no reldesc in the cache, so have RelationBuildDesc()
     *  build one and add it.
     * ----------------
     */
    buildinfo.infotype =  INFO_RELID;
    buildinfo.i.info_id = relationId;
    
    rd = RelationBuildDesc(buildinfo);
    return
	rd;
}

/* --------------------------------
 *	RelationNameGetRelation
 *
 *	return a relation descriptor based on its name.
 *	return a cached value if possible
 * --------------------------------
 */
Relation
RelationNameGetRelation(relationName)
    Name		relationName;
{
    Relation		  rd;
    RelationBuildDescInfo buildinfo;
    
    /* ----------------
     *	increment access statistics
     * ----------------
     */
    IncrHeapAccessStat(local_RelationNameGetRelation);
    IncrHeapAccessStat(global_RelationNameGetRelation);

    /* ----------------
     *	first try and get a reldesc from the cache
     * ----------------
     */
    rd = RelationNameCacheGetRelation(relationName);
    if (RelationIsValid(rd))
       return rd;
    
    /* ----------------
     *	no reldesc in the cache, so have RelationBuildDesc()
     *  build one and add it.
     * ----------------
     */
    buildinfo.infotype =    INFO_RELNAME;
    buildinfo.i.info_name = relationName;
    
    rd = RelationBuildDesc(buildinfo);
    return
	rd;
}

/* ----------------
 *	old "getreldesc" interface.
 * ----------------
 */
Relation
getreldesc(relationName)
    Name  relationName;
{
    /* ----------------
     *	increment access statistics
     * ----------------
     */
    IncrHeapAccessStat(local_getreldesc);
    IncrHeapAccessStat(global_getreldesc);
    
    return (Relation)
	RelationNameGetRelation(relationName);
}

/* ----------------------------------------------------------------
 *		cache invalidation support routines
 * ----------------------------------------------------------------
 */
 
/* --------------------------------
 *	RelationClose - close an open relation
 * --------------------------------
 */
void
RelationClose(relation)
    Relation	relation;
{
    /* Note: no locking manipulations needed */
    RelationDecrementReferenceCount(relation);
}

/* --------------------------------
 *	RelationFlushRelation
 * --------------------------------
 */
/**** xxref:
 *           RelationIdInvalidateRelationCacheByRelationId
 *           RelationFlushIndexes
 ****/
void
RelationFlushRelation(relation, onlyFlushReferenceCountZero)
    Relation	relation;
    bool	onlyFlushReferenceCountZero;
{
    int			i;
    Attribute		*p;
    MemoryContext	oldcxt;
    
    if (relation->rd_isnailed) {
	/* this is a nailed special relation for bootstraping */
	return;
    }
    
    if (!onlyFlushReferenceCountZero || 
	RelationHasReferenceCountZero(relation)) {
	
	oldcxt = MemoryContextSwitchTo((MemoryContext)CacheCxt);
	
	RelationCacheDelete(relation);
	
	FileInvalidate(RelationGetSystemPort(relation));
	
	i = relation->rd_rel->relnatts - 1;
	p = &relation->rd_att.data[i];
	while ((i -= 1) >= 0) {
	    pfree((char *)*p--);
	}
	
	pfree((char *)RelationGetLockInfo(relation));
	pfree((char *)RelationGetRelationTupleForm(relation));
	pfree((char *)relation);
	
	MemoryContextSwitchTo(oldcxt);
    }
}
 
/* --------------------------------
 *	RelationIdInvalidateRelationCacheByRelationId
 * --------------------------------
 */
/**** xxref:
 *           CacheIdInvalidate
 ****/
void
RelationIdInvalidateRelationCacheByRelationId(relationId)
    ObjectId	relationId;
{
    Relation	relation;
    
    RelationIdCacheLookup(relationId, relation);
    
    if (PointerIsValid(relation)) {
	/* Assert(RelationIsValid(relation)); */
	/*
	 * The boolean onlyFlushReferenceCountZero in RelationFlushReln()
	 * should be set to true when we are incrementing the command
	 * counter and to false when we are starting a new xaction.  This
	 * can be determined by checking the current xaction status.
	 */
	RelationFlushRelation(relation, CurrentXactInProgress());
    }
}
 
/* --------------------------------
 *	RelationIdInvalidateRelationCacheByAccessMethodId
 *
 *	RelationFlushIndexes is needed for use with HashTableWalk..
 * --------------------------------
 */
private void
RelationFlushIndexes(r, accessMethodId)
    Relation	*r;
    ObjectId	accessMethodId;
{
    Relation relation = *r;

    if (!RelationIsValid(relation)) {
	elog(NOTICE, "inval call to RFI");
	return;
    }

    if (relation->rd_rel->relkind == 'i' &&	/* XXX style */
	(!ObjectIdIsValid(accessMethodId) ||
	 relation->rd_rel->relam == accessMethodId))
	{
	    RelationFlushRelation(relation, false);
	}
}

/**** xxref:
 *           CacheIdInvalidate
 ****/
void
RelationIdInvalidateRelationCacheByAccessMethodId(accessMethodId)
    ObjectId	accessMethodId;
{
# if 0
    /*
     *  25 aug 1992:  mao commented out the ht walk below.  it should be
     *  doing the right thing, in theory, but flushing reldescs for index
     *  relations apparently doesn't work.  we want to cut 4.0.1, and i
     *  don't want to introduce new bugs.  this code never executed before,
     *  so i'm turning it off for now.  after the release is cut, i'll
     *  fix this up.
     */

    HashTableWalk(RelationNameCache, RelationFlushIndexes,
		  accessMethodId);
# else
    return;
# endif
}
 
/* --------------------------------
 *	RelationCacheInvalidate
 * --------------------------------
 */
/**** xxref:
 *           ResetSystemCaches
 ****/
void
RelationCacheInvalidate(onlyFlushReferenceCountZero)
    bool onlyFlushReferenceCountZero;
{
    HashTableWalk(RelationNameCache, RelationFlushRelation, 
		  onlyFlushReferenceCountZero);
}
 
/* --------------------------------
 *	RelationRegisterRelation
 * --------------------------------
 */
/**** xxref:
 *           RelationRegisterTempRel
 ****/
void
RelationRegisterRelation(relation)
    Relation	relation;
{
    extern GlobalMemory CacheCxt;
    MemoryContext   	oldcxt;
    
    oldcxt = MemoryContextSwitchTo((MemoryContext)CacheCxt);
    
    if (oldcxt != (MemoryContext)CacheCxt) 
	elog(NOIND,"RelationRegisterRelation: WARNING: Context != CacheCxt");
    
    RelationCacheInsert(relation);
    
    RelationInitLockInfo(relation);
    
    MemoryContextSwitchTo(oldcxt);
}
 
/* -----------------------------------
 *  RelationRegisterTempRel
 *
 *  	Register a temporary relation created by another backend
 *      only called in parallel mode.
 * ----------------------------------
 */
/**** xxref:
 *           <apparently-unused>
 ****/
void
RelationRegisterTempRel(temprel)
    Relation temprel;
{
    extern GlobalMemory CacheCxt;
    MemoryContext   	oldcxt;
    Relation 		rd;
    
    /* ----------------------------------------
     *  see if the temprel is created by the current backend
     *  and therefore is already in the hast table
     * ----------------------------------------
     */
    RelationIdCacheLookup(temprel->rd_id, rd);
    
    if (RelationIsValid(rd))
	return;
    
    /* ----------------------------------------
     *  the temprel is created by another backend, insert it into the 
     *  hash table
     * ----------------------------------------
     */
    oldcxt = MemoryContextSwitchTo((MemoryContext)CacheCxt);
    
    RelationRegisterRelation(temprel);
    
    MemoryContextSwitchTo(oldcxt);
}
 
/* --------------------------------
 *	RelationInitialize
 *
 *	This initializes the relation descriptor cache.
 * --------------------------------
 */

#define INITRELCACHESIZE	400

void 
RelationInitialize()
{
    extern GlobalMemory		CacheCxt;
    MemoryContext		oldcxt;
    HASHCTL			ctl;

    /* ----------------
     *	switch to cache memory context
     * ----------------
     */
    if (!CacheCxt)
	CacheCxt = CreateGlobalMemory("Cache");
    
    oldcxt = MemoryContextSwitchTo((MemoryContext)CacheCxt);

    /* ----------------
     *	create global caches
     * ----------------
     */
    bzero(&ctl, sizeof(ctl));
    ctl.keysize = sizeof(NameData);
    ctl.datasize = sizeof(Relation);
    RelationNameCache = hash_create(INITRELCACHESIZE, &ctl, HASH_ELEM);
    
    ctl.keysize = sizeof(ObjectId);
    ctl.hash = tag_hash;
    RelationIdCache = hash_create(INITRELCACHESIZE, &ctl, 
				  HASH_ELEM | HASH_FUNCTION);
    
    /* ----------------
     *	initialize the cache with pre-made relation descriptors
     *  for some of the more important system relations.  These
     *  relations should always be in the cache.
     * ----------------
     */
#define INIT_RELDESC(x) \
    formrdesc(SCHEMA_NAME(x), SCHEMA_NATTS(x), SCHEMA_DESC(x))
    
    INIT_RELDESC(pg_relation);
    INIT_RELDESC(pg_attribute);
    INIT_RELDESC(pg_proc);
    INIT_RELDESC(pg_type);
    INIT_RELDESC(pg_variable);
    INIT_RELDESC(pg_log);
    INIT_RELDESC(pg_time);

    /*
     *  If this isn't initdb time, then we want to initialize some index
     *  relation descriptors, as well.  The descriptors are for pg_attnumind
     *  (to make building relation descriptors fast) and possibly others,
     *  as they're added.
     */

    if (!IsBootstrapProcessingMode())
	init_irels();

    MemoryContextSwitchTo(oldcxt);
}

/*
 *  init_irels(), write_irels() -- handle special-case initialization of
 *				   index relation descriptors.
 *
 *	In late 1992, we started regularly having databases with more than
 *	a thousand classes in them.  With this number of classes, it became
 *	critical to do indexed lookups on the system catalogs.
 *
 *	Bootstrapping these lookups is very hard.  We want to be able to
 *	use an index on pg_attribute, for example, but in order to do so,
 *	we must have read pg_attribute for the attributes in the index,
 *	which implies that we need to use the index.
 *
 *	In order to get around the problem, we do the following:
 *
 *	   +  When the database system is initialized (at initdb time), we
 *	      don't use indices on pg_attribute.  We do sequential scans.
 *
 *	   +  When the backend is started up in normal mode, we load an image
 *	      of the appropriate relation descriptors, in internal format,
 *	      from an initialization file in the data/base/... directory.
 *
 *	   +  If the initialization file isn't there, then we create the
 *	      relation descriptor using sequential scans and write it to
 *	      the initialization file for use by subsequent backends.
 *
 *	This is complicated and interferes with system changes, but
 *	performance is so bad that we're willing to pay the tax.
 */

/* pg_attnumind, pg_classnameind, pg_classoidind */
#define Num_indices_bootstrap	3

void
init_irels()
{
    int len;
    int nread;
    File fd;
    Relation irel[Num_indices_bootstrap];
    Relation ird;
    AccessMethodTupleForm am;
    RelationTupleForm relform;
    IndexStrategy strat;
    RegProcedure *support;
    int i;
    int relno;
    char *p;

    if ((fd = FileNameOpenFile(INIT_FILENAME, O_RDONLY, 0600)) < 0) {
	write_irels();
	return;
    }

    (void) FileSeek(fd, 0L, L_SET);

    for (relno = 0; relno < Num_indices_bootstrap; relno++) {
	/* first read the relation descriptor */
	if ((nread = FileRead(fd, &len, sizeof(int))) != sizeof(int)) {
	    write_irels();
	    return;
	}

	ird = irel[relno] = (Relation) palloc(len);
	if ((nread = FileRead(fd, ird, len)) != len) {
	    write_irels();
	    return;
	}

	/* the file descriptor is not yet opened */
	ird->rd_fd = -1;

	/* lock info is not initialized */
	ird->lockInfo = (char *) NULL;

	/* next read the access method tuple form */
	if ((nread = FileRead(fd, &len, sizeof(int))) != sizeof(int)) {
	    write_irels();
	    return;
	}

	am = (AccessMethodTupleForm) palloc(len);
	if ((nread = FileRead(fd, am, len)) != len) {
	    write_irels();
	    return;
	}

	ird->rd_am = am;

	/* next read the relation tuple form */
	if ((nread = FileRead(fd, &len, sizeof(int))) != sizeof(int)) {
	    write_irels();
	    return;
	}

	relform = (RelationTupleForm) palloc(len);
	if ((nread = FileRead(fd, relform, len)) != len) {
	    write_irels();
	    return;
	}

	ird->rd_rel = relform;

	/* next read all the attribute tuple form data entries */
	len = sizeof(RuleLock) + sizeof(*ird->rd_att.data[0]);
	for (i = 0; i < relform->relnatts; i++) {
	    if ((nread = FileRead(fd, &len, sizeof(int))) != sizeof(int)) {
		write_irels();
		return;
	    }

	    ird->rd_att.data[i] = (AttributeTupleForm) palloc(len);

	    if ((nread = FileRead(fd, ird->rd_att.data[i], len)) != len) {
		write_irels();
		return;
	    }
	}

	/* next read the index strategy map */
	if ((nread = FileRead(fd, &len, sizeof(int))) != sizeof(int)) {
	    write_irels();
	    return;
	}

	strat = (IndexStrategy) palloc(len);
	if ((nread = FileRead(fd, strat, len)) != len) {
	    write_irels();
	    return;
	}

/* oh, for god's sake... */
#define SMD(i)	strat[0].strategyMapData[i].entry[0]

	/* have to reinit the function pointers in the strategy maps */
	for (i = 0; i < am->amstrategies; i++)
	    fmgr_info(SMD(i).procedure, &(SMD(i).func), &(SMD(i).nargs));


	p = (char *) (&ird->rd_att.data[ird->rd_rel->relnatts]);
	*((IndexStrategy *) p) = strat;

	/* finally, read the vector of support procedures */
	if ((nread = FileRead(fd, &len, sizeof(int))) != sizeof(int)) {
	    write_irels();
	    return;
	}

	support = (RegProcedure *) palloc(len);
	if ((nread = FileRead(fd, support, len)) != len) {
	    write_irels();
	    return;
	}

	p += sizeof(IndexStrategy);
	*((RegProcedure **) p) = support;

	RelationCacheInsert(ird);
    }
}

void
write_irels()
{
    int len;
    int nwritten;
    File fd;
    Relation irel[Num_indices_bootstrap];
    Relation ird;
    AccessMethodTupleForm am;
    RelationTupleForm relform;
    IndexStrategy *strat;
    RegProcedure **support;
    char *p;
    ProcessingMode oldmode;
    extern Name AttributeNumIndex;
    int i;
    int relno;
    RelationBuildDescInfo bi;

    fd = FileNameOpenFile(INIT_FILENAME, O_WRONLY|O_CREAT|O_TRUNC, 0600);
    if (fd < 0)
	elog(FATAL, "cannot create init file %s", INIT_FILENAME);

    (void) FileSeek(fd, 0L, L_SET);

    /*
     *  Build a relation descriptor for pg_attnumind without resort to the
     *  descriptor cache.  In order to do this, we set ProcessingMode
     *  to Bootstrap.  The effect of this is to disable indexed relation
     *  searches -- a necessary step, since we're trying to instantiate
     *  the index relation descriptors here.
     */

    oldmode = GetProcessingMode();
    SetProcessingMode(BootstrapProcessing);

    bi.infotype = INFO_RELNAME;
    bi.i.info_name = AttributeNumIndex;
    irel[0] = RelationBuildDesc(bi);
    irel[0]->rd_isnailed = true;

    bi.i.info_name = ClassNameIndex;
    irel[1] = RelationBuildDesc(bi);
    irel[1]->rd_isnailed = true;

    bi.i.info_name = ClassOidIndex;
    irel[2] = RelationBuildDesc(bi);
    irel[2]->rd_isnailed = true;

    SetProcessingMode(oldmode);

    /* nail the descriptor in the cache */
    for (relno = 0; relno < Num_indices_bootstrap; relno++) {
	ird = irel[relno];

	/* save the volatile fields in the relation descriptor */
	am = ird->rd_am;
	ird->rd_am = (AccessMethodTupleForm) NULL;
	relform = ird->rd_rel;
	ird->rd_rel = (RelationTupleForm) NULL;
	p = (char *) (&ird->rd_att.data[relform->relnatts]);
	strat = (IndexStrategy *) p;
	p += sizeof(IndexStrategy);
	support = (RegProcedure **) p;

	/* first write the relation descriptor, excluding strategy and support */
	len = sizeof(RelationData)
	      + ((relform->relnatts - 1) * sizeof(ird->rd_att));
	len += sizeof(IndexStrategy) + sizeof(RegProcedure *);

	if ((nwritten = FileWrite(fd, &len, sizeof(int))) != sizeof(int))
	    elog(FATAL, "cannot write init file -- descriptor length");

	if ((nwritten = FileWrite(fd, ird, len)) != len)
	    elog(FATAL, "cannot write init file -- reldesc");

	/* next write the access method tuple form */
	len = sizeof(AccessMethodTupleFormD);
	if ((nwritten = FileWrite(fd, &len, sizeof(int))) != sizeof(int))
	    elog(FATAL, "cannot write init file -- am tuple form length");

	if ((nwritten = FileWrite(fd, am, len)) != len)
	    elog(FATAL, "cannot write init file -- am tuple form");

	/* next write the relation tuple form */
	len = sizeof(RelationTupleFormD);
	if ((nwritten = FileWrite(fd, &len, sizeof(int))) != sizeof(int))
	    elog(FATAL, "cannot write init file -- relation tuple form length");

	if ((nwritten = FileWrite(fd, relform, len)) != len)
	    elog(FATAL, "cannot write init file -- relation tuple form");

	/* next, do all the attribute tuple form data entries */
	len = sizeof(RuleLock) + sizeof(*ird->rd_att.data[0]);
	for (i = 0; i < relform->relnatts; i++) {
	    if ((nwritten = FileWrite(fd, &len, sizeof(int))) != sizeof(int))
		elog(FATAL, "cannot write init file -- length of attdesc %d", i);
	    if ((nwritten = FileWrite(fd, ird->rd_att.data[i], len)) != len)
		elog(FATAL, "cannot write init file -- attdesc %d", i);
	}

	/* next write the index strategy map */
	len = AttributeNumberGetIndexStrategySize(relform->relnatts,
						  am->amstrategies);
	if ((nwritten = FileWrite(fd, &len, sizeof(int))) != sizeof(int))
	    elog(FATAL, "cannot write init file -- strategy map length");

	if ((nwritten = FileWrite(fd, *strat, len)) != len)
	    elog(FATAL, "cannot write init file -- stragegy map");

	/* finally, write the vector of support procedures */
	len = relform->relnatts * (am->amstrategies * sizeof(RegProcedure));
	if ((nwritten = FileWrite(fd, &len, sizeof(int))) != sizeof(int))
	    elog(FATAL, "cannot write init file -- support vector length");

	if ((nwritten = FileWrite(fd, *support, len)) != len)
	    elog(FATAL, "cannot write init file -- support vector");

	/* restore volatile fields */
	ird->rd_am = am;
	ird->rd_rel = relform;
    }

    (void) FileClose(fd);
}
