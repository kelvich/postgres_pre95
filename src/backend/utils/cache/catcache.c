/* ----------------------------------------------------------------
 * catcache.c --
 *	System catalog cache for tuples matching a key.
 *
 * Notes:
 *	XXX This needs to use exception.h to handle recovery when
 *		an abort occurs during DisableCache.
 *	
 * ----------------------------------------------------------------
 */
#include "tmp/postgres.h"

RcsId("$Header$");

#include "access/heapam.h"
#include "access/htup.h"
#include "access/skey.h"
#include "access/tqual.h"
#include "tmp/miscadmin.h"
#include "tmp/portal.h"
#include "utils/catcache.h"
#include "utils/fmgr.h"		/* for F_BOOLEQ, etc.  DANGER */
#include "utils/lmgr.h"
#include "utils/log.h"
#include "utils/mcxt.h"
#include "utils/rel.h"
#include "catalog/pg_type.h"	/* for OID of int28 type */
 
/* ----------------
 *	variables, macros and other stuff
 *
 *  note CCSIZE allocates 51 buckets .. one was already allocated in
 *  the catcache structure.
 * ----------------
 */

#ifdef CACHEDEBUG
#define CACHE1_elog(a,b)		elog(a,b)
#define CACHE2_elog(a,b,c)		elog(a,b,c)
#define CACHE3_elog(a,b,c,d)		elog(a,b,c,d)
#define CACHE4_elog(a,b,c,d,e)		elog(a,b,c,d,e)
#define CACHE5_elog(a,b,c,d,e,f)	elog(a,b,c,d,e,f)
#define CACHE6_elog(a,b,c,d,e,f,g)	elog(a,b,c,d,e,f,g)
#else
#define CACHE1_elog(a,b)
#define CACHE2_elog(a,b,c)
#define CACHE3_elog(a,b,c,d)
#define CACHE4_elog(a,b,c,d,e)
#define CACHE5_elog(a,b,c,d,e,f)
#define CACHE6_elog(a,b,c,d,e,f,g)
#endif

#define	NCCBUCK	50	/* CatCache buckets 			*/
#define MAXTUP 30	/* Maximum # of tuples cached per cache */
#define LIST	SLList
#define NODE	SLNode
#define CCSIZE	(sizeof(struct catcache) + NCCBUCK * sizeof(SLList))

struct catcache	*Caches = NULL;
GlobalMemory	CacheCxt;
static int	DisableCache;

/* ----------------
 *	EQPROC is used in CatalogCacheInitializeCache
 * 	XXX this should be replaced by catalog lookups soon
 * ----------------
 */
static long	eqproc[] = {
    F_BOOLEQ, 0l, F_CHAREQ, F_CHAR16EQ, 0l,
    F_INT2EQ, F_KEYFIRSTEQ, F_INT4EQ, 0l, F_TEXTEQ,
    F_OIDEQ, 0l, 0l, 0l, 0l
};

#define	EQPROC(SYSTEMTYPEOID)	eqproc[(SYSTEMTYPEOID)-16]
    ;

/* ----------------------------------------------------------------
 *		    internal support functions
 * ----------------------------------------------------------------
 */
/* --------------------------------
 *	CatalogCacheInitializeCache
 * --------------------------------
 */
#ifdef CACHEDEBUG
#define CatalogCacheInitializeCache_DEBUG1 \
    elog(DEBUG, "CatalogCacheInitializeCache: cache @%08lx", cache); \
    if (relation) \
        elog(DEBUG, "CatalogCacheInitializeCache: called w/relation(inval)"); \
    else \
        elog(DEBUG, "CatalogCacheInitializeCache: called w/relname %s", \
	    cache->cc_relname)
