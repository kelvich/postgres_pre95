
/*
 * 
 * POSTGRES Data Base Management System
 * 
 * Copyright (c) 1988 Regents of the University of California
 * 
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for educational, research, and non-profit purposes and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of the University of California not be used in advertising
 * or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Permission to incorporate this
 * software into commercial products can be obtained from the Campus
 * Software Office, 295 Evans Hall, University of California, Berkeley,
 * Ca., 94720 provided only that the the requestor give the University
 * of California a free licence to any derived software for educational
 * and research purposes.  The University of California makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 * 
 */



/*
 * catcache.c --
 *	System catalog cache for tuples matching a key.
 *
 * Notes:
 *	XXX This needs to use exception.h to handle recovery when
 *		an abort occurs during DisableCache.
 */

#include "access.h"
#include "catalog.h"
#include "fmgr.h"
#include "lmgr.h"
#include "log.h"
#include "heapam.h"
#include "htup.h"
#include "master.h"	/* XXX ? */
#include "name.h"
#include "tqual.h"
#include "oid.h"
#include "rel.h"

#include "catcache.h"

RcsId("$Header$");

#define	NCCBUCK	50	/* CatCache buckets */
#define CCSIZE	(sizeof(struct catcache) + NCCBUCK * sizeof(struct catctup))

struct catcache	*Caches = NULL;
struct context	*CacheCxt = NULL;

static int	DisableCache;

/* XXX this should be replaced by catalog lookups soon */
static long	eqproc[] = {
	F_BOOLEQ, 0l, F_CHAREQ, F_CHAR16EQ, 0l,
	F_INT2EQ, 0l, F_INT4EQ, 0l, F_TEXTEQ,
	F_OIDEQ, 0l, 0l, 0l, 0l
	};
#define	EQPROC(SYSTEMTYPEOID)	eqproc[(SYSTEMTYPEOID)-16]

/*
 * CatalogCacheComputeHashIndex --
 */
static
Index
CatalogCacheComputeHashIndex ARGS((
	struct catcache	*cacheInP;
));

struct catcache *
InitSysCache(relname, nkeys, key)
	char	*relname;
	int	nkeys;
	int	key[];
{
	struct catcache	*cp;
	register int	i;
	struct context	*oldcxt;
	Relation	relation;

	/* initialize the cache system */
	if (!CacheCxt) {
		oldcxt = newcontext();
		CacheCxt = Curcxt;
		CacheCxt->ct_status |= CT_PRESERVE;
	} else {
		oldcxt = switchcontext(CacheCxt);
	}
	cp = LintCast(struct catcache *, palloc(CCSIZE));
	bzero((char *) cp, CCSIZE);
	cp->cc_next = Caches;
	Caches = cp;

	relation = RelationNameOpenHeapRelation(relname);
	cp->relationId = RelationGetRelationId(relation);

	cp->id = InvalidCatalogCacheId;	/* XXX this should be an argument */
	cp->cc_nkeys = nkeys;
	for (i = 0; i < nkeys; ++i) {
		cp->cc_key[i] = key[i];
		if (!key[i]) {
			elog(FATAL, "InitSysCache: called with 0 key[%d]", i);
		}
		if (key[i] < 0) {
			if (key[i] != T_OID) {
				elog(FATAL,
				     "InitSysCache: called with %d key[%d]",
				     key[i], i);
			} else {
				cp->cc_klen[i] = sizeof(OID);
				cp->cc_skey[i].sk_attnum = key[i];
				cp->cc_skey[i].sk_opr = F_OIDEQ;
				continue;
			}
		}
		cp->cc_klen[i] = relation->rd_att.data[key[i] - 1]->attlen;
		cp->cc_skey[i].sk_attnum = key[i];
		/* XXX Replace me with a catalog lookup! */
		cp->cc_skey[i].sk_opr =
			EQPROC(relation->rd_att.data[key[i]-1]->atttypid);
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
                     cp->cc_skey[i].sk_data);
	}
