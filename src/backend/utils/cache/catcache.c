/*
 * catcache.c --
 *	System catalog cache for tuples matching a key.
 *
 * Notes:
 *	XXX This needs to use exception.h to handle recovery when
 *		an abort occurs during DisableCache.
 */

#include "c.h"

RcsId("$Header$");

#include "access.h"
#include "catalog.h"
#include "fmgr.h"	/* for F_BOOLEQ, etc.  DANGER */
#include "lmgr.h"
#include "log.h"
#include "heapam.h"
#include "htup.h"
#include "mcxt.h"
#include "name.h"
#include "portal.h"
#include "tqual.h"
#include "oid.h"
#include "os.h"		/* for etext */
#include "rel.h"

#include "catcache.h"

/* #define CACHEDEBUG */
/* #define CACHEDDEBUG2 */
/* #define CACHEDEBUG3 */


/*
 *  note CCSIZE allocates 51 buckets .. one was already allocated in
 *  the catcache structure.
 */

#define	NCCBUCK	50	/* CatCache buckets 			*/
#define MAXTUP 30	/* Maximum # of tuples cached per cache */
#define LIST	SLList
#define NODE	SLNode
#define CCSIZE	(sizeof(struct catcache) + NCCBUCK * sizeof(SLList))

struct catcache	*Caches = NULL;
GlobalMemory	CacheCxt;

static int	DisableCache;

/* XXX this should be replaced by catalog lookups soon */
static long	eqproc[] = {
    F_BOOLEQ, 0l, F_CHAREQ, F_CHAR16EQ, 0l,
    F_INT2EQ, F_KEYFIRSTEQ, F_INT4EQ, 0l, F_TEXTEQ,
    F_OIDEQ, 0l, 0l, 0l, 0l
};

#define	EQPROC(SYSTEMTYPEOID)	eqproc[(SYSTEMTYPEOID)-16]

/*
 * CatalogCacheComputeHashIndex --
 */

/* static */
Index
CatalogCacheComputeHashIndex ARGS((
	struct catcache	*cacheInP;
));

void CatCacheRemoveCTup();
void CatalogCacheInitializeCache();

/*
 *  This allocates and initializes a cache for a system catalog relation.
 *  Actually, the cache is only partially initialized to avoid opening the
 *  relation.  The relation will be opened and the rest of the cache
 *  structure initialized on the first access.
 */