#define CatalogCacheInitializeCache_DEBUG2 \
        if (cache->cc_key[i] > 0) { \
            elog(DEBUG, "CatalogCacheInitializeCache: load %d/%d w/%d, %d", \
	        i+1, cache->cc_nkeys, cache->cc_key[i], \
	        relation->rd_att.data[cache->cc_key[i] - 1]->attlen); \
	} else { \
            elog(DEBUG, "CatalogCacheInitializeCache: load %d/%d w/%d", \
	        i+1, cache->cc_nkeys, cache->cc_key[i]); \
	}
#else
#define CatalogCacheInitializeCache_DEBUG1
#define CatalogCacheInitializeCache_DEBUG2
#endif

/**** xxref:
 *           CatalogCacheComputeTupleHashIndex
 *           SearchSysCache
 ****/
void
CatalogCacheInitializeCache(cache, relation)
    struct catcache *cache;
    Relation relation;
{
    MemoryContext	oldcxt;
    short didopen = 0;
    short i;
    
    CatalogCacheInitializeCache_DEBUG1;
    
    /* ----------------
     *	first switch to the cache context so our allocations
     *  do not vanish at the end of a transaction
     * ----------------
     */
    if (!CacheCxt)
       CacheCxt = CreateGlobalMemory("Cache");
    oldcxt = MemoryContextSwitchTo(CacheCxt);
    
    /* ----------------
     *  If no relation was passed we must open it to get access to 
     *  its fields.  If one of the other caches has already opened
     *  it we can use the much faster ObjectIdOpenHeapRelation() call
     *  rather than RelationNameOpenHeapRelation() call.
     * ----------------
     */
    if (! RelationIsValid(relation)) {
	struct catcache *cp;
	/* ----------------
	 *  scan the caches to see if any other cache has opened the relation
	 * ----------------
	 */
        for (cp = Caches; cp; cp = cp->cc_next) {
	    if (strcmp(cp->cc_relname, cache->cc_relname) == 0) {
		if (cp->relationId != InvalidObjectId)
		    break;
	    }
	}
	
	/* ----------------
	 *  open the relation by name or by id
	 * ----------------
	 */
	if (cp) {
	    CACHE1_elog(DEBUG, "CatalogCacheInitializeCache: fastrelopen");
    	    relation = ObjectIdOpenHeapRelation(cp->relationId);
	} else {
    	    relation = RelationNameOpenHeapRelation(cache->cc_relname);
	}
	didopen = 1;
    }
    
    /* ----------------
     *	initialize the cache's relation id
     * ----------------
     */
    Assert(RelationIsValid(relation));
    cache->relationId = RelationGetRelationId(relation);
    
    CACHE3_elog(DEBUG, "CatalogCacheInitializeCache: relid %d, %d keys", 
		cache->relationId, cache->cc_nkeys);
    
    for (i = 0; i < cache->cc_nkeys; ++i) {
	
	CatalogCacheInitializeCache_DEBUG2;
	
	if (cache->cc_key[i] > 0) {

	    /*
	     *  Yoiks.  The implementation of the hashing code and the
	     *  implementation of int28's are at loggerheads.  The right
	     *  thing to do is to throw out the implementation of int28's
	     *  altogether; until that happens, we do the right thing here
	     *  to guarantee that the hash key generator doesn't try to
	     *  dereference an int2 by mistake.
	     */

	    if (relation->rd_att.data[cache->cc_key[i]-1]->atttypid == INT28OID)
		cache->cc_klen[i] = sizeof (short);
	    else
		cache->cc_klen[i] = 
		    relation->rd_att.data[cache->cc_key[i]-1]->attlen;

	    cache->cc_skey[i].sk_opr =
	        EQPROC(relation->rd_att.data[cache->cc_key[i]-1]->atttypid);
	    
	    CACHE5_elog(DEBUG, "CatalogCacheInit %16s %d %d %x",
			&relation->rd_rel->relname,
			i,
			relation->rd_att.data[cache->cc_key[i]-1]->attlen,
			cache);
	}
    }
    
    /* ----------------
     *	close the relation if we opened it
     * ----------------
     */
    if (didopen)
        RelationCloseHeapRelation(relation);
    
    /* ----------------
     *	return to the proper memory context
     * ----------------
     */
    MemoryContextSwitchTo(oldcxt);
}
  
