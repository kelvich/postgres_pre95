/* ----------------------------------------------------------------
 * relcache.c --
 *	POSTGRES relation descriptor cache code.
 *
 * NOTE:	This file is in the process of being cleaned up
 *		before I add system attribute indexing.  -cim 10/2/90
 *
 * OLD NOTES:
 *     This code will leak relation descriptor sctructures. XXXX.
 * It will happen whenever *GetRelation() is called without a corrosponding
 * FreeRelation().    This will happen at a minimum when a transaction
 * aborts.  To fix this, each transaction could keep a list of relation/
 * reference count pairs.
 *
 *     It is not clear who is responcible for incrementing and decrementing
 * the reference count and when RelationFree() should be called.  I would
 * propose that there be calls: RelationAddUsage and RelationSubtractUsage
 * where AddUsage is called instead of incrementing the reference count
 * and SubtractUsage instead of decrementing it.  SubtractUsage would
 * be responcible for calling RelationFree which would become a private
 * routine.  The following files (there could be more) paly with
 * the refernece count:
 *	access/{genam,logaccess}.c
 *	spam/{spam-controll,var-access}.c
 *	util/{access,create}.c
 *
 * Note:
 *	The following code contains many undocumented hacks.  Please be
 *	careful....
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
 
extern bool	AMI_OVERRIDE;	/* XXX style */
 
#include "utils/relcache.h"
#ifdef sprite
#include "sprite_file.h"
#else
#include "storage/fd.h"
#endif /* sprite */
 
extern HeapTuple	GetHeapTuple();	/* XXX use include file */
 
/* #define DEBUG_RELCACHE		/* debug relcache... */
#ifdef DEBUG_RELCACHE
# define DO_DB(A) A
# define DO_NOTDB(A) /* A */
# define private /* static */
#else
# define DO_DB(A) /* A */
# define DO_NOTDB(A) A
# define private static
#endif
     
#define NO_DO_DB(A) /* A */
#define IN()	DO_DB(ElogDebugIndentLevel++)
#define OUT()	DO_DB(ElogDebugIndentLevel--)
 
/* XXX using the values in h/anum.h might be better */
#define REL_NATTS	(14)
#define ATT_NATTS	(12)
#define PRO_NATTS	(10)
#define TYP_NATTS	(14)
 
/* these next 3 are bogus for now... */
#define VAR_NATTS	(2)
#define LOG_NATTS	(2)
#define TIM_NATTS	(2)
 
/*
 * 	Relations are cached two ways, by name and by id, thus there are two 
 *	hash tables for referencing them. 
 */
HashTable 	RelationCacheHashByName;
HashTable 	GlobalNameCache;
HashTable	RelationCacheHashById;
HashTable 	GlobalIdCache;
HashTable 	PrivateRelationCacheHashByName;
HashTable	PrivateRelationCacheHashById;
 
/*
 *	XXX the atttyparg field is always set to 0l in the tables.
 *	This may or may not be correct.  (Depends on array semantics.)
 */
private AttributeTupleFormData	relatt[REL_NATTS] = {
    { 83l, "relname",      19l, 83l, 0l, 0l, 16, 1, 0, '\0', '\001', 0l },
    { 83l, "relowner",     26l, 83l, 0l, 0l, 4, 2, 0, '\001', '\001', 0l },
    { 83l, "relam",        26l, 83l, 0l, 0l, 4, 3, 0, '\001', '\001', 0l },
    { 83l, "relpages",     23,  83l, 0l, 0l, 4, 4, 0, '\001', '\001', 0l },
    { 83l, "reltuples",    23,  83l, 0l, 0l, 4, 5, 0, '\001', '\001', 0l },
    { 83l, "relexpires",   20,  83l, 0l, 0l, 4, 6, 0, '\001', '\001', 0l },
    { 83l, "relpreserved", 20,  83l, 0l, 0l, 4, 7, 0, '\001', '\001', 0l },
    { 83l, "relhasindex",  16,  83l, 0l, 0l, 1, 8, 0, '\001', '\001', 0l },
    { 83l, "relisshared",  16,  83l, 0l, 0l, 1, 9, 0, '\001', '\001', 0l },
    { 83l, "relkind",      18,  83l, 0l, 0l, 1, 10, 0, '\001', '\001', 0l },
    { 83l, "relarch",      18,  83l, 0l, 0l, 1, 11, 0, '\001', '\001', 0l },
    { 83l, "relnatts",     21,  83l, 0l, 0l, 2, 12, 0, '\001', '\001', 0l },
    { 83l, "relkey",       22,  83l, 0l, 0l, 16, 13, 0, '\0', '\001', 0l },
/*
 *  { 83l, "relkeyop",     30,  83l, 0l, 0l, 32, 14, 0, '\0', '\001', 0l },
 *  { 83l, "rellock",     591,   0l, 0l, 0l,  8, 15, 0, '\0', '\001', 0l }
 */
    { 83l, "relkeyop",     30,  83l, 0l, 0l, 32, 14, 0, '\0', '\001', 0l }
};
 
