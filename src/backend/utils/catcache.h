/*
 * catcache.h --
 *	Low-level catalog cache definitions.
 *
 * Identification:
 *	$Header$
 */

#ifndef	CatCacheIncluded	/* Include this file only once */
#define CatCacheIncluded	1

/* #define	CACHEDEBUG 	/* turns DEBUG elogs on */

#include "tmp/postgres.h"
    
#include "access/skey.h"
#include "access/htup.h"
#include "utils/rel.h"
#include "tmp/simplelists.h"

/*
 *	struct catctup:		tuples in the cache.
 *	struct catcache:	information for managing a cache.
 */

typedef struct catctup {
	HeapTuple	ct_tup;		/* A pointer to a tuple		*/
	SLNode		ct_node;	/* Doubly linked list node	*/
	SLNode		ct_lrunode;	/* ditto, for LRU algorithm	*/
} CatCTup;

typedef struct catcache {
	ObjectId	relationId;
	ObjectId	indexId;
	char		*cc_relname;	/* relation name for defered open */
	char		*cc_indname;	/* index name for defered open */
	HeapTuple	(*cc_iscanfunc)(); /* index scanfunction */
	TupleDescriptor cc_tupdesc; 	/* tuple descriptor from reldesc */
	int		id;		/* XXX could be improved -hirohama */
	short		cc_ntup;	/* # of tuples in this cache	*/
	short		cc_maxtup;	/* max # of tuples allowed (LRU)*/
	short		cc_nkeys;
	short		cc_size;
	short		cc_key[4];
	short		cc_klen[4];
	struct skey	cc_skey[4];
	struct catcache *cc_next;
	SLList    	cc_lrulist;	/* LRU list, most recent first  */
	SLList    	cc_cache[1];	/* Extended over NCCBUCK+1 elmnts*/
					/* used to be: struct catctup	*/
} CatCache;

#define	InvalidCatalogCacheId	(-1)

extern struct catcache	*Caches;

extern void CatalogCacheInitializeCache ARGS((struct catcache *cache, Relation relation));
extern void CatalogCacheSetId ARGS((CatCache *cacheInOutP, int id));
extern long comphash ARGS((long l, char *v));
extern Index CatalogCacheComputeHashIndex ARGS((struct catcache *cacheInP));
extern Index CatalogCacheComputeTupleHashIndex ARGS((struct catcache *cacheInOutP, Relation relation, HeapTuple tuple));
extern void CatCacheRemoveCTup ARGS((CatCache *cache, CatCTup *ct));
extern void CatalogCacheIdInvalidate ARGS((int cacheId, Index hashIndex, ItemPointer pointer));
extern void ResetSystemCache ARGS((void));
extern struct catcache *InitIndexedSysCache ARGS((char *relname, char *indname, int nkeys, int key[], HeapTuple (*iScanfuncP)()));
extern struct catcache *InitSysCache ARGS((char *relname, Name *indname, int nkeys, int key[], HeapTuple (*iScanfuncP)()));
extern HeapTuple SearchSysCache ARGS((struct catcache *cache, DATUM v1, DATUM v2, DATUM v3, DATUM v4));
extern void RelationInvalidateCatalogCacheTuple ARGS((Relation relation, HeapTuple tuple, void (*function)()));

#endif	/* !defined(CatCacheIncluded) */
