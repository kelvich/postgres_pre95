/*
 * lmgr.h --
 *	POSTGRES lock manager definitions.
 *
 * Identification:
 *	$Header$
 */

#ifndef	LMgrIncluded	/* Include this file only once */
#define LMgrIncluded	1

#include "tmp/postgres.h"

#include "storage/itemptr.h"
#include "storage/part.h"
#include "storage/lock.h"
#include "utils/rel.h"

/* 
 * This was moved from pladt.h for the new lock manager.  Want to obsolete
 * all of the old code.
 */
typedef struct LRelId {
    ObjectId	 relId;     /* a relation identifier */
    ObjectId     dbId;      /* a database identifier */
} LRelId;

typedef struct LockInfoData  {
        bool                    initialized;
        LRelId                  lRelId;
        TransactionIdData       transactionIdData;
        uint16                  flags;
} LockInfoData;
typedef LockInfoData    *LockInfo;

#define LockInfoIsValid(linfo) \
	((PointerIsValid(linfo)) &&  ((LockInfo) linfo)->initialized)
/*
 * RelationGetLRelId --
 *	Returns "lock" relation identifier for a relation.
 */
extern
LRelId
RelationGetLRelId ARGS((
	Relation	relation
));

/*
 * LRelIdGetDatabaseId --
 *	Returns database identifier for a "lock" relation identifier.
 */
extern
ObjectId
LRelIdGetDatabaseId ARGS((
	Relation	relation
));

/*
 * LRelIdGetRelationId --
 *	Returns relation identifier for a "lock" relation identifier.
 */
extern
ObjectId
LRelIdGetRelationId ARGS((
	Relation	relation
));

/*
 * DatabaseIdIsMyDatabaseId --
 *	True iff database object identifier is valid in my present database.
 */
extern
bool
DatabaseIdIsMyDatabaseId ARGS((
	ObjectId	databaseId
));

/*
 * LRelIdContainsMyDatabaseId --
 *	True iff "lock" relation identifier is valid in my present database.
 */
extern
bool
LRelIdContainsMyDatabaseId ARGS((
	LRelId	lRelId
));

/*
 * RelationInitLockInfo --
 *	Initializes the lock information in a relation descriptor.
 */
extern
void
RelationInitLockInfo ARGS((
	Relation	relation
));

/*
 * RelationDiscardLockInfo --
 *	Discards the lock information in a relation descriptor.
 */
extern
void
RelationDiscardLockInfo ARGS((
	Relation	relation
));

/*
 * RelationSetLockForDescriptorOpen --
 *	Sets read locks for a relation descriptor.
 */
extern
void
RelationSetLockForDescriptorOpen ARGS((
	Relation	relation
));

/*
 * RelationSetLockForRead --
 *	Sets relation level read lock.
 */
extern
void
RelationSetLockForRead ARGS((
	Relation	relation
));

/*
 * RelationSetLockForWrite --
 *	Sets relation level write lock.
 */
extern
void
RelationSetLockForWrite ARGS((
	Relation	relation
));

/*
 * RelationUnsetLockForRead --
 *	Unsets relation level read lock.
 */
extern
void
RelationUnsetLockForRead ARGS((
	Relation	relation
));

/*
 * RelationUnsetLockForWrite --
 *	Unsets relation level write lock.
 */
extern
void
RelationUnsetLockForWrite ARGS((
	Relation	relation
));

/*
 * RelationSetLockForTupleRead --
 *	Sets tuple level read lock.
 */
extern
void
RelationSetLockForTupleRead ARGS((
	Relation	relation
	ItemPointer	itemPointer
));

/*
 * RelationSetLockForReadPage --
 *	Sets read lock on a page.
 */
extern
void
RelationSetLockForReadPage ARGS((
	Relation	relation,
	PagePartition	partition,
	ItemPointer	itemPointer
));

/*
 * RelationSetLockForWritePage --
 *	Sets write lock on a page.
 */
extern
void
RelationSetLockForWritePage ARGS((
	Relation	relation,
	PagePartition	partition,
	ItemPointer	itemPointer
));

/*
 * RelationUnsetLockForReadPage --
 *	Frees read lock on a page.
 */
extern
void
RelationUnsetLockForReadPage ARGS((
	Relation	relation,
	PagePartition	partition,
	ItemPointer	itemPointer
));

/*
 * RelationUnsetLockForWritePage --
 *	Frees write lock on a page.
 */
extern
void
RelationUnsetLockForWritePage ARGS((
	Relation	relation,
	PagePartition	partition,
	ItemPointer	itemPointer
));

void
LRelIdAssign ARGS((
	LRelId		*lRelId,
	ObjectId	dbId,
	ObjectId	relId
));

#endif	/* !defined(LMgrIncluded) */