private AttributeTupleFormData	attatt[ATT_NATTS] = {
    { 75l, "attrelid",    26l, 75l, 0l, 0l, 4, 1, 0, '\001', '\001', 0l },
    { 75l, "attname",     19l, 75l, 0l, 0l, 16, 2, 0, '\0', '\001', 0l },
    { 75l, "atttypid",    26l, 75l, 0l, 0l, 4, 3, 0, '\001', '\001', 0l },
    { 75l, "attdefrel",   26l, 75l, 0l, 0l, 4, 4, 0, '\001', '\001', 0l },
    { 75l, "attnvals",    23l, 75l, 0l, 0l, 4, 5, 0, '\001', '\001', 0l },
    { 75l, "atttyparg",   26l, 75l, 0l, 0l, 4, 6, 0, '\001', '\001', 0l },
    { 75l, "attlen",      21l, 75l, 0l, 0l, 2, 7, 0, '\001', '\001', 0l },
    { 75l, "attnum",      21l, 75l, 0l, 0l, 2, 8, 0, '\001', '\001', 0l },
    { 75l, "attbound",    21l, 75l, 0l, 0l, 2, 9, 0, '\001', '\001', 0l },
    { 75l, "attbyval",    16l, 75l, 0l, 0l, 1, 10, 0, '\001', '\001', 0l },
    { 75l, "attcanindex", 16l, 75l, 0l, 0l, 1, 11, 0, '\001', '\001', 0l },
    { 75l, "attproc",     26l, 75l, 0l, 0l, 4, 12, 0, '\001', '\001', 0l }
};
 
private AttributeTupleFormData	proatt[PRO_NATTS] = {
    { 81l, "proname",       19l, 81l, 0l, 0l, 16, 1, 0, '\0', '\001', 0l },
    { 81l, "proowner",      26l, 81l, 0l, 0l, 4, 2, 0, '\001', '\001', 0l },
    { 81l, "prolang",       26l, 81l, 0l, 0l, 4, 3, 0, '\001', '\001', 0l },
    { 81l, "proisinh",      16l, 81l, 0l, 0l, 1, 4, 0, '\001', '\001', 0l },
    { 81l, "proistrusted",  16l, 81l, 0l, 0l, 1, 5, 0, '\001', '\001', 0l },
    { 81l, "proiscachable", 16l, 81l, 0l, 0l, 1, 6, 0, '\001', '\001', 0l },
    { 81l, "pronargs",      21l, 81l, 0l, 0l, 2, 7, 0, '\001', '\001', 0l },
    { 81l, "prorettype",    26l, 81l, 0l, 0l, 4, 8, 0, '\001', '\001', 0l },
    { 81l, "prosrc",        25l, 81l, 0l, 0l, -1, 9, 0, '\0', '\001', 0l },
    { 81l, "probin",        17l, 81l, 0l, 0l, -1, 10, 0, '\0', '\001', 0l }
};
 
