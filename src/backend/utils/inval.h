/*
 * inval.h --
 *	POSTGRES cache invalidation dispatcher definitions.
 *
 * Identification:
 *	$Header$
 */

#ifndef	InvalIncluded	/* Include this file only once */
#define InvalIncluded	1

#include "tmp/postgres.h"
#include "access/htup.h"
#include "utils/rel.h"

/*
 * DiscardInvalid --
 *	Causes the invalidated cache state to be discarded.
 *
 * Note:
 *	This should be called as the first step in processing a transaction.
 *	This should be called while waiting for a query from the front end
 *	when other backends are active.
 */
extern
void
DiscardInvalid ARGS((
	void
));

/*
 * RegisterInvalid --
 *	Causes registration of invalidated state with other backends iff true.
 *
 * Note:
 *	This should be called as the last step in processing a transaction.
 */
extern
void
RegisterInvalid ARGS((
	bool	send
));

/*
 * SetRefreshWhenInvalidate --
 *	Causes the local caches to be immediately refreshed iff true.
 */
void
SetRefreshWhenInvalidate ARGS((
	bool	on
));

/*
 * RelationIdInvalidateHeapTuple --
 *	Causes the given tuple in a relation to be invalidated.
 *
 * Note:
 *	Assumes object id is valid.
 *	Assumes tuple is valid.
 */
extern
void
RelationIdInvalidateHeapTuple ARGS((
	ObjectId	relationId,
	HeapTuple	tuple
));

/*
 * The following definitions were sucked out of...
 *
 * linval.h --
 *	POSTGRES local cache invalidation definitions.
 *
 * ...which failed to die when its functions were subsumed in inval.c.
 */

typedef struct InvalidationUserData {
	struct InvalidationUserData	*dataP[1];	/* VARIABLE LENGTH */
} InvalidationUserData;	/* VARIABLE LENGTH STRUCTURE */

typedef struct InvalidationEntryData {
	InvalidationUserData	*nextP;
	InvalidationUserData	userData;	/* VARIABLE LENGTH ARRAY */
} InvalidationEntryData;	/* VARIABLE LENGTH STRUCTURE */

typedef Pointer InvalidationEntry;

typedef InvalidationEntry	LocalInvalid;

#define EmptyLocalInvalid	NULL

/*
 * InvalididationEntryAllocate --
 *	Allocates an invalidation entry.
 */
InvalidationEntry
InvalidationEntryAllocate ARGS((
	uint16	size
));

/*
 * LocalInvalidRegister --
 *	Returns a new local cache invalidation state containing a new entry.
 */
extern
LocalInvalid
LocalInvalidRegister ARGS((
	LocalInvalid		invalid,
	InvalidationEntry	entry
));

/*
 * LocalInvalidInvalidate --
 *	Processes, then frees all entries in a local cache invalidation state.
 */
extern
void
LocalInvalidInvalidate ARGS((
	LocalInvalid	invalid,
	void		(*function)(
		InvalidationEntry	entry
	)
));

void getmyrelids ARGS((void ));

#endif	/* !defined(InvalIncluded) */