/* --------------------------------
 *	CatalogCacheSetId
 *
 * 	XXX temporary function
 * --------------------------------
 */
/**** xxref:
 *           InitCatalogCache
 *           SearchSysCacheTuple
 ****/
void
CatalogCacheSetId(cacheInOutP, id)
    CatCache *cacheInOutP;
    int      id;
{
    Assert(id == InvalidCatalogCacheId || id >= 0);
    cacheInOutP->id = id;
}
 
/* ----------------
 * comphash --
 *	Compute a hash value, somehow.
 *
 * XXX explain algorithm here.
 *
 * l is length of the attribute value, v
 * v is the attribute value ("Datum")
 * ----------------
 */
/**** xxref:
 *           CatalogCacheComputeHashIndex
 ****/
comphash(l, v)
    int			l;
    register char	*v;
{
    register int  i;
    CACHE3_elog(DEBUG, "comphash (%d,%x)", l, v);
    
    switch (l) {
    case 1:
    case 2:
    case 4:
	return((int) v);
    }

    /* special hack for char16 values */
    if (l == 16)
	l = NameComputeLength((Name)v);
    else if (l < 0)
	l = PSIZE(v);

    i = 0;
    while (l--) {
	i += *v++;
    }
    return(i);
}
/* --------------------------------
 *	CatalogCacheComputeHashIndex
 * --------------------------------
 */

/**** xxref:
 *           CatalogCacheComputeTupleHashIndex
 *           SearchSysCache
 ****/
Index
CatalogCacheComputeHashIndex(cacheInP)
    struct catcache *cacheInP;
{
    Index	hashIndex;
    hashIndex = 0x0;
    CACHE6_elog(DEBUG,"CatalogCacheComputeHashIndex %s %d %d %d %x",
	 cacheInP->cc_relname,
	 cacheInP->cc_nkeys,
	 cacheInP->cc_klen[0],
	 cacheInP->cc_klen[1],
	 cacheInP);
    
    switch (cacheInP->cc_nkeys) {
    case 4:
	hashIndex ^= comphash(cacheInP->cc_klen[3],
		cacheInP->cc_skey[3].sk_data) << 9;
	/* FALLTHROUGH */
    case 3:
	hashIndex ^= comphash(cacheInP->cc_klen[2],
		cacheInP->cc_skey[2].sk_data) << 6;
	/* FALLTHROUGH */
    case 2:
	hashIndex ^= comphash(cacheInP->cc_klen[1],
		cacheInP->cc_skey[1].sk_data) << 3;
	/* FALLTHROUGH */
    case 1:
	hashIndex ^= comphash(cacheInP->cc_klen[0],
		cacheInP->cc_skey[0].sk_data);
	break;
    default:
	elog(FATAL, "CCComputeHashIndex: %d cc_nkeys", cacheInP->cc_nkeys);
	break;
    }
    hashIndex %= cacheInP->cc_size;
    return (hashIndex);
}

/* --------------------------------
 *	CatalogCacheComputeTupleHashIndex
 * --------------------------------
 */
/**** xxref:
 *           RelationInvalidateCatalogCacheTuple
 ****/