private AttributeTupleFormData	typatt[TYP_NATTS] = {
    { 71l, "typname",      19l, 71l, 0l, 0l, 16, 1, 0, '\0', '\001', 0l },
    { 71l, "typowner",     26l, 71l, 0l, 0l, 4, 2, 0, '\001', '\001', 0l },
    { 71l, "typlen",       21l, 71l, 0l, 0l, 2, 3, 0, '\001', '\001', 0l },
    { 71l, "typprtlen",    21l, 71l, 0l, 0l, 2, 4, 0, '\001', '\001', 0l },
    { 71l, "typbyval",     16l, 71l, 0l, 0l, 1, 5, 0, '\001', '\001', 0l },
    { 71l, "typisproc",    16l, 71l, 0l, 0l, 1, 6, 0, '\001', '\001', 0l },
    { 71l, "typisdefined", 16l, 71l, 0l, 0l, 1, 7, 0, '\001', '\001', 0l },
    { 71l, "typprocid",    26l, 71l, 0l, 0l, 4, 8, 0, '\001', '\001', 0l },
    { 71l, "typelem",      26l, 71l, 0l, 0l, 4, 9, 0, '\001', '\001', 0l },
    { 71l, "typinput",     24l, 71l, 0l, 0l, 4, 10, 0, '\001', '\001', 0l },
    { 71l, "typoutput",    24l, 71l, 0l, 0l, 4, 11, 0, '\001', '\001', 0l },
    { 71l, "typreceive",   24l, 71l, 0l, 0l, 4, 12, 0, '\001', '\001', 0l },
    { 71l, "typsend",      24l, 71l, 0l, 0l, 4, 13, 0, '\001', '\001', 0l },
    { 71l, "typdefault",   25l, 71l, 0l, 0l, -1, 14, 0, '\0', '\001', 0l }
};
 
/* ----------------
 * XXX
 * these are bogus, the values come from the bogus entries
 * in the local.ami script
 * ----------------
 */
private AttributeTupleFormData	varatt[VAR_NATTS] = {
   { 90l, "varname",  19l, 90l, 0l, 0l, 16, 1, 0, 0, '\001', 0l },
   { 90l, "varvalue", 17l, 90l, 0l, 0l, -1, 2, 0, 0, '\001', 0l },
};
 
private AttributeTupleFormData	logatt[LOG_NATTS] = {
   { 91l, "varname",  19l, 91l, 0l, 0l, 16, 1, 0, 0, '\001', 0l },
   { 91l, "varvalue", 17l, 91l, 0l, 0l, -1, 2, 0, 0, '\001', 0l },
};
 
private AttributeTupleFormData	timatt[VAR_NATTS] = {
   { 92l, "varname",  19l, 92l, 0l, 0l, 16, 1, 0, 0, '\001', 0l },
   { 92l, "varvalue", 17l, 92l, 0l, 0l, -1, 2, 0, 0, '\001', 0l },
};
 
/* ----------------------------------------------------------------
 *			hash function support
 *
 *	XXX where are these used???
 * ----------------------------------------------------------------
 */
/* --------------------------------
 *	HashByNameAsArgument
 * --------------------------------
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

/* --------------------------------
 *	HashByNameInRelation
 * --------------------------------
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
 
/* --------------------------------
 *	HashByIdAsArgument
 * --------------------------------
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
 
/* --------------------------------
 *	HashByIdInRelation
 * --------------------------------
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
 
/* ----------------------------------------------------------------
 *	      RelationIdGetRelation() and getreldesc()
 *			support functions
 * ----------------------------------------------------------------
 */
 
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
    
    IN();
    
    oumask = (int) umask(0077);
    
    file = FileNameOpenFile(relpath(relationName), flags, mode);
    if (file == -1) 
	elog(NOTICE, "Unable too open %s (%d)", relpath(relationName), errno);
    
    umask(oumask);
    
    OUT();
    
    return (file);
}
 
/* --------------------------------
 * GetAttributeRelationIndexRelationId --
 * 
 * find relation id of the index of relation id's on the attribute relation
 *
 *  attempt to return the object id of an index relation of 
 *  RelationId on the AttributeRelation.
 * --------------------------------
 */
 
#define	UNTRIED 0
#define FOUND	1
#define FAILED	2
#define	TRYING	3
#define TRIED	4

/**** xxref:
 *           BuildRelation
 ****/