struct catcache *
InitSysCache(relname, nkeys, key)
char	*relname;
int	nkeys;
int	key[];
{
    struct catcache     *cp;
    register int        i;
    MemoryContext	oldcxt;

    /* REMOVED (DEFERED)
     *Relation		relation;
     */
 
    /* initialize the cache system */
        
    if (!CacheCxt)
       CacheCxt = CreateGlobalMemory("Cache");
    
    oldcxt = MemoryContextSwitchTo(CacheCxt);

    cp = LintCast(struct catcache *, palloc(CCSIZE));
    bzero((char *)cp, CCSIZE); for (i = 0; i <= NCCBUCK; ++i)	/* each bucket is a list header */ SLNewList(&cp->cc_cache[i], offsetof(struct catctup, ct_node));

    					/* list of tuples for LRU alg.	*/
    SLNewList(&cp->cc_lrulist, offsetof(struct catctup, ct_lrunode));
    cp->cc_next = Caches;		/* list of caches (single link) */
    Caches = cp;

    /* REMOVED (now defering open)
     * relation = RelationNameOpenHeapRelation(relname);
     * cp->relationId = RelationGetRelationId(relation);
     */
    cp->relationId = InvalidObjectId;
    cp->cc_relname = relname;

    cp->id = InvalidCatalogCacheId;	/* XXX this should be an argument */
    cp->cc_maxtup = MAXTUP;
    cp->cc_nkeys = nkeys;
    for (i = 0; i < nkeys; ++i) {
	cp->cc_key[i] = key[i];
	if (!key[i]) {
	    elog(FATAL, "InitSysCache: called with 0 key[%d]", i);
	}
	if (key[i] < 0) {
	    if (key[i] != T_OID) {
		elog(FATAL, "InitSysCache: called with %d key[%d]", key[i], i);
	    } else {
		cp->cc_klen[i] = sizeof(OID);
		cp->cc_skey[i].sk_attnum = key[i];
		cp->cc_skey[i].sk_opr = F_OIDEQ;
		continue;
	    }
	}
	/* REMOVED (DEFERED)
	 * cp->cc_klen[i] = relation->rd_att.data[key[i] - 1]->attlen;
	 */
	cp->cc_skey[i].sk_attnum = key[i];

	/* XXX Replace me with a catalog lookup! */

	/* REMOVED (DEFERED)
	 *cp->cc_skey[i].sk_opr =
	 *   EQPROC(relation->rd_att.data[key[i]-1]->atttypid);
	 */
    }
    cp->cc_size = NCCBUCK;

#ifdef CACHEDEBUG
    elog(DEBUG, "InitSysCache: rid=%d id=%d nkeys=%d size=%d\n",
		cp->relationId, cp->id, cp->cc_nkeys, cp->cc_size);
    for (i = 0; i < nkeys; i += 1) {
        elog(DEBUG, "InitSysCache: key=%d len=%d skey=[%d %d %d %d]\n",
            cp->cc_key[i], cp->cc_klen[i], 
            cp->cc_skey[i].sk_flags,
            cp->cc_skey[i].sk_attnum,
            cp->cc_skey[i].sk_opr,
            cp->cc_skey[i].sk_data
	);
    }
#endif
    /* REMOVED (DEFERED)
     * RelationCloseHeapRelation(relation);
     */
   
    MemoryContextSwitchTo(oldcxt);

    return(cp);
}

void
ResetSystemCache()
{
    MemoryContext	oldcxt;
    struct catcache	*cache;

#ifdef	CACHEDEBUG
    elog(DEBUG, "ResetSystemCache called");
#endif

    if (DisableCache) {
	elog(WARN, "ResetSystemCache: Called while cache disabled");
	return;
    }

    /* initialize the cache system */

    if (!CacheCxt)
       CacheCxt = CreateGlobalMemory("Cache");
    
    oldcxt = MemoryContextSwitchTo(CacheCxt);

    for (cache = Caches; PointerIsValid(cache); cache = cache->cc_next) {
        register int hash;
	for (hash = 0; hash < NCCBUCK; hash += 1) {
	    LIST *list = &cache->cc_cache[hash];
	    register CatCTup *ct, *nextct;

	    for (ct = (CatCTup *)SLGetHead(list); ct; ct = nextct) {
		nextct = (CatCTup *)SLGetSucc(&ct->ct_node);

		CatCacheRemoveCTup(cache, ct);
	        if (cache->cc_ntup == -1) 
		    elog(WARN, "ResetSystemCache: cc_ntup<0 (software error)");
	    }
	}
	cache->cc_ntup = 0; /* in case of WARN error above */
    }
#ifdef	CACHEDEBUG
    elog(DEBUG, "end of ResetSystemCache call");
#endif
    
    MemoryContextSwitchTo(oldcxt);
}

/*
 *  SEARCHSYSCACHE
 *
 *  This call searches a system cache for a tuple, opening the relation
 *  if necessary (the first access to a particular cache).
 */