#endif
	RelationCloseHeapRelation(relation);

	switchcontext(oldcxt);
	return(cp);
}

void
ResetSystemCache()
{
	register unsigned	hash;
	register struct catctup	*ct;
	struct context		*oldcxt;
	struct catctup		*hct;
	struct catcache		*cache;

#ifdef	CACHEDEBUG
	elog(DEBUG, "ResetSystemCache called");
#endif	/* defined(CACHEDEBUG) */

	if (DisableCache) {
		elog(WARN, "ResetSystemCache: Called while cache disabled");
		return;
	}

	/* initialize the cache system */
	if (!CacheCxt) {
		oldcxt = newcontext();
		CacheCxt = Curcxt;
		CacheCxt->ct_status |= CT_PRESERVE;
	} else {
		oldcxt = switchcontext(CacheCxt);
	}

	for (cache = Caches; PointerIsValid(cache); cache = cache->cc_next) {
		for (hash = 0; hash < NCCBUCK; hash += 1) {
			ct = &cache->cc_cache[hash];

			while (PointerIsValid(ct) &&
					PointerIsValid(ct->ct_tup)) {

				pfree((char *) ct->ct_tup);

				/* free *ct */
				hct = ct->ct_next;
				*ct = *hct;
				pfree((char *)hct);
			}
		}
	}
	switchcontext(oldcxt);
}

