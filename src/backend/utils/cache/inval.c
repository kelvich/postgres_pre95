/*
 * inval.c --
 *	POSTGRES cache invalidation dispatcher code.
 */

#include "c.h"

#include "buf.h"	/* XXX for InvalidBuffer */
#include "cat.h"	/* XXX to support hacks below */
#include "catcache.h"
#include "catname.h"	/* XXX to support hacks below */
#include "heapam.h"	/* XXX to support hacks below */
#include "htup.h"
#include "ipci.h"
#include "linval.h"
#include "log.h"
#include "oid.h"
#include "rel.h"
#include "sinval.h"
#include "syscache.h"	/* XXX to support the hacks below */

#include "inval.h"

RcsId("$Header$");

typedef struct CatalogInvalidationData {
	Index		cacheId;
	Index		hashIndex;
	ItemPointerData	pointerData;
} CatalogInvalidationData;

typedef struct RelationInvalidationData {
	ObjectId	relationId;
	ObjectId	objectId;
} RelationInvalidationData;

typedef union AnyInvalidation {
	CatalogInvalidationData		catalog;
	RelationInvalidationData	relation;
} AnyInvalidation;

typedef struct InvalidationMessageData {
	char		kind;
	AnyInvalidation	any;
} InvalidationMessageData;

typedef InvalidationMessageData	*InvalidationMessage;

static LocalInvalid	Invalid = EmptyLocalInvalid;	/* XXX global */

static bool	RefreshWhenInvalidate = false;

/* XXX hack */
ObjectId	MyRelationRelationId = InvalidObjectId;
ObjectId	MyAttributeRelationId = InvalidObjectId;
/* ObjectId	MyIndexRelationId = InvalidObjectId;	/* unneeded */
ObjectId	MyAMRelationId = InvalidObjectId;
ObjectId	MyAMOPRelationId = InvalidObjectId;

/*
 * getmyrelids -- XXX hack
 */
extern
void
getmyrelids ARGS((
	void
));

#define ValidateHacks() \
	if (!ObjectIdIsValid(MyRelationRelationId)) getmyrelids()

/*
 * CacheIdRegisterLocalInvalid --
 */
static
void
CacheIdRegisterLocalInvalid ARGS((
	Index		cacheId,
	Index		hashIndex,
	ItemPointer	pointer
));

/*
 * CacheIdInvalidate --
 */
static
void
CacheIdInvalidate ARGS((
	Index		cacheId,
	Index		hashIndex,
	ItemPointer	pointer
));

/*
 * ResetSystemCaches --
 */
static
void
ResetSystemCaches ARGS((
	void
));

/*
 * InvalidationMessageRegisterSharedInvalid --
 */
static
void
InvalidationMessageRegisterSharedInvalid ARGS((
	InvalidationMessage	message
));

/*
 * InvalidationMessageRegisterCacheInvalidate --
 */
static
void
InvalidationMessageCacheInvalidate ARGS((
	InvalidationMessage	message
));

/*
 * RelationInvalidateRelationCache --
 */
static
void
RelationInvalidateRelationCache ARGS((
	Relation	relation,
	HeapTuple	tuple,
	void		(*function)()
));

/*
 * RelationIdRegisterLocalInvalid --
 */
static
void
RelationIdRegisterLocalInvalid ARGS((
	ObjectId	relationId,
	ObjectId	objectId
));

void
DiscardInvalid()
{
#ifdef	INVALIDDEBUG
	elog(DEBUG, "DiscardInvalid called");
#endif	/* defined(INVALIDDEBUG) */

	InvalidateSharedInvalid(CacheIdInvalidate, ResetSystemCaches);
}

void
RegisterInvalid(send)
	bool	send;
{
#ifdef	INVALIDDEBUG
	elog(DEBUG, "RegisterInvalid(%d) called", send);
#endif	/* defined(INVALIDDEBUG) */

	if (send) {
		LocalInvalidInvalidate(Invalid,
			InvalidationMessageRegisterSharedInvalid);
	} else {
		LocalInvalidInvalidate(Invalid,
			InvalidationMessageCacheInvalidate);
	}
	Invalid = EmptyLocalInvalid;
}

void
SetRefreshWhenInvalidate(on)
	bool	on;
{
#ifdef	INVALIDDEBUG
	elog(DEBUG, "RefreshWhenInvalidate(%d) called", on);
#endif	/* defined(INVALIDDEBUG) */

	RefreshWhenInvalidate = on;
}