HeapTuple
SearchSysCache(cache, v1, v2, v3, v4)
register struct catcache	*cache;
DATUM				v1, v2, v3, v4;
{
    register unsigned	hash;
    register CatCTup	*ct;
    HeapTuple		ntp;
    Buffer		buffer;
    HeapTuple		palloctup();
    struct catctup	*nct;
    HeapScanDesc		sd;
    Relation		relation;
    MemoryContext	oldcxt;

    if (cache->relationId == InvalidObjectId)
	CatalogCacheInitializeCache(cache, NULL);
    if (DisableCache) {
	elog(WARN, "SearchSysCache: Called while cache disabled");
	return((HeapTuple) NULL);
    }

    relation = ObjectIdOpenHeapRelation(cache->relationId);

#ifdef	CACHEDEBUG
    elog(DEBUG, "SearchSysCache(%s)", RelationGetRelationName(relation));
#endif

    cache->cc_skey[0].sk_data = v1;
    cache->cc_skey[1].sk_data = v2;
    cache->cc_skey[2].sk_data = v3;
    cache->cc_skey[3].sk_data = v4;

    hash = CatalogCacheComputeHashIndex(cache);

    /* XXX keytest arguments */
    for (ct = (CatCTup *)SLGetHead(&cache->cc_cache[hash]); 
	 ct;
	 ct = (CatCTup *)SLGetSucc(&ct->ct_node)
    ) {
	/* HeapTupleSatisfiesTimeRange(ct->ct_tup, NowTimeRange) */
	if (keytest(ct->ct_tup, relation, cache->cc_nkeys, cache->cc_skey))
	    break;
#ifdef USEQCMP
	if (qcmp(cache->cc_klen[0], v1, amgetattr(ct->ct_tup,
			cache->cc_keys[0], &cache->cc_rdesc->rd_att)))
	    continue;
	if (cache->cc_nkeys == 1)
	    break;
	if (qcmp(cache->cc_klen[1], v2, amgetattr(ct->ct_tup,
		 	cache->cc_keys[1], &cache->cc_rdesc->rd_att)))
	    continue;
	if (cache->cc_nkeys == 2)
	    break;
	if (qcmp(cache->cc_klen[2], v3, amgetattr(ct->ct_tup,
			cache->cc_keys[2], &cache->cc_rdesc->rd_att)))
	    continue;
	if (cache->cc_nkeys == 3)
	    break;
	if (!qcmp(cache->cc_klen[3], v4, amgetattr(ct->ct_tup,
			cache->cc_keys[3], &cache->cc_rdesc->rd_att)))
	    break;
#endif
    }
    if (ct) {		/* Tuple found in cache!   */
	SLRemove(&ct->ct_lrunode);		/* most recently used */
	SLAddHead(&cache->cc_lrulist, &ct->ct_lrunode);

#ifdef	CACHEDEBUG
	elog(DEBUG, "SearchSysCache(%s): found in bucket %d",
	    RelationGetRelationName(relation), hash
	);
#endif
	/*
	 * XXX race condition with cache invalidation ???
	 */
	RelationSetLockForRead(relation);
	RelationCloseHeapRelation(relation);
	    return (ct->ct_tup);
    }

    /*
     *	Tuple not found in cache, retrieve and add to cache
     */


    DisableCache = 1;
        
    if (!CacheCxt)
       CacheCxt = CreateGlobalMemory("Cache");
    
    oldcxt = MemoryContextSwitchTo(CacheCxt);
   
/*
    StartPortalAllocMode(StaticAllocMode);
*/

#ifdef	CACHEDEBUG
    elog(DEBUG, "SearchSysCache: performing scan (override==%d)",
	heapisoverride()
    );
#endif

    sd = ambeginscan(relation, 0, NowTimeQual,cache->cc_nkeys,cache->cc_skey);
    ntp = amgetnext(sd, 0, &buffer);
    if (HeapTupleIsValid(ntp)) {
#ifdef	CACHEDEBUG
	elog(DEBUG, "SearchSysCache: found tuple");
#endif
	ntp = palloctup(ntp, buffer, relation);
/*
	ntp = LintCast(HeapTuple, savemmgr((char *) ntp));
*/
	if (RuleLockIsValid(ntp->t_lock.l_lock)) {
/*
	    ntp->t_lock.l_lock = 
	        LintCast(RuleLock, savemmgr((char *) ntp->t_lock.l_lock));
*/
        }
    }
    amendscan(sd);
/*
    EndPortalAllocMode();
*/
    DisableCache = 0;

    if (HeapTupleIsValid(ntp)) {
	nct = LintCast(struct catctup *, palloc(sizeof(struct catctup)));
	nct->ct_tup = ntp;
	SLNewNode(&nct->ct_node);
	SLNewNode(&nct->ct_lrunode);
	SLAddHead(&cache->cc_lrulist, &nct->ct_lrunode);
	SLAddHead(&cache->cc_cache[hash], &nct->ct_node);
	if (++cache->cc_ntup > cache->cc_maxtup) {
	    register CatCTup *ct;

	    ct = (CatCTup *)SLGetTail(&cache->cc_lrulist);
	    if (ct != nct) {
#ifdef CACHEDEBUG
 		elog(DEBUG, "SearchSysCache(%s): Overflow, LRU removal",
		    RelationGetRelationName(relation)
		);
#endif
		CatCacheRemoveCTup(cache, ct);
	    }
	} 
#ifdef CACHEDEBUG
	elog(DEBUG, "SearchSysCache(%s): Contains %d/%d tuples",
	    RelationGetRelationName(relation),
	    cache->cc_ntup, cache->cc_maxtup
	);
#endif
#ifdef	CACHEDEBUG
	elog(DEBUG, "SearchSysCache(%s): put in bucket %d",
	    RelationGetRelationName(relation), hash
	);
#endif
    }

    RelationCloseHeapRelation(relation);
   
    MemoryContextSwitchTo(oldcxt);

    return(ntp);
}