private ObjectId
GetAttributeRelationIndexRelationId ()
{
    HeapTuple		tuple;
    ObjectId		objectId;
    
    static int		state = UNTRIED;
    
    /*
     * XXX The next line is incorrect.  It should be removed if the
     * performance penalty is not too great or if indexes are added
     * and dropped frequently.  A more clever hack which will produce
     * correct behavior in a *single-user* situation is to keep a
     * global variable which is incremented each time DEFINE INDEX
     * is called.  Then if the state is FAILED and a index has been
     * created since the last call to this function, then treat the
     * state as UNTRIED.  But, this hack works incorrectly in a
     * multiuser environment.  The correct thing to do is to have the
     * system cache keep information about failed search and to have
     * this information invalidated when a tuple is appended.  -hirohama
     */
    IN();
    if (state == FAILED) {
	OUT();
	return InvalidObjectId;
    }
    
    /*
     * We don't want any infinately recusive calls to this
     * routine because SearchSysCacheTuple does some relation
     * descriptor lookups.  
     */
    state = FAILED;
    
    /*
     * The syscache must be searched again in the case of FOUND state
     * since the result may have been invalidated since the last call
     * to this function.
     */
    tuple = SearchSysCacheTuple(INDRELIDKEY,
				AttributeRelationId,
				AttributeRelationIdAttributeNumber);
	
    if (!HeapTupleIsValid(tuple)) {
	OUT();
	state = FAILED;
	return InvalidObjectId;
    }
    
    objectId = ((IndexTupleForm) HeapTupleGetForm(tuple))-> indexrelid;
    
    state = FOUND;
    OUT();
     
    return objectId;
}
 
/* --------------------------------
 *	BuildRelationAttributes
 * --------------------------------
 */
/**** xxref:
 *           BuildRelation
 ****/
private inline void
BuildRelationAttributes(attp, relation, errorName) 
    AttributeTupleForm	attp;
    Relation		relation;
{
    IN();
 
    if (!AttributeNumberIsValid(attp->attnum))
	elog(WARN, "getreldesc: found zero attnum %s", errorName);
    
    if (AttributeNumberIsForUserDefinedAttribute(attp->attnum)) {
	
	if (AttributeIsValid(relation->rd_att.data[attp->attnum - 1]))
	    elog(WARN, "getreldesc: corrupted ATTRIBUTE %s",errorName);
	relation->rd_att.data[attp->attnum - 1] = (Attribute)
	    palloc(sizeof (RuleLock) + sizeof *relation->rd_att.data[0]);
	
	bcopy((char *)attp, (char *)relation->rd_att.data[attp->attnum - 1],
	      sizeof *relation->rd_att.data[0]);
    }
    
    OUT();
}
 
/* --------------------------------
 *	BuildRelation
 * --------------------------------
 */
/**** xxref:
 *           RelationIdGetRelation
 *           getreldesc
 ****/