Index
CatalogCacheComputeTupleHashIndex(cacheInOutP, relation, tuple)
    struct catcache	*cacheInOutP;
    Relation		relation;
    HeapTuple		tuple;
{
    Boolean		isNull = '\0';
    extern DATUM	fastgetattr();
    if (cacheInOutP->relationId == InvalidObjectId)
	CatalogCacheInitializeCache(cacheInOutP, relation);
    switch (cacheInOutP->cc_nkeys) {
    case 4:
	cacheInOutP->cc_skey[3].sk_data =
	    (cacheInOutP->cc_key[3] == ObjectIdAttributeNumber)
		? (DATUM) tuple->t_oid
		: fastgetattr(tuple,
			      cacheInOutP->cc_key[3],
			      &relation->rd_att.data[0],
			      &isNull);
	Assert(!isNull);
	/* FALLTHROUGH */
    case 3:
	cacheInOutP->cc_skey[2].sk_data =
	    (cacheInOutP->cc_key[2] == ObjectIdAttributeNumber)
		? (DATUM) tuple->t_oid
		: fastgetattr(tuple,
			      cacheInOutP->cc_key[2],
			      &relation->rd_att.data[0],
			      &isNull);
	Assert(!isNull);
	/* FALLTHROUGH */
    case 2:
	cacheInOutP->cc_skey[1].sk_data =
	    (cacheInOutP->cc_key[1] == ObjectIdAttributeNumber)
		? (DATUM) tuple->t_oid
		: fastgetattr(tuple,
			      cacheInOutP->cc_key[1],
			      &relation->rd_att.data[0],
			      &isNull);
	Assert(!isNull);
	/* FALLTHROUGH */
    case 1:
	cacheInOutP->cc_skey[0].sk_data =
	    (cacheInOutP->cc_key[0] == ObjectIdAttributeNumber)
		? (DATUM) tuple->t_oid
		: fastgetattr(tuple,
			      cacheInOutP->cc_key[0],
			      &relation->rd_att.data[0],
			      &isNull);
	Assert(!isNull);
	break;
    default:
	elog(FATAL, "CCComputeTupleHashIndex: %d cc_nkeys",
	    cacheInOutP->cc_nkeys
	);
	break;
    }
    
    return
	CatalogCacheComputeHashIndex(cacheInOutP);
}

/* --------------------------------
 *	CatCacheRemoveCTup
 * --------------------------------
 */

/**** xxref:
 *           CatalogCacheIdInvalidate
 *           ResetSystemCache
 *           SearchSysCache
 ****/
void
CatCacheRemoveCTup(cache, ct)
    CatCache *cache;
    CatCTup  *ct;
{
    SLRemove(&ct->ct_node);
    SLRemove(&ct->ct_lrunode);
    if (RuleLockIsValid(ct->ct_tup->t_lock.l_lock)) 
        pfree((char *)ct->ct_tup->t_lock.l_lock);
    pfree((char *) ct->ct_tup);
    pfree((char *)ct);
    --cache->cc_ntup;
}

/* --------------------------------
 *  CatalogCacheIdInvalidate()
 *
 *  Invalidate a tuple given a cache id.  In this case the id should always
 *  be found (whether the cache has opened its relation or not).  Of course,
 *  if the cache has yet to open its relation, there will be no tuples so
 *  no problem.
 * --------------------------------
 */
/**** xxref:
 *           CacheIdInvalidate
 *           InvalidationMessageCacheInvalidate
 ****/
void
CatalogCacheIdInvalidate(cacheId, hashIndex, pointer)
    int		cacheId;	/* XXX */
    Index	hashIndex;
    ItemPointer	pointer;
{
    struct catcache 	*ccp;
    struct catctup	*ct, *hct;
    MemoryContext	oldcxt;
    
    /* ----------------
     *	sanity checks
     * ----------------
     */
    Assert(IndexIsValid(hashIndex) && IndexIsInBounds(hashIndex, NCCBUCK));
    Assert(ItemPointerIsValid(pointer));
    CACHE1_elog(DEBUG, "CatalogCacheIdInvalidate: called");
    
    /* ----------------
     *	switch to the cache context for our memory allocations
     * ----------------
     */
    if (!CacheCxt)
       CacheCxt = CreateGlobalMemory("Cache");
    oldcxt = MemoryContextSwitchTo(CacheCxt);
    
    /* ----------------
     *	inspect every cache that could contain the tuple
     * ----------------
     */
    for (ccp = Caches; ccp; ccp = ccp->cc_next) {
	if (cacheId != ccp->id) 
    	    continue;
	/* ----------------
	 *  inspect the hash bucket until we find a match or exhaust
	 * ----------------
	 */
	for (ct = (CatCTup *) SLGetHead(&ccp->cc_cache[hashIndex]);
	     ct;
	     ct = (CatCTup *) SLGetSucc(&ct->ct_node))
	{
	    if (ItemPointerEquals(pointer, &ct->ct_tup->t_ctid)) 
		break;
 	}
	
	/* ----------------
	 *  if we found a matching tuple, invalidate it.
	 * ----------------
	 */
	if (ct) {
	    CatCacheRemoveCTup(ccp, ct);
	    
	    CACHE1_elog(DEBUG, "CatalogCacheIdInvalidate: invalidated");
	}
	
	if (cacheId != InvalidCatalogCacheId) 
	    break;
    }
    
    /* ----------------
     *	return to the proper memory context
     * ----------------
     */
    MemoryContextSwitchTo(oldcxt);
    /*	sendpm('I', "Invalidated tuple"); */
}