#ifdef USEQCMP
qcmp(l, a, b)
int	l;
char	*a, *b;
{
    switch (l) {
    case 1:
    case 2:
    case 4:
 	return (a != b);
    }
    if (l < 0) {
	if (PSIZE(a) != PSIZE(b))
	    return(1);
	return(bcmp(a, b, PSIZE(a)));
    }
    return(bcmp(a, b, l));
}

#endif

/*
 * comphash --
 *	Compute a hash value, somehow.
 *
 * XXX explain algorithm here.
 *
 * l is length of the attribute value, v
 * v is the attribute value ("Datum")
 */
comphash(l, v)
int	l;
register char	*v;
{
    register int  i;

#ifdef CACHEDEBUG3 
    elog(DEBUG,"comphash (%d,%x)", l, v);
#endif
    switch (l) {
    case 1:
    case 2:
    case 4:
	return((int) v);
    }
    if (l == 16) {
		if (v < etext) {
			/*
			 * XXX int2, v, is first element of int16[8] key
			 * XXX this is a non-portable hack for built-in arrays
			 * XXX even I don't understand this well and I wrote
			 * XXX this -hirohama
			 */
			return((int16)v);
		} else {		/* XXX char16 special handling */
			l = NameComputeLength((Name)v);
		}
    } else if (l < 0) {
	l = PSIZE(v);
    }
    i = 0;
    while (l--) {
	i += *v++;
    }
    return(i);
}

/*
 *  RelationInvalidateCatalogCacheTuple()
 *
 *  Invalidate a tuple from a specific relation.  This call determines the
 *  cache in question and calls CatalogCacheIdInvalidate().  It is -ok-
 *  if the relation cannot be found, it simply means this backend has yet
 *  to open it.
 */

void
RelationInvalidateCatalogCacheTuple(relation, tuple, function)
Relation	relation;
HeapTuple	tuple;
void		(*function)();
{
    struct catcache *ccp;
    MemoryContext	oldcxt;
    ObjectId	relationId;

    Assert(RelationIsValid(relation));
    Assert(HeapTupleIsValid(tuple));

#ifdef	CACHEDEBUG
    elog(DEBUG, "RelationInvalidateCatalogCacheTuple: called");
#endif

    /* initialize the cache system */
            
    if (!CacheCxt)
       CacheCxt = CreateGlobalMemory("Cache");
    oldcxt = MemoryContextSwitchTo(CacheCxt);
   
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
	    &tuple->t_ctid
	);

	RelationCloseHeapRelation(relation);
    }

    MemoryContextSwitchTo(oldcxt);
    
/*	sendpm('I', "Invalidated tuple"); */
}

/*
 *  CatalogCacheIdInvalidate()
 *
 *  Invalidate a tuple given a cache id.  In this case the id should always
 *  be found (whether the cache has opened its relation or not).  Of course,
 *  if the cache has yet to open its relation, there will be no tuples so
 *  no problem.
 */

