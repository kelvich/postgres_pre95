
; /*
; * 
; * POSTGRES Data Base Management System
; * 
; * Copyright (c) 1988 Regents of the University of California
; * 
; * Permission to use, copy, modify, and distribute this software and its
; * documentation for educational, research, and non-profit purposes and
; * without fee is hereby granted, provided that the above copyright
; * notice appear in all copies and that both that copyright notice and
; * this permission notice appear in supporting documentation, and that
; * the name of the University of California not be used in advertising
; * or publicity pertaining to distribution of the software without
; * specific, written prior permission.  Permission to incorporate this
; * software into commercial products can be obtained from the Campus
; * Software Office, 295 Evans Hall, University of California, Berkeley,
; * Ca., 94720 provided only that the the requestor give the University
; * of California a free licence to any derived software for educational
; * and research purposes.  The University of California makes no
; * representations about the suitability of this software for any
; * purpose.  It is provided "as is" without express or implied warranty.
; * 
; */



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

/*
 *	struct catctup:		tuples in the cache.
 *	struct catcache:	information for managing a cache.
 */

struct catctup {
	HeapTuple	ct_tup;
	struct catctup	*ct_next;
};

struct catcache {
	ObjectId	relationId;
	int		id;		/* XXX could be improved -hirohama */
	short		cc_nkeys;
	short		cc_size;
	short		cc_key[4];
	short		cc_klen[4];
	struct skey	cc_skey[4];
	struct catcache *cc_next;
	struct catctup	cc_cache[1];
};

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