private Relation
BuildRelation(rd, sd, errorName, oldcxt, tuple, NameCacheSave, IdCacheSave)
    Relation		rd;
    HeapScanDesc	sd;
    String		errorName;
    MemoryContext	oldcxt;
    HeapTuple		tuple;
    HashTable		NameCacheSave;
    HashTable		IdCacheSave;
{
    static bool		avoidIndex = false;
    File		fd;
    Relation		relation;
    int			len;
    u_int		numatts, natts;
    ObjectId		relid;
    RelationTupleForm	relp;
    AttributeTupleForm	attp;
    ScanKeyData		key;
    ObjectId		attrioid; /* attribute relation index relation oid */
    
    RelationTupleForm	relationTupleForm;
    
    extern		pfree();
    
    if (!HeapTupleIsValid(tuple)) {
	elog(NOTICE, "BuildRelation: %s relation nonexistent", errorName);
	amendscan(sd);
	amclose(rd);
	RelationCacheHashByName = NameCacheSave;
	RelationCacheHashById = IdCacheSave;
	MemoryContextSwitchTo(oldcxt); 
	 
	return (NULL);
    }
    
    fd = relopen(&((RelationTupleForm) GETSTRUCT(tuple))->relname,
		 O_RDWR, 0666);
    
    Assert (fd >= -1);
    if (fd == -1) {
	elog(NOTICE, "BuildRelation: relopen(%s): %m",
	     &((RelationTupleForm)GETSTRUCT(tuple))->relname);
    }
    
    relid = tuple->t_oid;
    relp = (RelationTupleForm) HeapTupleGetForm(tuple);
    numatts = natts = relp->relnatts;
    
    /* ----------------
     *    allocate space for the relation descriptor, including
     *    the index strategy (this is a hack)
     * ----------------
     */
    len = sizeof(RelationData) + 
	((int)natts - 1) * sizeof(relation->rd_att) + /* var len struct */
	    sizeof(IndexStrategy);
    
    relationTupleForm = (RelationTupleForm)
	palloc(sizeof (RuleLock) + sizeof *relation->rd_rel);
    
    /* should use relp->relam to create this */
    
    bcopy((char *)relp, (char *)relationTupleForm, sizeof *relp);
    
    amendscan(sd);
    amclose(rd);
    
    if (!ObjectIdIsValid(relationTupleForm->relam)) {
	relation = (Relation)palloc(len);
	bzero((char *)relation, len);
	relation->rd_rel = relationTupleForm;
	 
    } else {
	AccessMethodTupleForm	accessMethodTupleForm;
	 
	/* ----------------
	 *	ADD INDEXING HERE
	 * ----------------
	 */
	rd = amopenr(AccessMethodRelationName);
	    
	key.data[0].flags = 0;
	key.data[0].attributeNumber = ObjectIdAttributeNumber;
	key.data[0].procedure = ObjectIdEqualRegProcedure;
	key.data[0].argument = ObjectIdGetDatum(relationTupleForm->relam);
	    
	sd = ambeginscan(rd, 0, NowTimeQual, 1, &key);
	    
	tuple = amgetnext(sd, 0, (Buffer *)NULL);
	if (!HeapTupleIsValid(tuple)) {
	    elog(NOTICE, "BuildRelation: %s: unknown AM %d",
		 errorName,
		 relationTupleForm->relam);
	    /*
	      amendscan(sd);
	      amclose(rd);
	      return (NULL);
	      */
	    RelationCacheHashByName = NameCacheSave;
	    RelationCacheHashById =   IdCacheSave;
	}
	accessMethodTupleForm = (AccessMethodTupleForm)
	    palloc(sizeof *relation->rd_am);
	
	bcopy((char *)HeapTupleGetForm(tuple), accessMethodTupleForm,
		  sizeof *accessMethodTupleForm);
	    
	amendscan(sd);
	amclose(rd);
	
	/*
	 * this was removed because the index strategy is now
	 * a pointer and it alrwady has it's space allocated above..
	 *
	 * len += AttributeNumberGetIndexStrategySize(
	 * 		relationTupleForm->relnatts,
	 * 		AMStrategies(accessMethodTupleForm->amstrategies));
	 */
		
	relation = (Relation)palloc(len);
	bzero((char *)relation, len);
	relation->rd_rel = relationTupleForm;
	relation->rd_am = accessMethodTupleForm;
    }
    
    /* ----------------
     *	ADD INDEXING HERE (correctly)
     * ----------------
     */
    key.data[0].flags = 	  0;
    key.data[0].attributeNumber = AttributeRelationIdAttributeNumber;
    key.data[0].procedure = 	  ObjectIdEqualRegProcedure;
    key.data[0].argument = 	  ObjectIdGetDatum(relid);
    
    attrioid = GetAttributeRelationIndexRelationId();
    
    if (ObjectIdIsValid(attrioid) && !avoidIndex) {
	/*
	 * indexed scan of Attribute Relation 
	 */
	IndexScanDesc			scan;
	GeneralRetrieveIndexResult	result;
	Relation			attrd;
	Relation			attrird;
	Buffer				buffer;
	    
	/*
	 * avoidIndex prevents a recursive call to this code.  I think
	 * it would recurse indefinately.  It is okay to have to
	 * do this one scan the long way because the result will
	 * be cached.
	 */
	attrd = RelationIdGetRelation(AttributeRelationId);
	
	avoidIndex = true;
	attrird = RelationIdGetRelation(attrioid);
	
	avoidIndex = false;
	scan = RelationGetIndexScan(attrird, 0, 1, &key);
	    
	for(;;) {	/* Get rid of loop?  Just use first index? */
	    IndexTuple	indexTuple;
		
	    result = IndexScanGetGeneralRetrieveIndexResult(scan, 1);
	    if (! GeneralRetrieveIndexResultIsValid(result))
		break;
	    
	    /*
	     * XXX
	     *
	     * the buffer should be freed.... 
	     *
	     * however, the buffer won't be retured and thus
	     * wont't be freed.  (bug)
	     */
	    tuple = GetHeapTuple(result, attrd, &buffer);
	    if (!HeapTupleIsValid(tuple)) 
		    break;
		
	    attp = (AttributeTupleForm) HeapTupleGetForm(tuple);
	    BuildRelationAttributes(attp, relation, errorName);
	}
	
	IndexScanEnd(scan);
	RelationFree(attrd);
	RelationFree(attrird);
    } else {
	/*
	 * sequential scan of Attribute Relation
	 */
	rd = amopenr(AttributeRelationName);
	sd = ambeginscan(rd, 0, NowTimeQual, 1, &key);
	while (tuple = amgetnext(sd, 0, (Buffer *) NULL),
	       HeapTupleIsValid(tuple)) {
	    
	    attp = (AttributeTupleForm)
		HeapTupleGetForm(tuple);
		    
	    BuildRelationAttributes(attp, relation, errorName);
	}
	amendscan(sd);
	amclose(rd);
    }
    
    while ((int)--natts >= 0) {
	if (!AttributeIsValid(relation->rd_att.data[natts]))
	    elog(NOTICE, "getreldesc: ATTRIBUTE relation corrupted %s", 
		 errorName);
    }
    
    relation->rd_fd = fd;
    relation->rd_nblocks = FileGetNumberOfBlocks(fd);
    
    RelationSetReferenceCount(relation, 1);
    
    if (ObjectIdIsValid(relation->rd_rel->relam)) {
	IndexStrategy strategy;
	int stratSize;
	    
	stratSize =
	    AttributeNumberGetIndexStrategySize(numatts,
						relation->rd_am->amstrategies);
	    
	strategy = (IndexStrategy)
	    palloc(stratSize);
	    
	IndexStrategyInitialize(strategy,
	  RelationGetTupleDescriptor(relation)->data[0]->attrelid,
	  RelationGetRelationTupleForm(relation)->relam,
	  AMStrategies(
	    RelationGetAccessMethodTupleForm(relation)->amstrategies));

	RelationSetIndexStrategy(relation,strategy);
    }
    
    relation->rd_id = relid;
    
    RelationCacheHashByName = NameCacheSave;
    RelationCacheHashById = IdCacheSave;
    
    HashTableInsert(RelationCacheHashByName, relation);
    HashTableInsert(RelationCacheHashById, relation);
    
    RelationInitLockInfo(relation);
	
    MemoryContextSwitchTo(oldcxt);
    
    RelationInitLockInfo(relation);
    
    return relation;
}
 