void
CatalogCacheIdInvalidate(cacheId, hashIndex, pointer)
int		cacheId;	/* XXX */
Index		hashIndex;
ItemPointer	pointer;
{
    struct catcache *ccp;
    struct catctup	*ct, *hct;
    MemoryContext	oldcxt;

    Assert(IndexIsValid(hashIndex) && IndexIsInBounds(hashIndex, NCCBUCK));
    Assert(ItemPointerIsValid(pointer));

#ifdef	CACHEDEBUG
    elog(DEBUG, "CatalogCacheIdInvalidate: called");
#endif	/* defined(CACHEDEBUG) */

    /* initialize the cache system */
        
    if (!CacheCxt)
       CacheCxt = CreateGlobalMemory("Cache");

    oldcxt = MemoryContextSwitchTo(CacheCxt);
   
    for (ccp = Caches; ccp; ccp = ccp->cc_next) {
	if (cacheId != ccp->id) 
    	    continue;
	for (ct = (CatCTup *)SLGetHead(&ccp->cc_cache[hashIndex]);
	     ct;
	     ct = (CatCTup *)SLGetSucc(&ct->ct_node)
	) {
	    /* HeapTupleSatisfiesTimeRange(ct->ct_tup, NowTQual) */
	    if (ItemPointerEquals(pointer, &ct->ct_tup->t_ctid)) 
		break;
 	}
	if (ct) {
	    /* found tuple to invalidate */
	    CatCacheRemoveCTup(ccp, ct);
#ifdef	CACHEDEBUG
	    elog(DEBUG, "CatalogCacheIdInvalidate: invalidated");
#endif
	}
	if (cacheId != InvalidCatalogCacheId) 
	    break;
    }

    MemoryContextSwitchTo(oldcxt);

    /*	sendpm('I', "Invalidated tuple"); */
}

Index
CatalogCacheComputeTupleHashIndex(cacheInOutP, relation, tuple)
struct catcache	*cacheInOutP;
Relation	relation;
HeapTuple	tuple;
{
    Boolean		isNull = '\0';
    extern DATUM	fastgetattr();

    if (cacheInOutP->relationId == InvalidObjectId)
	CatalogCacheInitializeCache(cacheInOutP, relation);

    switch (cacheInOutP->cc_nkeys) {
    case 4:
	cacheInOutP->cc_skey[3].sk_data = (cacheInOutP->cc_key[3] == T_OID) ?
	    (DATUM)tuple->t_oid : fastgetattr(tuple,
					cacheInOutP->cc_key[3],
					&relation->rd_att.data[0],
					&isNull
				  );
	Assert(!isNull);
	/* FALLTHROUGH */
    case 3:
	cacheInOutP->cc_skey[2].sk_data = (cacheInOutP->cc_key[2] == T_OID) ?
	    (DATUM)tuple->t_oid : fastgetattr(tuple,
					cacheInOutP->cc_key[2],
					&relation->rd_att.data[0],
					&isNull
				  );
	Assert(!isNull);
	/* FALLTHROUGH */
    case 2:
	cacheInOutP->cc_skey[1].sk_data = (cacheInOutP->cc_key[1] == T_OID) ?
	    (DATUM)tuple->t_oid : fastgetattr(tuple,
					cacheInOutP->cc_key[1],
					&relation->rd_att.data[0],
					&isNull
				    );
	Assert(!isNull);
	/* FALLTHROUGH */
    case 1:
	cacheInOutP->cc_skey[0].sk_data = (cacheInOutP->cc_key[0] == T_OID) ?
	    (DATUM)tuple->t_oid : fastgetattr(tuple,
					cacheInOutP->cc_key[0],
					&relation->rd_att.data[0],
					&isNull
				  );
	Assert(!isNull);
	break;
    default:
	elog(FATAL, "CCComputeTupleHashIndex: %d cc_nkeys",
	    cacheInOutP->cc_nkeys
	);
	break;
    }
    return (CatalogCacheComputeHashIndex(cacheInOutP));
}

