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
 
#include "tmp/miscadmin.h"
#include "tmp/hashlib.h"
#include "tmp/hasht.h"
 
#include "utils/memutils.h"
#include "utils/lmgr.h"
#include "utils/log.h"
#include "utils/mcxt.h"
#include "utils/rel.h"
 
#include "catalog/catname.h"
#include "catalog/syscache.h"

#include "catalog/pg_attribute.h"
#include "catalog/pg_index.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_relation.h"
#include "catalog/pg_type.h"

#include "catalog/pg_variable.h"
#include "catalog/pg_log.h"
#include "catalog/pg_time.h"

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
HashTable 	RelationCacheHashByName;
HashTable 	GlobalNameCache;
HashTable	RelationCacheHashById;
HashTable 	GlobalIdCache;
HashTable 	PrivateRelationCacheHashByName;
HashTable	PrivateRelationCacheHashById;
 
/* ----------------------------------------------------------------
 *	RelationIdGetRelation() and RelationNameGetRelation()
 *			support functions
 * ----------------------------------------------------------------
 */
 
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

/* --------------------------------
 *	relopen		- physically open a relation
 *
 *	Note that this function should be replaced by a fd manager
 *	which interacts with the reldesc manager.  (Thus, this function
 *	may be eventually have different functionality/have a different name.
 *
 *	Perhaps relpath() code should be placed in-line here eventually.
 * --------------------------------
 */
/**** xxref:
 *           BuildRelation
 *           formrdesc
 ****/
File
relopen(relationName, flags, mode)
    char	relationName[];
    int		flags;
    int		mode;
{
    File	file;
    int		oumask;
    extern char	*relpath();
    
    oumask = (int) umask(0077);
    
    file = FileNameOpenFile(relpath(relationName), flags, mode);
    if (file == -1) 
	elog(NOTICE, "Unable too open %s (%d)", relpath(relationName), errno);
    
    umask(oumask);
    
    return
	file;
}

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
 *
 *	ADD INDEXING HERE
 * --------------------------------
 */