/* ----------------------------------------------------------------
 *		       public functions
 *
 *	ResetSystemCache
 *	InitSysCache
 *  	SearchSysCache
 *  	RelationInvalidateCatalogCacheTuple
 * ----------------------------------------------------------------
 */
/* --------------------------------
 *	ResetSystemCache
 * --------------------------------
 */
/**** xxref:
 *           ResetSystemCaches
 ****/

void
ResetSystemCache()
{
    MemoryContext	oldcxt;
    struct catcache	*cache;
    
    /* ----------------
     *	sanity checks
     * ----------------
     */
    CACHE1_elog(DEBUG, "ResetSystemCache called");
    if (DisableCache) {
	elog(WARN, "ResetSystemCache: Called while cache disabled");
	return;
    }
    
    /* ----------------
     *	first switch to the cache context so our allocations
     *  do not vanish at the end of a transaction
     * ----------------
     */
    if (!CacheCxt)
       CacheCxt = CreateGlobalMemory("Cache");
    
    oldcxt = MemoryContextSwitchTo(CacheCxt);
    
    /* ----------------
     *  here we purge the contents of all the caches
     *
     *	for each system cache
     *     for each hash bucket
     *         for each tuple in hash bucket
     *		   remove the tuple
     * ----------------
     */
    for (cache = Caches; PointerIsValid(cache); cache = cache->cc_next) {
        register int hash;
	for (hash = 0; hash < NCCBUCK; hash += 1) {
	    LIST *list = &cache->cc_cache[hash];
	    register CatCTup *ct, *nextct;
	    for (ct = (CatCTup *) SLGetHead(list); ct; ct = nextct) {
		nextct = (CatCTup *)SLGetSucc(&ct->ct_node);
		CatCacheRemoveCTup(cache, ct);
		
	        if (cache->cc_ntup == -1) 
		    elog(WARN, "ResetSystemCache: cc_ntup<0 (software error)");
	    }
	}
	cache->cc_ntup = 0; /* in case of WARN error above */
    }
    
    CACHE1_elog(DEBUG, "end of ResetSystemCache call");
    
    /* ----------------
     *	back to the old context before we return...
     * ----------------
     */
    MemoryContextSwitchTo(oldcxt);
}
 
/* --------------------------------
 *	InitSysCache
 *
 *  This allocates and initializes a cache for a system catalog relation.
 *  Actually, the cache is only partially initialized to avoid opening the
 *  relation.  The relation will be opened and the rest of the cache
 *  structure initialized on the first access.
 * --------------------------------
 */
#ifdef CACHEDEBUG
#define InitSysCache_DEBUG1 \
    elog(DEBUG, "InitSysCache: rid=%d id=%d nkeys=%d size=%d\n", \
	 cp->relationId, cp->id, cp->cc_nkeys, cp->cc_size); \
    for (i = 0; i < nkeys; i += 1) { \
        elog(DEBUG, "InitSysCache: key=%d len=%d skey=[%d %d %d %d]\n", \
            cp->cc_key[i], cp->cc_klen[i], \
            cp->cc_skey[i].sk_flags, \
            cp->cc_skey[i].sk_attnum, \
            cp->cc_skey[i].sk_opr, \
            cp->cc_skey[i].sk_data); \
    }
