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

#include "access.h"	/* XXX struct skey */
#include "htup.h"
#include "oid.h"
#include "rel.h"
#include "simplelists.h"

/*
 *	struct catctup:		tuples in the cache.
 *	struct catcache:	information for managing a cache.
 */

typedef struct catctup {
	HeapTuple	ct_tup;		/* A pointer to a tuple		*/
	SimpleNode	ct_node;	/* Doubly linked list node	*/
	SimpleNode	ct_lrunode;	/* ditto, for LRU algorithm	*/
} CatCTup;

typedef struct catcache {
	ObjectId	relationId;
	char		*cc_relname;	/* relation name for defered open  */
	int		id;		/* XXX could be improved -hirohama */
	short		cc_ntup;	/* # of tuples in this cache	*/
	short		cc_maxtup;	/* max # of tuples allowed (LRU)*/
	short		cc_nkeys;
	short		cc_size;
	short		cc_key[4];
	short		cc_klen[4];
	struct skey	cc_skey[4];
	struct catcache *cc_next;
	SimpleList	cc_lrulist;	/* LRU list, most recent first  */
	SimpleList	cc_cache[1];	/* Extended over NCCBUCK+1 elmnts*/
					/* used to be: struct catctup	*/
} CatCache;

#define	InvalidCatalogCacheId	(-1)

extern struct catcache	*Caches;

/*
 * InitSysCache --
 */
extern
struct catcache *
InitSysCache ARGS((
	Name	relationName,
	int	numberOfAttributes,
	int	key[]
));

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
	DATUM		v2
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
	void		(*function)(
		int		cacheId,	/* XXX */
		Index		hashIndex,
		ItemPointer	pointer
	)
));

/*
 * CatalogCacheIdInvalidate --
 */
extern
void
CatalogCacheIdInvalidate ARGS((
	int		cacheId,	/* XXX */
	Index		hashIndex,
	ItemPointer	pointer
));

/*
 * CatalogCacheComputeTupleHashIndex --
 */
Index
CatalogCacheComputeTupleHashIndex ARGS((
	struct catcache	*cacheInOutP,
	HeapTuple	tuple
));

/*
 * CatalogCacheSetId --
 *	XXX This is a temporary function.
 */
extern
void
CatalogCacheSetId ARGS((
	struct catcache	*cache,
	int		id
));

#endif	/* !defined(CatCacheIncluded) */