/* --------------------------------
 *	RelationIdGetRelation
 * --------------------------------
 */
/**** xxref:
 *           BuildRelation
 ****/
Relation
RelationIdGetRelation(relationId)
    ObjectId	relationId;
{
    Relation		rd;
    HeapScanDesc	sd;
    ScanKeyData		key;
    char 		errorName[50];
    
    extern GlobalMemory CacheCxt;
    MemoryContext	oldcxt;
    
    HeapTuple		tuple;
    HashTable		NameCacheSave;
    HashTable		IdCacheSave;
    
    IN();
    
    /* if not in the current cache, check the global cache */
    
    rd = (Relation)
	KeyHashTableLookup(RelationCacheHashById, relationId);
    
    if (!RelationIsValid(rd))
	rd = (Relation) KeyHashTableLookup(GlobalIdCache, relationId);
    
    if (RelationIsValid(rd)) {
	if (rd -> rd_fd == -1) {
	    rd -> rd_fd = relopen (&rd -> rd_rel -> relname,
				   O_RDWR | O_CREAT, 0666);
	    Assert (rd -> rd_fd != -1);
	}
	RelationIncrementReferenceCount(rd);
	
	RelationSetLockForDescriptorOpen(rd);
	
	return rd;
    }
    
    NameCacheSave = 		RelationCacheHashByName;
    IdCacheSave = 		RelationCacheHashById;
    RelationCacheHashByName =	PrivateRelationCacheHashByName;
    RelationCacheHashById = 	PrivateRelationCacheHashById;
    
    oldcxt = MemoryContextSwitchTo(CacheCxt);
    
    rd = amopenr(RelationRelationName);
    
    key.data[0].flags = 	  0;
    key.data[0].attributeNumber = ObjectIdAttributeNumber;
    key.data[0].procedure = 	  ObjectIdEqualRegProcedure;
    key.data[0].argument = 	  ObjectIdGetDatum(relationId);
    
    sd = ambeginscan(rd, 0, NowTimeQual, 1, &key);
    
    tuple = amgetnext(sd, 0, (Buffer *)NULL);
    
    if (!HeapTupleIsValid(tuple)) {
	sprintf(errorName, "RelationId=%d", relationId);
    } else {
	sprintf(errorName, "%s (id: %d)", 
		&((RelationTupleForm)GETSTRUCT(tuple))->relname, 
		relationId);
    }
    
    rd = BuildRelation(rd, sd, errorName, oldcxt, tuple, 
		       NameCacheSave, IdCacheSave);
    
    return rd;
}
 