HeapTuple
SearchSysCache(cache, v1, v2, v3, v4)
	register struct catcache	*cache;
	DATUM				v1, v2, v3, v4;
{
	register unsigned	hash;
	register struct catctup	*ct;
	struct context		*oldcxt;
	HeapTuple		ntp;
	Buffer			buffer;
	HeapTuple		palloctup();
	struct catctup		*nct;
	HeapScan		sd;
	Relation		relation;

	if (DisableCache) {
		elog(WARN, "SearchSysCache: Called while cache disabled");
		return((HeapTuple) NULL);
	}

	relation = ObjectIdOpenHeapRelation(cache->relationId);

#ifdef	CACHEDEBUG
	elog(DEBUG, "SearchSysCache(%s)", RelationGetRelationName(relation));
#endif	/* defined(CACHEDEBUG) */

	cache->cc_skey[0].sk_data = v1;
	cache->cc_skey[1].sk_data = v2;
	cache->cc_skey[2].sk_data = v3;
	cache->cc_skey[3].sk_data = v4;

	hash = CatalogCacheComputeHashIndex(cache);

/* XXX keytest arguments */
	for (ct = &(cache->cc_cache[hash]);
			PointerIsValid(ct) && PointerIsValid(ct->ct_tup);
			ct = ct->ct_next) {
		/* HeapTupleSatisfiesTimeRange(ct->ct_tup, NowTimeRange) */
		if (keytest(ct->ct_tup, relation, cache->cc_nkeys,
			    cache->cc_skey))
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

	if (PointerIsValid(ct) && PointerIsValid(ct->ct_tup)) {
		/* tuple found in cache! */
#ifdef	CACHEDEBUG
		elog(DEBUG, "SearchSysCache(%s): found in bucket %d",
			RelationGetRelationName(relation), hash);
#endif	/* defined(CACHEDEBUG) */

		/*
		 * XXX race condition with cache invalidation ???
		 */
		RelationSetLockForRead(relation);

		RelationCloseHeapRelation(relation);

		return (ct->ct_tup);
	}

	DisableCache = 1;

	oldcxt = switchcontext(CacheCxt);
	startmmgr(M_STATIC);

#ifdef	CACHEDEBUG
	elog(DEBUG, "SearchSysCache: performing scan (override==%d)",
		heapisoverride());
#endif	/* defined(CACHEDEBUG) */

	sd = ambeginscan(relation, 0, NowTimeQual, cache->cc_nkeys,
		cache->cc_skey);
	ntp = amgetnext(sd, 0, &buffer);
	if (HeapTupleIsValid(ntp)) {
#ifdef	CACHEDEBUG
		elog(DEBUG, "SearchSysCache: found tuple");
#endif	/* defined(CACHEDEBUG) */

		ntp = palloctup(ntp, buffer, relation);

		ntp = LintCast(HeapTuple, savemmgr((char *) ntp));

		if (RuleLockIsValid(ntp->t_lock.l_lock)) {
			ntp->t_lock.l_lock =
				LintCast(RuleLock,
					 savemmgr((char *)
						  ntp->t_lock.l_lock));
		}
	}
	amendscan(sd);
	(void)endmmgr((char *)NULL);

	DisableCache = 0;

	if (HeapTupleIsValid(ntp)) {
		nct = LintCast(struct catctup *,
			palloc(sizeof(struct catctup)));
		*nct = cache->cc_cache[hash];
		cache->cc_cache[hash].ct_next = nct;
		cache->cc_cache[hash].ct_tup = ntp;
#ifdef	CACHEDEBUG
		elog(DEBUG, "SearchSysCache(%s): put in bucket %d",
			RelationGetRelationName(relation), hash);
#endif	/* defined(CACHEDEBUG) */
	}

	RelationCloseHeapRelation(relation);

	switchcontext(oldcxt);
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

comphash(l, v)
	int		l;
	register char	*v;
{
	register int	i;

	switch (l) {
	case 1:
	case 2:
	case 4:
		return((int) v);
	}
	if (l == 16) {		/* XXX char16 special handling */
		l = NameComputeLength((Name)v);
	} else if (l < 0) {
		l = PSIZE(v);
	}
	i = 0;
	while (l--) {
		i += *v++;
	}
	return(i);
}

void
RelationInvalidateCatalogCacheTuple(relation, tuple, function)
	Relation	relation;
	HeapTuple	tuple;
	void		(*function)();
{
	struct catcache *ccp;
	struct context	*oldcxt;
	ObjectId	relationId;

	Assert(RelationIsValid(relation));
	Assert(HeapTupleIsValid(tuple));

#ifdef	CACHEDEBUG
	elog(DEBUG, "RelationInvalidateCatalogCacheTuple: called");
#endif	/* defined(CACHEDEBUG) */

	/* initialize the cache system */
	if (!CacheCxt) {
		oldcxt = newcontext();
		CacheCxt = Curcxt;
		CacheCxt->ct_status |= CT_PRESERVE;
	} else {
		oldcxt = switchcontext(CacheCxt);
	}

	relationId = RelationGetRelationId(relation);

	for (ccp = Caches; ccp; ccp = ccp->cc_next) {

		if (relationId != ccp->relationId) {
			continue;
		}

		/* OPT inline simplification of CatalogCacheIdInvalidate */
		if (!PointerIsValid(function)) {
			function = CatalogCacheIdInvalidate;
		}
		(*function)(ccp->id,
			CatalogCacheComputeTupleHashIndex(ccp, relation, tuple),
			&tuple->t_ctid);

		RelationCloseHeapRelation(relation);
	}
	switchcontext(oldcxt);
/*	sendpm('I', "Invalidated tuple"); */
}

void
CatalogCacheIdInvalidate(cacheId, hashIndex, pointer)
	int		cacheId;	/* XXX */
	Index		hashIndex;
	ItemPointer	pointer;
{
	struct catcache *ccp;
	struct catctup	*ct, *hct;
	struct context	*oldcxt;

	Assert(IndexIsValid(hashIndex) && IndexIsInBounds(hashIndex, NCCBUCK));
	Assert(ItemPointerIsValid(pointer));

#ifdef	CACHEDEBUG
	elog(DEBUG, "CatalogCacheIdInvalidate: called");
#endif	/* defined(CACHEDEBUG) */

	/* initialize the cache system */
	if (!CacheCxt) {
		oldcxt = newcontext();
		CacheCxt = Curcxt;
		CacheCxt->ct_status |= CT_PRESERVE;
	} else {
		oldcxt = switchcontext(CacheCxt);
	}

	for (ccp = Caches; ccp; ccp = ccp->cc_next) {
		if (cacheId != ccp->id) {
			continue;
		}
		for (ct = &ccp->cc_cache[hashIndex]; PointerIsValid(ct) &&
				PointerIsValid(ct->ct_tup); ct = ct->ct_next) {
			/* HeapTupleSatisfiesTimeRange(ct->ct_tup, NowTQual) */
			if (ItemPointerEquals(pointer, &ct->ct_tup->t_ctid)) {
				break;
			}
 		}
		if (PointerIsValid(ct) && PointerIsValid(ct->ct_tup)) {

			/* found tuple to invalidate */
			if (RuleLockIsValid(ct->ct_tup->t_lock.l_lock)) {
				pfree((char *)ct->ct_tup->t_lock.l_lock);
			}
			pfree((char *) ct->ct_tup);

			/* free *ct */
			if (ct->ct_next) {
				hct = ct->ct_next;
				*ct = *hct;
				pfree((char *) hct);
			}
#ifdef	CACHEDEBUG
			elog(DEBUG,
				"CatalogCacheIdInvalidate: invalidated");
#endif	/* defined(CACHEDEBUG) */
		}
		if (cacheId != InvalidCatalogCacheId) {
			break;
		}
	}
	switchcontext(oldcxt);
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

	switch (cacheInOutP->cc_nkeys) {
	case 4:
		cacheInOutP->cc_skey[3].sk_data =
			(cacheInOutP->cc_key[3] == T_OID) ?
				(DATUM)tuple->t_oid : fastgetattr(tuple,
					cacheInOutP->cc_key[3],
					relation->rd_att.data[0],
					&isNull);
		Assert(!isNull);
		/* FALLTHROUGH */
	case 3:
		cacheInOutP->cc_skey[2].sk_data =
			(cacheInOutP->cc_key[2] == T_OID) ?
				(DATUM)tuple->t_oid : fastgetattr(tuple,
					cacheInOutP->cc_key[2],
					&relation->rd_att.data[0],
					&isNull);
		Assert(!isNull);
		/* FALLTHROUGH */
	case 2:
		cacheInOutP->cc_skey[1].sk_data =
			(cacheInOutP->cc_key[1] == T_OID) ?
				(DATUM)tuple->t_oid : fastgetattr(tuple,
					cacheInOutP->cc_key[1],
					&relation->rd_att.data[0],
					&isNull);
		Assert(!isNull);
		/* FALLTHROUGH */
	case 1:
		cacheInOutP->cc_skey[0].sk_data =
			(cacheInOutP->cc_key[0] == T_OID) ?
				(DATUM)tuple->t_oid : fastgetattr(tuple,
					cacheInOutP->cc_key[0],
					&relation->rd_att.data[0],
					&isNull);
		Assert(!isNull);
		break;
	default:
		elog(FATAL, "CCComputeTupleHashIndex: %d cc_nkeys",
			cacheInOutP->cc_nkeys);
		break;
	}

	return (CatalogCacheComputeHashIndex(cacheInOutP));
}

static
Index
CatalogCacheComputeHashIndex(cacheInP)
	struct catcache	*cacheInP;
{
	Index	hashIndex;

	hashIndex = 0x0;

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
		elog(FATAL, "CCComputeHashIndex: %d cc_nkeys",
			cacheInP->cc_nkeys);
		break;
	}

	hashIndex %= cacheInP->cc_size;

	return (hashIndex);
}

void
CatalogCacheSetId(cacheInOutP, id)	/* XXX temporary function */
	struct catcache	*cacheInOutP;
	int		id;
{
	Assert(id == InvalidCatalogCacheId || id >= 0);

	cacheInOutP->id = id;
}
