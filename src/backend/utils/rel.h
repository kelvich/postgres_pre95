/*
 * rel.h --
 *	POSTGRES relation descriptor definitions.
 */

#ifndef	RelIncluded		/* Include this file only once */
#define RelIncluded	1

/*
 * Identification:
 */
#define REL_H	"$Header$"

#include "tmp/postgres.h"

#include "storage/fd.h"
#include "access/istrat.h"
#include "access/tupdesc.h"

#include "catalog/pg_am.h"
#include "catalog/pg_operator.h"
#include "catalog/pg_relation.h"

typedef struct RelationData {
	File			rd_fd;		/* open file descriptor */
	int                     rd_nblocks;     /* number of blocks in rel */
	uint16			rd_refcnt;	/* reference count */
	bool			rd_ismem;	/* rel is in-memory only */
	bool			rd_isnailed;	/* rel is nailed in cache */
	AccessMethodTupleForm	rd_am;		/* AM tuple */
	RelationTupleForm	rd_rel;		/* RELATION tuple */
	ObjectId		rd_id;		/* relations's object id */
	Pointer			lockInfo;	/* ptr. to misc. info. */
	TupleDescriptorData	rd_att;		/* tuple desciptor */
/* VARIABLE LENGTH ARRAY AT END OF STRUCT */
/*	IndexStrategy		rd_is;	*/	/* index strategy */
} RelationData;

typedef RelationData	*Relation;

/* ----------------
 *	RelationPtr is used in the executor to support index scans
 *	where we have to keep track of several index relations in an
 *	array.  -cim 9/10/89
 * ----------------
 */
typedef Relation	*RelationPtr;

#define InvalidRelation	((Relation)NULL)

typedef char	ArchiveMode;

/*
 * RelationIsValid --
 *	True iff relation descriptor is valid.
 */
#define	RelationIsValid(relation) PointerIsValid(relation)

/*
 * RelationGetSystemPort --
 *	Returns system port of a relation.
 *
 * Note:
 *	Assumes relation descriptor is valid.
 */
#define RelationGetSystemPort(relation) ((relation)->rd_fd)

/*
 * RelationHasReferenceCountZero --
 *	True iff relation reference count is zero.
 *
 * Note:
 *	Assumes relation descriptor is valid.
 */
#define RelationHasReferenceCountZero(relation) \
	((bool)((relation)->rd_refcnt == 0))

/*
 * RelationSetReferenceCount --
 *	Sets relation reference count.
 */
#define RelationSetReferenceCount(relation,count) ((relation)->rd_refcnt = count)

/*
 * RelationIncrementReferenceCount --
 *	Increments relation reference count.
 */
#define RelationIncrementReferenceCount(relation) ((relation)->rd_refcnt += 1);

/*
 * RelationDecrementReferenceCount --
 *	Decrements relation reference count.
 */
#define RelationDecrementReferenceCount(relation) ((relation)->rd_refcnt -= 1)

/*
 * RelationGetAccessMethodTupleForm --
 *	Returns access method attribute values for a relation.
 *
 * Note:
 *	Assumes relation descriptor is valid.
 */
#define RelationGetAccessMethodTupleForm(relation) ((relation)->rd_am)

/*
 * RelationGetRelationTupleForm --
 *	Returns relation attribute values for a relation.
 *
 * Note:
 *	Assumes relation descriptor is valid.
 */
#define RelationGetRelationTupleForm(relation) ((relation)->rd_rel)

/*
 * RelationGetTupleDescriptor --
 *	Returns tuple descriptor for a relation.
 *
 * Note:
 *	Assumes relation descriptor is valid.
 */
#define RelationGetTupleDescriptor(relation) (&(relation)->rd_att)

/* 
 * RelationGetRelationId --
 *
 *  returns the object id of the relation
 *
 */
#define RelationGetRelationId(relation) ((relation)->rd_id)

/*
 * RelationGetFile --
 *
 *    Returns the open File decscriptor
 */
#define RelationGetFile(relation) ((relation)->rd_fd)


/*
 * RelationGetRelationName --
 *
 *    Returns a Relation Name
 */
#define RelationGetRelationName(relation) (&(relation)->rd_rel->relname)

/*
 * RelationGetRelationName --
 *
 *    Returns a the number of attributes.
 */
#define RelationGetNumberOfAttributes(relation) ((relation)->rd_rel->relnatts)

/*
 * RelationGetIndexStrategy --
 *	Returns index strategy for a relation.
 *
 * Note:
 *	Assumes relation descriptor is valid.
 *	Assumes relation descriptor is for an index relation.
 */
extern
IndexStrategy
RelationGetIndexStrategy ARGS((
	Relation	relation
));

/*
 * RelationSetIndexSupport --
 *	Sets index strategy and support info for a relation.
 *
 * Note:
 *	Assumes relation descriptor is a valid pointer to sufficient space.
 *	Assumes index strategy is valid.  Assumes support is valid if non-
 *	NULL.
 */
extern
void
RelationSetIndexSupport ARGS((
	Relation	relation,
	IndexStrategy	strategy,
	RegProcedure	*support
));

#endif	/* !defined(RelIncluded) */