/* --------------------------------
 *	getreldesc / RelationNameGetRelation
 *
 *	return a relation descriptor based on its name.
 *	return a cached value if possible
 * --------------------------------
 */
 
Relation
getreldesc(relationName)
    Name		relationName;
{
    Relation		rd;
    HeapScanDesc	sd;
    ScanKeyData		key;
    
    extern GlobalMemory CacheCxt;
    MemoryContext	oldcxt;
    
    HeapTuple		tuple;
    HashTable		NameCacheSave;
    HashTable		IdCacheSave;
    
    IN();
    
    /* if not in the current cache, check the global cache */
    
    rd = (Relation)
	KeyHashTableLookup(RelationCacheHashByName,relationName);
    
    if (! RelationIsValid(rd))
	rd = (Relation)
	    KeyHashTableLookup(GlobalNameCache, relationName);
    
    if (RelationIsValid(rd)) {
	if (rd -> rd_fd == -1) {
	    rd -> rd_fd =
		relopen (&rd->rd_rel->relname,  O_RDWR | O_CREAT, 0666);
	    Assert (rd->rd_fd != -1);
	}
	
	RelationIncrementReferenceCount(rd);
	OUT();
	    
	RelationSetLockForDescriptorOpen(rd);
	    
	rd->rd_nblocks = FileGetNumberOfBlocks(rd->rd_fd);
	return rd;
    }
    
    NameCacheSave = 		RelationCacheHashByName;
    IdCacheSave = 		RelationCacheHashById;
    RelationCacheHashByName = 	PrivateRelationCacheHashByName;
    RelationCacheHashById = 	PrivateRelationCacheHashById;
    
    oldcxt = MemoryContextSwitchTo(CacheCxt);
     
    /* ----------------
     *	ADD INDEXING HERE
     * ----------------
     */
    rd = amopenr(RelationRelationName);
    
    key.data[0].flags = 	  0;
    key.data[0].attributeNumber = RelationNameAttributeNumber;
    key.data[0].procedure = 	  Character16EqualRegProcedure;
    key.data[0].argument = 	  NameGetDatum(relationName);
    
    sd = ambeginscan(rd, 0, NowTimeQual, 1, &key);
    
    tuple = amgetnext(sd, 0, (Buffer *)NULL);
    
    rd = BuildRelation(rd, sd, relationName, oldcxt, tuple,
		       NameCacheSave, IdCacheSave);
    
    OUT();
    
    return rd;
}
 
/* ----------------------------------------------------------------
 *		cache invalidation support routines
 * ----------------------------------------------------------------
 */
 
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
    
    IN();
    
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
    
    OUT();
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
    IN();
    HashTableWalk(RelationCacheHashByName, RelationFlushIndexes,
		  accessMethodId);
    OUT();
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
    IN();
    HashTableWalk(RelationCacheHashByName, RelationFlushRelation, 
		  onlyFlushReferenceCountZero);
    OUT();
}
 
/* --------------------------------
 *	RelationFree
 * --------------------------------
 */
/**** xxref:
 *           BuildRelation
 ****/
