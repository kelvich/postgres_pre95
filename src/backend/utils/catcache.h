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

/*
 * InitSysCache --
 */
extern
struct catcache *
InitSysCache ARGS((char *relname , int nkeys , int key []));

/*
 * ResetSystemCache --
 *	Causes the entire cached system state to be discarded.
 */
extern
void
ResetSystemCache ARGS((
	void
));

/*
 * SearchSysCache --
 */
extern
HeapTuple
SearchSysCache ARGS((
	struct catcache	*cache,
	DATUM		v1,
	DATUM		v2,
	DATUM		v3,
	DATUM		v4
));

/*
 * RelationIdInvalidateCatalogCacheTuple --
 */
extern
void
RelationIdInvalidateCatalogCacheTuple ARGS((
	ObjectId	relationId,
	HeapTuple	tuple,
	void		(*function)()
));

/*
 * CatalogCacheIdInvalidate --
 */
extern
void
CatalogCacheIdInvalidate ARGS((
	int cacheId,
	Index hashIndex,
	ItemPointer pointer
));

/*
 * CatalogCacheComputeTupleHashIndex --
 */
Index CatalogCacheComputeTupleHashIndex ARGS((
	struct catcache *cacheInOutP,
	Relation relation,
	HeapTuple tuple
));

/*
 * CatalogCacheSetId --
 *	XXX This is a temporary function.
 */
extern
void CatalogCacheSetId ARGS((CatCache *cacheInOutP , int id ));

void CatalogCacheInitializeCache ARGS((
	struct catcache *cache,
	Relation relation
));

int comphash ARGS((int l , char *v ));

Index CatalogCacheComputeHashIndex ARGS((struct catcache *cacheInP ));

void CatCacheRemoveCTup ARGS((CatCache *cache , CatCTup *ct ));

struct catcache *InitIndexedSysCache ARGS((
	char *relname, 
	char *indname, 
	int nkeys, 
	int key []
));

#endif	/* !defined(CatCacheIncluded) */