/* static */
Index
CatalogCacheComputeHashIndex(cacheInP)
struct catcache	*cacheInP;
{
    Index	hashIndex;

    hashIndex = 0x0;

#ifdef CACHEDEBUG3
    elog(DEBUG,"CatalogCacheComputeHashIndex %s %d %d %d %x",cacheInP->cc_relname,cacheInP->cc_nkeys, cacheInP->cc_klen[0], cacheInP->cc_klen[1], cacheInP);
#endif
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

void
CatalogCacheSetId(cacheInOutP, id)	/* XXX temporary function */
CatCache *cacheInOutP;
int      id;
{
    Assert(id == InvalidCatalogCacheId || id >= 0);

    cacheInOutP->id = id;
}

/* static */
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

void
CatalogCacheInitializeCache(cache, relation)
struct catcache *cache;
Relation relation;
{
    MemoryContext	oldcxt;
    short didopen = 0;
    short i;

#ifdef CACHEDDEBUG2
    elog(DEBUG, "CatalogCacheInitializeCache: cache @%08lx", cache);
    if (relation)
        elog(DEBUG, "CatalogCacheInitializeCache: called w/relation(inval)");
    else
        elog(DEBUG, "CatalogCacheInitializeCache: called w/relname %s",
	    cache->cc_relname
	);
#endif
        
    if (!CacheCxt)
       CacheCxt = CreateGlobalMemory("Cache");

    oldcxt = MemoryContextSwitchTo(CacheCxt);

    /*
     *  If no relation was passed we must open it to get access to 
     *  its fields.  If one of the other caches has already opened
     *  it we can use the much faster ObjectIdOpenHeapRelation() call
     *  rather than RelationNameOpenHeapRelation() call.
     */

    if (!RelationIsValid(relation)) {
	struct catcache *cp;
        for (cp = Caches; cp; cp = cp->cc_next) {
	    if (strcmp(cp->cc_relname, cache->cc_relname) == 0) {
		if (cp->relationId != InvalidObjectId)
		    break;
	    }
	}
	if (cp) {
#ifdef CACHEDDEBUG2
	    elog(DEBUG, "CatalogCacheInitializeCache: fastrelopen");
#endif
    	    relation = ObjectIdOpenHeapRelation(cp->relationId);
	} else {
    	    relation = RelationNameOpenHeapRelation(cache->cc_relname);
	}
	didopen = 1;
    }
    Assert(RelationIsValid(relation));
    cache->relationId = RelationGetRelationId(relation);
#ifdef CACHEDDEBUG2
    elog(DEBUG, "CatalogCacheInitializeCache: relid %d, %d keys", 
        cache->relationId,
        cache->cc_nkeys
    );
#endif
    for (i = 0; i < cache->cc_nkeys; ++i) {
#ifdef CACHEDDEBUG2
        if (cache->cc_key[i] > 0) {
            elog(DEBUG, "CatalogCacheInitializeCache: load %d/%d w/%d, %d", 
	        i+1, cache->cc_nkeys, cache->cc_key[i],
	        relation->rd_att.data[cache->cc_key[i] - 1]->attlen
	    );
	} else {
            elog(DEBUG, "CatalogCacheInitializeCache: load %d/%d w/%d", 
	        i+1, cache->cc_nkeys, cache->cc_key[i]
	    );
	}
#endif
	if (cache->cc_key[i] > 0) {
	    cache->cc_klen[i] = 
		relation->rd_att.data[cache->cc_key[i]-1]->attlen;
	    cache->cc_skey[i].sk_opr =
	        EQPROC(relation->rd_att.data[cache->cc_key[i]-1]->atttypid);
#ifdef CACHEDEBUG3
	    elog(DEBUG,"CatalogCacheInit %16s %d %d %x",&relation->rd_rel->relname,i,relation->rd_att.data[cache->cc_key[i]-1]->attlen, cache);
#endif
	}
    }
    if (didopen)
        RelationCloseHeapRelation(relation);

    MemoryContextSwitchTo(oldcxt);
}