#else
#define InitSysCache_DEBUG1
#endif
    
/**** xxref:
 *           InitCatalogCache
 *           SearchSysCacheTuple
 ****/
struct catcache *
InitSysCache(relname, nkeys, key)
    char	*relname;
    int		nkeys;
    int		key[];
{
    struct catcache     *cp;
    register int        i;
    MemoryContext	oldcxt;
    
    /* ----------------
     *	first switch to the cache context so our allocations
     *  do not vanish at the end of a transaction
     * ----------------
     */
    if (!CacheCxt)
       CacheCxt = CreateGlobalMemory("Cache");
    
    oldcxt = MemoryContextSwitchTo(CacheCxt);
    
    /* ----------------
     *  allocate a new cache structure
     * ----------------
     */
    cp = (struct catcache *) palloc(CCSIZE);
    bzero((char *)cp, CCSIZE);
    
    /* ----------------
     *	initialize the cache buckets (each bucket is a list header)
     *  and the LRU tuple list
     * ----------------
     */
    for (i = 0; i <= NCCBUCK; ++i)
	SLNewList(&cp->cc_cache[i], offsetof(struct catctup, ct_node));
    
    SLNewList(&cp->cc_lrulist, offsetof(struct catctup, ct_lrunode));
    
    /* ----------------
     *  Caches is the pointer to the head of the list of all the
     *  system caches.  here we add the new cache to the top of the list.
     * ----------------
     */
    cp->cc_next = Caches;  /* list of caches (single link) */
    Caches = cp;
    
    /* ----------------
     *	initialize the cache's relation information for the relation
     *  corresponding to this cache and initialize some of the the new
     *  cache's other internal fields.
     * ----------------
     */
    cp->relationId = 	InvalidObjectId;
    cp->cc_relname = 	relname;
    cp->id = 		InvalidCatalogCacheId;	/* XXX should be an argument */
    cp->cc_maxtup = 	MAXTUP;
    cp->cc_size = 	NCCBUCK;
    cp->cc_nkeys = 	nkeys;
    
    /* ----------------
     *	initialize the cache's key information
     * ----------------
     */
    for (i = 0; i < nkeys; ++i) {
	cp->cc_key[i] = key[i];
	if (!key[i]) {
	    elog(FATAL, "InitSysCache: called with 0 key[%d]", i);
	}
	if (key[i] < 0) {
	    if (key[i] != ObjectIdAttributeNumber) {
		elog(FATAL, "InitSysCache: called with %d key[%d]", key[i], i);
	    } else {
		cp->cc_klen[i] = sizeof(OID);
		cp->cc_skey[i].sk_attnum = key[i];
		cp->cc_skey[i].sk_opr = F_OIDEQ;
		continue;
	    }
	}
	
	cp->cc_skey[i].sk_attnum = key[i];
    }
    
    /* ----------------
     *	all done.  new cache is initialized.  print some debugging
     *  information, if appropriate.
     * ----------------
     */
    InitSysCache_DEBUG1;
    
    /* ----------------
     *	back to the old context before we return...
     * ----------------
     */
    MemoryContextSwitchTo(oldcxt);
    return(cp);
}
 
/* --------------------------------
 *  	SearchSysCache
 *
 *  	This call searches a system cache for a tuple, opening the relation
 *  	if necessary (the first access to a particular cache).
 * --------------------------------
 */
/**** xxref:
 *           SearchSysCacheTuple
 ****/
