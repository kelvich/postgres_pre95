/*
 * inval.h --
 *	POSTGRES cache invalidation dispatcher definitions.
 *
 * Identification:
 *	$Header$
 */

#ifndef	InvalIncluded	/* Include this file only once */
#define InvalIncluded	1

#include "tmp/c.h"
#include "access/htup.h"
#include "tmp/oid.h"
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

#endif	/* !defined(InvalIncluded) */