HeapTuple
ScanPgRelation(buildinfo)   
    RelationBuildDescInfo buildinfo;    
{
    HeapTuple	 pg_relation_tuple;
    Relation	 pg_relation_desc;
    HeapScanDesc pg_relation_scan;
    ScanKeyData	 key;
    
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
    pg_relation_scan =
	heap_beginscan(pg_relation_desc, 0, NowTimeQual, 1, &key);
    pg_relation_tuple = heap_getnext(pg_relation_scan, 0, (Buffer *)NULL);

    /* ----------------
     *	close the relation
     * ----------------
     */
    heap_endscan(pg_relation_scan);
    heap_close(pg_relation_desc);

    /* ----------------
     *	return tuple or NULL
     * ----------------
     */
    if (! HeapTupleIsValid(pg_relation_tuple))
	return NULL;

    return pg_relation_tuple;
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
	    + sizeof(RegProcedure *);
    
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
 *
 *	ADD INDEXING HERE
 * --------------------------------
 */
void
RelationBuildTupleDesc(buildinfo, relation, attp, natts) 
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

    /* ----------------
     *	form a scan key
     * ----------------
     */
	ScanKeyEntryInitialize(&key.data[0], 0, 
                           AttributeRelationIdAttributeNumber,
                           ObjectIdEqualRegProcedure,
                           ObjectIdGetDatum(relation->rd_id));
    
    /* ----------------
     *	open pg_relation and begin a scan
     * ----------------
     */
    pg_attribute_desc = heap_openr(AttributeRelationName);
    pg_attribute_scan =
	heap_beginscan(pg_attribute_desc, 0, NowTimeQual, 1, &key);
    
    /* ----------------
     *	for each tuple scanned, add attribute data to relation->rd_att
     * ----------------
     */
    while (pg_attribute_tuple =
	   heap_getnext(pg_attribute_scan, 0, (Buffer *) NULL),
	   HeapTupleIsValid(pg_attribute_tuple)) {
	    
	attp = (AttributeTupleForm)
	    HeapTupleGetForm(pg_attribute_tuple);
		    
	if (!AttributeNumberIsValid(attp->attnum))
	    elog(WARN, "RelationBuildTupleDesc: %s bad attribute number %d",
		 BuildDescInfoError(buildinfo), attp->attnum);
    
	if (AttributeNumberIsForUserDefinedAttribute(attp->attnum)) {
	    if (AttributeIsValid(relation->rd_att.data[attp->attnum - 1]))
		elog(WARN, "RelationBuildTupleDesc: %s bad attribute %d",
		     BuildDescInfoError(buildinfo), attp->attnum - 1);
	
	    relation->rd_att.data[attp->attnum - 1] = (Attribute)
		palloc(sizeof (RuleLock) + sizeof *relation->rd_att.data[0]);
	
	    bcopy((char *) attp,
		  (char *) relation->rd_att.data[attp->attnum - 1],
		  sizeof *relation->rd_att.data[0]);
	}
    }
    
    /* ----------------
     *	end the scan and close the attribute relation
     * ----------------
     */
    heap_endscan(pg_attribute_scan);
    heap_close(pg_attribute_desc);
    
    /* ----------------
     *	check that all attributes are valid because it's possible that
     *  some tuples in the pg_attribute relation are missing so rd_att.data
     *  might have zeros in it someplace.
     * ----------------
     */
    for(i = --natts; i>=0; i--) {
	if (! AttributeIsValid(relation->rd_att.data[i]))
	    elog(WARN,
		 "RelationBuildTupleDesc: %s attribute %d invalid",
		    RelationGetRelationName(relation), i);
    }
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
    AttributeTupleForm	attp;
    ObjectId		attrioid; /* attribute relation index relation oid */

    extern GlobalMemory CacheCxt;
    MemoryContext	oldcxt;
    
    HeapTuple		pg_relation_tuple;
    HashTable		NameCacheSave;
    HashTable		IdCacheSave;
    
    /* ----------------
     *	hold onto our caches
     * ----------------
     */
    NameCacheSave = 		RelationCacheHashByName;
    IdCacheSave = 		RelationCacheHashById;
    RelationCacheHashByName =	PrivateRelationCacheHashByName;
    RelationCacheHashById = 	PrivateRelationCacheHashById;
    
    oldcxt = MemoryContextSwitchTo(CacheCxt);

    /* ----------------
     *	find the tuple in pg_relation corresponding to the given relation id
     * ----------------
     */
    pg_relation_tuple = ScanPgRelation(buildinfo);

    /* ----------------
     *	if no such tuple exists, report an error and return NULL
     * ----------------
     */
    if (! HeapTupleIsValid(pg_relation_tuple)) {
	elog(NOTICE, "RelationBuildDesc: %s nonexistent",
	     BuildDescInfoError(buildinfo));
	
	RelationCacheHashByName = NameCacheSave;
	RelationCacheHashById = IdCacheSave;
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
     *	open the file named in the pg_relation_tuple and
     *  assign the file descriptor to rd_fd and record the
     *  number of blocks in the relation
     * ----------------
     */
    fd = relopen(&relp->relname, O_RDWR, 0666);
    
    Assert (fd >= -1);
    if (fd == -1)
	elog(NOTICE, "RelationIdBuildRelation: relopen(%s): %m",
	     &relp->relname);
    
    relation->rd_fd = fd;

    /* ----------------
     *	initialize relation->rd_refcnt
     * ----------------
     */
    RelationSetReferenceCount(relation, 1);
    
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
     *	insert newly created relation into proper relcaches,
     *  restore memory context and return the new reldesc.
     * ----------------
     */
    RelationCacheHashByName = NameCacheSave;
    RelationCacheHashById = IdCacheSave;
    
    HashTableInsert(RelationCacheHashByName, relation);
    HashTableInsert(RelationCacheHashById, relation);
    
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
			    relamstrategies, relamsupport);

    RelationSetIndexSupport(relation, strategy, support);
}

/* --------------------------------
 *	formrdesc
 *
 *	This is a special version of RelationBuildDesc()
 *	used by RelationInitialize() in initializing the
 *	relcache.
 * --------------------------------
 */