HeapTuple
SearchSysCache(cache, v1, v2, v3, v4)
    register struct catcache	*cache;
    DATUM			v1, v2, v3, v4;
{
    register unsigned	hash;
    register CatCTup	*ct;
    HeapTuple		ntp;
    Buffer		buffer;
    HeapTuple		palloctup();
    struct catctup	*nct;
    HeapScanDesc	sd;
    Relation		relation;
    MemoryContext	oldcxt;
    
    /* ----------------
     *	sanity checks
     * ----------------
     */
    if (cache->relationId == InvalidObjectId)
	CatalogCacheInitializeCache(cache, NULL);
    
    if (DisableCache) {
	elog(WARN, "SearchSysCache: Called while cache disabled");
	return((HeapTuple) NULL);
    }
    
    /* ----------------
     *	open the relation associated with the cache
     *
     *	XXX this is inefficient! we only need the relation now 
     *      for the schema.  We should recode things to avoid
     *	    opening the relation unless it is really necessary.
     *	    (like when the initial cache search fails) -cim 10/2/90
     * ----------------
     */
    relation = ObjectIdOpenHeapRelation(cache->relationId);
    CACHE2_elog(DEBUG, "SearchSysCache(%s)",
		RelationGetRelationName(relation));
    
    /* ----------------
     *	initialize the search key information
     * ----------------
     */
    cache->cc_skey[0].sk_data = v1;
    cache->cc_skey[1].sk_data = v2;
    cache->cc_skey[2].sk_data = v3;
    cache->cc_skey[3].sk_data = v4;
    
    /* ----------------
     *	find the hash bucket in which to look for the tuple
     * ----------------
     */
    hash = CatalogCacheComputeHashIndex(cache);
    
    /* ----------------
     *	scan the hash bucket until we find a match or exhaust our tuples
     * ----------------
     */
    for (ct = (CatCTup *)SLGetHead(&cache->cc_cache[hash]); 
	 ct;
	 ct = (CatCTup *)SLGetSucc(&ct->ct_node))
    {
	/* ----------------
	 *  see if the cached tuple matches our key.
	 *  (should we be worried about time ranges? -cim 10/2/90)
	 * ----------------
	 */
	if (keytest(ct->ct_tup, relation, cache->cc_nkeys, cache->cc_skey))
	    break;
    }
    
    /* ----------------
     *	if we found a tuple in the cache, move it to the top of the
     *  lru list, and return it.
     * ----------------
     */
    if (ct) {
	SLRemove(&ct->ct_lrunode);		/* most recently used */
	SLAddHead(&cache->cc_lrulist, &ct->ct_lrunode);
	
	CACHE3_elog(DEBUG, "SearchSysCache(%s): found in bucket %d",
		    RelationGetRelationName(relation), hash);
	
	/*
	 * XXX race condition with cache invalidation ???
	 */
	RelationSetLockForRead(relation);
	RelationCloseHeapRelation(relation);
	
	return
	    (ct->ct_tup);
    }
    
    /* ----------------
     *	Tuple was not found in cache, so we have to try and
     *  retrieve it directly from the relation.  If it's found,
     *  we add it to the cache.
     *
     *  First DisableCache and then switch to the cache memory context.
     * ----------------
     */
    DisableCache = 1;
        
    if (!CacheCxt)
       CacheCxt = CreateGlobalMemory("Cache");
    
    oldcxt = MemoryContextSwitchTo(CacheCxt);
   
    /* ----------------
     *	now preform a scan of the relation
     *  ADD INDEXING HERE.
     * ----------------
     */
    CACHE2_elog(DEBUG, "SearchSysCache: performing scan (override==%d)",
		heapisoverride());
    
    sd =  ambeginscan(relation, 0, NowTimeQual,
		      cache->cc_nkeys, cache->cc_skey);
    
    ntp = amgetnext(sd, 0, &buffer);
    
    if (HeapTupleIsValid(ntp)) {
	CACHE1_elog(DEBUG, "SearchSysCache: found tuple");
	ntp = palloctup(ntp, buffer, relation);
    }
    
    amendscan(sd);
        
    DisableCache = 0;
    
    /* ----------------
     *	scan is complete.  if tup is valid, we copy it and add the copy to
     *  the cache.
     * ----------------
     */
    
    if (HeapTupleIsValid(ntp)) {
	/* ----------------
	 *  allocate a new cache tuple holder, store the pointer
	 *  to the heap tuple there and initialize the list pointers.
	 * ----------------
	 */
	nct = (struct catctup *)
	    palloc(sizeof(struct catctup));
    
	nct->ct_tup = ntp;
	
	SLNewNode(&nct->ct_node);
	SLNewNode(&nct->ct_lrunode);
	SLAddHead(&cache->cc_lrulist, &nct->ct_lrunode);
	SLAddHead(&cache->cc_cache[hash], &nct->ct_node);
	
	/* ----------------
	 *  deal with hash bucket overflow
	 * ----------------
	 */
	if (++cache->cc_ntup > cache->cc_maxtup) {
	    register CatCTup *ct;
	    ct = (CatCTup *) SLGetTail(&cache->cc_lrulist);
	    
	    if (ct != nct) {
 		CACHE2_elog(DEBUG, "SearchSysCache(%s): Overflow, LRU removal",
			    RelationGetRelationName(relation));
		
		CatCacheRemoveCTup(cache, ct);
	    }
	} 
	
	CACHE4_elog(DEBUG, "SearchSysCache(%s): Contains %d/%d tuples",
		    RelationGetRelationName(relation),
		    cache->cc_ntup, cache->cc_maxtup);
	CACHE3_elog(DEBUG, "SearchSysCache(%s): put in bucket %d",
		    RelationGetRelationName(relation), hash);
    }
    
    /* ----------------
     *	close the relation, switch back to the original memory context
     *  and return the tuple we found (or NULL)
     * ----------------
     */
    RelationCloseHeapRelation(relation);
   
    MemoryContextSwitchTo(oldcxt);
    return ntp;
}
 
