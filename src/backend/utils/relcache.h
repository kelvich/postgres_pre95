/*
 * relcache.h --
 *	Relation descriptor cache definitions.
 *
 * Identification:
 *	$Header$
 */

#ifndef	RelCacheIncluded	/* Include this file only once. */
#define RelCacheIncluded	1

#include "tmp/postgres.h"
#include "utils/rel.h"

/*
 * relation lookup routines
 */
extern Relation
RelationNameGetRelation ARGS((
	Name		relationName
));
extern Relation
getreldesc ARGS((
	Name		relationName
));

extern Relation
RelationIdCacheGetRelation ARGS((
	ObjectId	relationId
));

extern Relation
RelationNameCacheGetRelation ARGS((
	ObjectId	relationId
));

extern Relation
RelationIdGetRelation ARGS((
	ObjectId	relationId
));

#ifdef EXTERN_UNDEFINED_FUNCTIONS
/*
 * RelationAllocate --
 *	Allocates a relation descriptor.
 */
extern
Relation
RelationAllocate ARGS((
	void	/* XXX ??? */
));
#endif

/*
 * XXXX
 *
 * The following two functions should be private to relcache.c but
 * arn't.
 *
 */

extern File
relopen ARGS((
	char	*relationName,
	int	flags,
	int	mode
));

extern void
RelationRegisterRelation ARGS((
	Relation	relation
));

extern void
RelationRegisterTempRel ARGS((
	Relation relation
));

/*
 * RelationFlushRelation
 *
 *   Actually blows away a relation... RelationFree doesn't do 
 *   anything anymore.
 */

extern void
RelationFlushRelation ARGS((
	Relation        relation,
	bool            onlyFlushReferenceCountZero
));

/*
 * RelationIdInvalidateRelationCacheByRelationId --
 */
extern
void
RelationIdInvalidateRelationCacheByRelationId ARGS((
	ObjectId	relationId
));

/*
 * RelationIdInvalidateRelationCacheByAccessMethodId --
 */
extern
void
RelationIdInvalidateRelationCacheByAccessMethodId ARGS((
	ObjectId	accessMethodId
));

/*
 * InvalidateRelationCacheIndexes --
 */
extern
void
InvalidateRelationCacheIndexes ARGS((
	void
));

/*
 * RelationCacheInvalidate
 *
 *   Will blow away either all the cached relation descriptors or
 *   those that have a zero reference count.
 *
 */

extern void
RelationCacheInvalidate ARGS((
	bool            onlyFlushReferenceCountZero
));

#endif	/* !defined(RelCacheIncluded) */