void
RelationInvalidateHeapTuple(relation, tuple)
	Relation	relation;
	HeapTuple	tuple;
{
	Assert(RelationIsValid(relation));
	Assert(HeapTupleIsValid(tuple));

	if (issystem(&RelationGetRelationTupleForm(relation)->relname)) {
#ifdef	INVALIDDEBUG
		elog(DEBUG, "RelationInvalidateHeapTuple(%.16s, [%d,%d])",
			RelationGetRelationName(relation),
			ItemPointerGetBlockNumber(&tuple->t_ctid),
			ItemPointerSimpleGetOffsetNumber(&tuple->t_ctid));
#endif	/* defined(INVALIDDEBUG) */

		RelationInvalidateCatalogCacheTuple(relation, tuple,
			CacheIdRegisterLocalInvalid);

		RelationInvalidateRelationCache(relation, tuple,
			RelationIdRegisterLocalInvalid);
		
		if (RefreshWhenInvalidate) {

			RelationInvalidateCatalogCacheTuple(relation, tuple,
				(void (*)())NULL);
/*
			RelationInvalidateRelationCache(relation, tuple,
				(void (*)())NULL);
*/
		}
	}
}

static
void
CacheIdRegisterLocalInvalid(cacheId, hashIndex, pointer)
	Index		cacheId;
	Index		hashIndex;
	ItemPointer	pointer;
{
	InvalidationMessage	message;

#ifdef	INVALIDDEBUG
	elog(DEBUG, "CacheIdRegisterLocalInvalid(%d, %d, [%d, %d])",
		cacheId, hashIndex, ItemPointerGetBlockNumber(pointer),
		ItemPointerSimpleGetOffsetNumber(pointer));
#endif	/* defined(INVALIDDEBUG) */

	message = (InvalidationMessage)
		InvalidationEntryAllocate(sizeof (InvalidationMessageData));

	message->kind = 'c';
	message->any.catalog.cacheId = cacheId;
	message->any.catalog.hashIndex = hashIndex;
	ItemPointerCopy(pointer, &message->any.catalog.pointerData);

	Invalid = LocalInvalidRegister(Invalid, message);
}

static
void
CacheIdInvalidate(cacheId, hashIndex, pointer)
	Index		cacheId;
	Index		hashIndex;
	ItemPointer	pointer;
{
	InvalidationMessage	message;

#ifdef	INVALIDDEBUG
	elog(DEBUG, "CacheIdInvalidate(%d, %d, 0x%x[%d])", cacheId, hashIndex,
		pointer, ItemPointerIsValid(pointer));
#endif	/* defined(INVALIDDEBUG) */

	if (ItemPointerIsValid(pointer)) {
		CatalogCacheIdInvalidate(cacheId, hashIndex, pointer);
	} else {
		ValidateHacks();	/* XXX */

		if (cacheId == MyRelationRelationId) {
			RelationIdInvalidateRelationCacheByRelationId(
				hashIndex);
		} else if (cacheId == MyAttributeRelationId) {
			RelationIdInvalidateRelationCacheByRelationId(
				hashIndex);
		} else if (cacheId == MyAMRelationId) {
			RelationIdInvalidateRelationCacheByAccessMethodId(
				hashIndex);
		} else if (cacheId == MyAMOPRelationId) {
			RelationIdInvalidateRelationCacheByAccessMethodId(
				InvalidObjectId);
		} else {
			elog(FATAL, "CacheIdInvalidate: %d relation id?",
				cacheId);
		}
	}
}

static
void
ResetSystemCaches()
{
	ResetSystemCache();

	RelationCacheInvalidate(false);
}