/* --------------------------------
 *  RelationInvalidateCatalogCacheTuple()
 *
 *  Invalidate a tuple from a specific relation.  This call determines the
 *  cache in question and calls CatalogCacheIdInvalidate().  It is -ok-
 *  if the relation cannot be found, it simply means this backend has yet
 *  to open it.
 * --------------------------------
 */

/**** xxref:
 *           RelationInvalidateHeapTuple
 ****/
void
RelationInvalidateCatalogCacheTuple(relation, tuple, function)
    Relation	relation;
    HeapTuple	tuple;
    void	(*function)();
{
    struct catcache 	*ccp;
    MemoryContext	oldcxt;
    ObjectId		relationId;

    /* ----------------
     *	sanity checks
     * ----------------
     */
    Assert(RelationIsValid(relation));
    Assert(HeapTupleIsValid(tuple));
    CACHE1_elog(DEBUG, "RelationInvalidateCatalogCacheTuple: called");
    
    /* ----------------
     *	switch to the cache memory context
     * ----------------
     */
    if (!CacheCxt)
       CacheCxt = CreateGlobalMemory("Cache");
    oldcxt = MemoryContextSwitchTo(CacheCxt);
    
    /* ----------------
     *	for each cache
     *     if the cache contains tuples from the specified relation
     *	       call the invalidation function on the tuples
     *	       in the proper hash bucket
     * ----------------
     */
    relationId = RelationGetRelationId(relation);
    
    for (ccp = Caches; ccp; ccp = ccp->cc_next) {
        if (relationId != ccp->relationId) 
	    continue;
	
	/* OPT inline simplification of CatalogCacheIdInvalidate */
	if (!PointerIsValid(function)) {
	    function = CatalogCacheIdInvalidate;
	}
	
	(*function)(ccp->id,
		    CatalogCacheComputeTupleHashIndex(ccp, relation, tuple),
		    &tuple->t_ctid);
	RelationCloseHeapRelation(relation);
    }
    
    /* ----------------
     *	return to the proper memory context
     * ----------------
     */
    MemoryContextSwitchTo(oldcxt);
    
    /*	sendpm('I', "Invalidated tuple"); */
}