void
RelationFree(relation)
    Relation	relation;
{
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
 *	formrdesc
 *
 *	this is used to create relation descriptors for
 *	RelationInitialize() below.
 * --------------------------------
 */
/**** xxref:
 *           RelationInitialize
 ****/
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
    
    len = sizeof *relation + ((int)natts - 1) * sizeof relation->rd_att;
    relation = (Relation) palloc(len);
    bzero((char *)relation, len);
    
    relation->rd_fd = -1;
    
    RelationSetReferenceCount(relation, 1);
    
    relation->rd_rel = (RelationTupleForm)
	palloc(sizeof (RuleLock) + sizeof *relation->rd_rel);
    
    strncpy(&relation->rd_rel->relname, relationName, 16);
    relation->rd_rel->relowner = InvalidObjectId;	/* XXX incorrect */
    relation->rd_rel->relpages = 1;			/* XXX */
    relation->rd_rel->reltuples = 1;		/* XXX */
    relation->rd_rel->relhasindex = '\0';
    relation->rd_rel->relkind = 'r';
    relation->rd_rel->relarch = 'n';
    relation->rd_rel->relnatts = (uint16)natts;
    
    for (i = 0; i < natts; i++) {
	relation->rd_att.data[i] = (Attribute)
	    palloc(sizeof (RuleLock) + sizeof *relation->rd_att.data[0]);
	
	bzero((char *)relation->rd_att.data[i], sizeof (RuleLock) +
	      sizeof *relation->rd_att.data[0]);
	bcopy((char *)&att[i], (char *)relation->rd_att.data[i],
	      sizeof *relation->rd_att.data[0]);
    }
    
    relation->rd_id = relation->rd_att.data[0]->attrelid;
    HashTableInsert(PrivateRelationCacheHashByName, relation);
    HashTableInsert(PrivateRelationCacheHashById, relation);
    
    if (initialReferenceCount != 0) {
	publicCopy = (Relation)palloc(len);
	bcopy(relation, publicCopy, len);
	
	RelationSetReferenceCount(publicCopy, initialReferenceCount);
	HashTableInsert(RelationCacheHashByName, publicCopy);
	HashTableInsert(RelationCacheHashById, publicCopy);
	
	RelationInitLockInfo(relation);	/* XXX unuseful here ??? */
    }
}
 
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
 *	RelationInitialize
 * --------------------------------
 */
void 
RelationInitialize()
{
    extern GlobalMemory		CacheCxt;
    MemoryContext		oldcxt;
    int				initialReferences;
    
    IN();
    
    if (!CacheCxt)
	CacheCxt = CreateGlobalMemory("Cache");
    
    oldcxt = MemoryContextSwitchTo(CacheCxt);
    
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
    
    /* 
     * these should use a hash function that is perfect on them
     * and the hash table code should have a way of telling it not
     * to bother trying to resize 'cauase they all fit and there
     * arn't any collisions.  Also, the hash table code could have
     * a HashTableDisableModification call.
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
    
    initialReferences = 0;
    
    if (AMI_OVERRIDE) {
	/* a hack to prevent generation of the special descriptors */
	initialReferences = 0x2000;
    }
    
    formrdesc(RelationRelationName, relatt[0].attrelid, REL_NATTS, relatt,
	      initialReferences);
    
    formrdesc(AttributeRelationName, attatt[0].attrelid, ATT_NATTS,	attatt,
	      initialReferences);
    
    formrdesc(ProcedureRelationName, proatt[0].attrelid, PRO_NATTS,	proatt,
	      initialReferences);
    
    formrdesc(TypeRelationName, typatt[0].attrelid, TYP_NATTS, typatt,
	      initialReferences);
    
    /* ----------------
     * the var, log, and time relations
     * are also cached...
     * ----------------
     */
    formrdesc(VariableRelationName, 
	      varatt[0].attrelid, VAR_NATTS, varatt, 0x1fff);
    
    formrdesc(LogRelationName, 
	      logatt[0].attrelid, LOG_NATTS, logatt, 0x1fff);
    
    formrdesc(TimeRelationName, 
	      timatt[0].attrelid, TIM_NATTS, timatt, 0x1fff);
    
    MemoryContextSwitchTo(oldcxt);
    
    OUT();
}