private void
formrdesc(relationName, reloid, natts, att, initialReferenceCount)
    char		relationName[];
    OID			reloid;			/* XXX unused */
    u_int		natts;
    AttributeTupleFormData att[];
    int			initialReferenceCount;
{
    Relation	relation;
    int		fd;
    int		len;
    int		i;
    char	*relpath();
    File	relopen();
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
    
    strncpy(&relation->rd_rel->relname, relationName, 16);
    relation->rd_rel->relowner = InvalidObjectId;	/* XXX incorrect */
    relation->rd_rel->relpages = 1;			/* XXX */
    relation->rd_rel->reltuples = 1;			/* XXX */
    relation->rd_rel->relhasindex = '\0';
    relation->rd_rel->relkind = 'r';
    relation->rd_rel->relarch = 'n';
    relation->rd_rel->relnatts = (uint16) natts;
    
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
     *	initialize relation id (??? should use reloid parameter instead ???)
     * ----------------
     */
    relation->rd_id = relation->rd_att.data[0]->attrelid;
    
    /* ----------------
     *	add new reldesc to the private relcache
     * ----------------
     */
    HashTableInsert(PrivateRelationCacheHashByName, relation);
    HashTableInsert(PrivateRelationCacheHashById, relation);
    
    /* ----------------
     *	add new reldesc to the public relcache
     * ----------------
     */
    if (initialReferenceCount != 0) {
	publicCopy = (Relation)palloc(len);
	bcopy(relation, publicCopy, len);
	
	RelationSetReferenceCount(publicCopy, initialReferenceCount);
	HashTableInsert(RelationCacheHashByName, publicCopy);
	HashTableInsert(RelationCacheHashById, publicCopy);
	
	RelationInitLockInfo(relation);	/* XXX unuseful here ??? */
    }
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

    rd = (Relation)
	KeyHashTableLookup(RelationCacheHashById, relationId);
    
    if (!RelationIsValid(rd))
	rd = (Relation) KeyHashTableLookup(GlobalIdCache, relationId);

    if (RelationIsValid(rd)) {
	if (rd->rd_fd == -1) {
	    rd->rd_fd = relopen(&rd->rd_rel-> relname,
				   O_RDWR | O_CREAT, 0666);
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

    rd = (Relation)
	KeyHashTableLookup(RelationCacheHashByName, relationName);
    
    if (!RelationIsValid(rd))
	rd = (Relation) KeyHashTableLookup(GlobalNameCache, relationName);

    if (RelationIsValid(rd)) {
	if (rd->rd_fd == -1) {
	    rd->rd_fd = relopen(&rd->rd_rel->relname,
				O_RDWR | O_CREAT, 0666);
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
    
    if (relation->rd_refcnt > 0x1000) {	/* XXX */
	/* this is a non-regeneratable special relation */
	return;
    }
    
    if (onlyFlushReferenceCountZero == false 
	|| RelationHasReferenceCountZero(relation)) {
	
	oldcxt = MemoryContextSwitchTo(CacheCxt);
	
	HashTableDelete(RelationCacheHashByName,relation);
	HashTableDelete(RelationCacheHashById,relation);
	
	FileClose(RelationGetSystemPort(relation));
	
	i = relation->rd_rel->relnatts - 1;
	p = &relation->rd_att.data[i];
	while ((i -= 1) >= 0) {
	    pfree((char *)*p--);
	}
	
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
    
    relation = (Relation)
	KeyHashTableLookup(RelationCacheHashById, relationId);
    
    if (PointerIsValid(relation)) {
	/* Assert(RelationIsValid(relation)); */
	RelationFlushRelation(relation, false);
    }
}
 
/* --------------------------------
 *	RelationIdInvalidateRelationCacheByAccessMethodId
 *
 *	RelationFlushIndexes is needed for use with HashTableWalk..
 * --------------------------------
 */
private void
RelationFlushIndexes(relation, accessMethodId)
    Relation	relation;
    ObjectId	accessMethodId;
{
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
    HashTableWalk(RelationCacheHashByName, RelationFlushIndexes,
		  accessMethodId);
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
    HashTableWalk(RelationCacheHashByName, RelationFlushRelation, 
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
    
    oldcxt = MemoryContextSwitchTo(CacheCxt);
    
    if (oldcxt != (MemoryContext)CacheCxt) 
	elog(NOIND,"RelationRegisterRelation: WARNING: Context != CacheCxt");
    
    HashTableInsert(RelationCacheHashByName,relation);
    HashTableInsert(RelationCacheHashById,relation);
    
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
    rd = (Relation) KeyHashTableLookup(RelationCacheHashById,temprel->rd_id);
    
    if (!RelationIsValid(rd))
        rd = (Relation) KeyHashTableLookup(GlobalIdCache, temprel->rd_id);
    
    if (RelationIsValid(rd))
	return;
    
    /* ----------------------------------------
     *  the temprel is created by another backend, insert it into the 
     *  hash table
     * ----------------------------------------
     */
    oldcxt = MemoryContextSwitchTo(CacheCxt);
    
    RelationRegisterRelation(temprel);
    
    MemoryContextSwitchTo(oldcxt);
}
 
/* ----------------------------------------------------------------
 *	    catalog cache initialization support functions
 * ----------------------------------------------------------------
 */
 
/* --------------------------------
 *	CompareNameInArgumentWithRelationNameInRelation()
 *	CompareIdInArgumentWithIdInRelation()
 *
 *	these are needed for use with CreateHashTable() in
 *	RelationInitialize() below.
 * --------------------------------
 */
private int
CompareNameInArgumentWithRelationNameInRelation(relation, relationName)
    Relation	relation;
    String	relationName;
{
    return
	strncmp(relationName, &relation->rd_rel->relname, 16);
}
private int
CompareIdInArgumentWithIdInRelation(relation, relationId)
    Relation	relation;
    ObjectId	relationId;
{
    return
	relation->rd_id - relationId;
}
 
/* --------------------------------
 *	RelationInitialize hash function support
 * --------------------------------
 */
/* ----------------
 *	HashByNameAsArgument
 * ----------------
 */
private Index
HashByNameAsArgument(collisions, hashTableSize, relationName)
    uint16	collisions;
    Size	hashTableSize;
    Name	relationName;
{
    return
      NameHashFunction(collisions, hashTableSize, relationName);
}

/* ----------------
 *	HashByNameInRelation
 * ----------------
 */
private Index
HashByNameInRelation(collisions, hashTableSize, relation)
    uint16	collisions;
    Size	hashTableSize;
    Relation	relation;
{
    return
      NameHashFunction(collisions, hashTableSize, &relation->rd_rel->relname);
}
 
/* ----------------
 *	HashByIdAsArgument
 * ----------------
 */
private Index
HashByIdAsArgument(collisions, hashTableSize, relationId)
    uint16	collisions;
    Size	hashTableSize;
    uint32	relationId; 
{
    return
	IntegerHashFunction(collisions, hashTableSize, relationId);
}
 
/* ----------------
 *	HashByIdInRelation
 * ----------------
 */
private Index
HashByIdInRelation(collisions, hashTableSize, relation)
    uint16	collisions;
    Size	hashTableSize;
    Relation	relation;
{
    return
	IntegerHashFunction(collisions, hashTableSize, relation->rd_id);
}
 
/* --------------------------------
 *	RelationInitialize
 *
 *	This initializes the relation descriptor cache.
 * --------------------------------
 */
void 
RelationInitialize()
{
    extern GlobalMemory		CacheCxt;
    MemoryContext		oldcxt;
    int				initialReferences;

    /* ----------------
     *	switch to cache memory context
     * ----------------
     */
    if (!CacheCxt)
	CacheCxt = CreateGlobalMemory("Cache");
    
    oldcxt = MemoryContextSwitchTo(CacheCxt);

    /* ----------------
     *	create global caches
     * ----------------
     */
    RelationCacheHashByName =
	CreateHashTable(HashByNameAsArgument, 
			HashByNameInRelation, 
			CompareNameInArgumentWithRelationNameInRelation, 
			NULL,
			300);
    
    GlobalNameCache = RelationCacheHashByName;
    
    RelationCacheHashById =
	CreateHashTable(HashByIdAsArgument, 
			HashByIdInRelation, 
			CompareIdInArgumentWithIdInRelation,
			NULL,
			300);
    
    GlobalIdCache = RelationCacheHashById;
    
    /* ----------------
     *  create private caches
     *
     *  these should use a hash function that is perfect on them
     *  and the hash table code should have a way of telling it not
     *  to bother trying to resize 'cauase they all fit and there
     *  arn't any collisions.  Also, the hash table code could have
     *  a HashTableDisableModification call.
     *
     *  ??? What are the private caches used for? my guess is that
     *      system catalog relations use them but I'm not sure
     *      -cim 2/5/91
     * ----------------
     */
    PrivateRelationCacheHashByName =
	CreateHashTable(HashByNameAsArgument, 
			HashByNameInRelation, 
			CompareNameInArgumentWithRelationNameInRelation, 
			NULL,
			30);
    
    PrivateRelationCacheHashById =
	CreateHashTable(HashByIdAsArgument, 
			HashByIdInRelation, 
			CompareIdInArgumentWithIdInRelation,
			NULL,
			30);
    
    /* ----------------
     * using a large positive initial reference count is a hack
     * to prevent generation of the special descriptors
     * ----------------
     */
    initialReferences = 0;
    if (AMI_OVERRIDE) {
	initialReferences = 0x2000;
    }
    
    /* ----------------
     *	initialize the cache with pre-made relation descriptors
     *  for some of the more important system relations.  These
     *  relations should always be in the cache.
     * ----------------
     */
#define INIT_RELDESC(x) \
    formrdesc(SCHEMA_NAME(x), \
	      SCHEMA_DESC(x)[0].attrelid, \
	      SCHEMA_NATTS(x), \
	      SCHEMA_DESC(x), \
	      initialReferences);
    
    INIT_RELDESC(pg_relation);
    INIT_RELDESC(pg_attribute);
    INIT_RELDESC(pg_proc);
    INIT_RELDESC(pg_type);
    
    initialReferences = 0x2000;
    
    INIT_RELDESC(pg_variable);
    INIT_RELDESC(pg_log);
    INIT_RELDESC(pg_time);
    
    MemoryContextSwitchTo(oldcxt);
}