static
void
InvalidationMessageRegisterSharedInvalid(message)
	InvalidationMessage	message;
{
	Assert(PointerIsValid(message));

	switch (message->kind) {
	case 'c':
#ifdef	INVALIDDEBUG
		elog(DEBUG, "InvalidationMessageRegisterSharedInvalid(c, %d, %d, [%d, %d])",
			message->any.catalog.cacheId,
			message->any.catalog.hashIndex,
			ItemPointerGetBlockNumber(
				&message->any.catalog.pointerData),
			ItemPointerSimpleGetOffsetNumber(
				&message->any.catalog.pointerData));
#endif	/* defined(INVALIDDEBUG) */

		RegisterSharedInvalid(message->any.catalog.cacheId,
			message->any.catalog.hashIndex,
			&message->any.catalog.pointerData);
		break;
	case 'r':
#ifdef	INVALIDDEBUG
		elog(DEBUG,
			"InvalidationMessageRegisterSharedInvalid(r, %d, %d)",
			message->any.relation.relationId,
			message->any.relation.objectId);
#endif	/* defined(INVALIDDEBUG) */

		RegisterSharedInvalid(message->any.relation.relationId,
			message->any.relation.objectId, (ItemPointer)NULL);
		break;
	default:
		elog(FATAL,
			"InvalidationMessageRegisterSharedInvalid: `%c' kind",
			message->kind);
	}
}

static
void
InvalidationMessageCacheInvalidate(message)
	InvalidationMessage	message;
{
	Assert(PointerIsValid(message));

	switch (message->kind) {
	case 'c':
#ifdef	INVALIDDEBUG
		elog(DEBUG, "InvalidationMessageCacheInvalidate(c, %d, %d, [%d, %d])",
			message->any.catalog.cacheId,
			message->any.catalog.hashIndex,
			ItemPointerGetBlockNumber(
				&message->any.catalog.pointerData),
			ItemPointerSimpleGetOffsetNumber(
				&message->any.catalog.pointerData));
#endif	/* defined(INVALIDDEBUG) */

		CatalogCacheIdInvalidate(message->any.catalog.cacheId,
			message->any.catalog.hashIndex,
			&message->any.catalog.pointerData);
		break;
	case 'r':
#ifdef	INVALIDDEBUG
		elog(DEBUG, "InvalidationMessageCacheInvalidate(r, %d, %d)",
			message->any.relation.relationId,
			message->any.relation.objectId);
#endif	/* defined(INVALIDDEBUG) */

		/* XXX ignore this--is this correct ??? */
		break;
	default:
		elog(FATAL, "InvalidationMessageCacheInvalidate: `%c' kind",
			message->kind);
	}
}

void
getmyrelids()
{
	HeapTuple	tuple;

	tuple = SearchSysCacheTuple(RELNAME, RelationRelationName);
	Assert(HeapTupleIsValid(tuple));
	MyRelationRelationId = tuple->t_oid;

	tuple = SearchSysCacheTuple(RELNAME, AttributeRelationName);
	Assert(HeapTupleIsValid(tuple));
	MyAttributeRelationId = tuple->t_oid;

	tuple = SearchSysCacheTuple(RELNAME, AccessMethodRelationName);
	Assert(HeapTupleIsValid(tuple));
	MyAMRelationId = tuple->t_oid;

	tuple = SearchSysCacheTuple(RELNAME, AccessMethodOperatorRelationName);
	Assert(HeapTupleIsValid(tuple));
	MyAMOPRelationId = tuple->t_oid;
}

static
void
RelationInvalidateRelationCache(relation, tuple, function)
	Relation	relation;
	HeapTuple	tuple;
	void		(*function)();
{
	ObjectId	relationId;
	ObjectId	objectId;

	ValidateHacks();	/* XXX */

	relationId = RelationGetRelationId(relation);

	if (relationId == MyRelationRelationId) {
		objectId = tuple->t_oid;
	} else if (relationId == MyAttributeRelationId) {
		objectId = ((AttributeTupleForm)GETSTRUCT(tuple))->attrelid;
	} else if (relationId == MyAMRelationId) {
		objectId = tuple->t_oid;
	} else if (relationId == MyAMOPRelationId) {
		;	/* objectId is unused */
	} else {
		return;
	}

	/* can't handle immediate relation descriptor invalidation */
	Assert(PointerIsValid(function));

	(*function)(relationId, objectId);
}

static
void
RelationIdRegisterLocalInvalid(relationId, objectId)
	ObjectId	relationId;
	ObjectId	objectId;
{
	InvalidationMessage	message;

#ifdef	INVALIDDEBUG
	elog(DEBUG, "RelationRegisterLocalInvalid(%d, %d)", relationId,
		objectId);
#endif	/* defined(INVALIDDEBUG) */

	message = (InvalidationMessage)
		InvalidationEntryAllocate(sizeof (InvalidationMessageData));

	message->kind = 'r';
	message->any.relation.relationId = relationId;
	message->any.relation.objectId = objectId;

	Invalid = LocalInvalidRegister(Invalid, message);
}
