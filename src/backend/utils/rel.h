/*
 * rel.h --
 *	POSTGRES relation descriptor definitions.
 *
 * Identification:
 *	$Header$
 */

#ifndef	RelIncluded	/* Include this file only once. */
#define RelIncluded	1

#ifndef C_H
#include "c.h"
#endif

#include "os.h"

#include "oid.h"
#include "cat.h"
#include "istrat.h"
#include "tupdesc.h"
#include "fd.h"

typedef struct RelationData {
	File			rd_fd;		/* open file descriptor */
	uint16			rd_refcnt;	/* reference count */
	AccessMethodTupleForm	rd_am;		/* AM tuple */
	RelationTupleForm	rd_rel;		/* RELATION tuple */
	ObjectId		rd_id;		/* relations's object id */
	Pointer			lockInfo;	/* ptr. to misc. info. */
	TupleDescriptorData	rd_att;		/* tuple desciptor */
/* VARIABLE LENGTH ARRAY AT END OF STRUCT */
/*	IndexStrategy		rd_is;		/* index strategy */
} RelationData;

typedef RelationData	*Relation;

#define InvalidRelation	((Relation)NULL)

typedef char	ArchiveMode;

/*
 * RelationIsValid --
 *	True iff relation descriptor is valid.
 */
extern
bool
RelationIsValid ARGS ((
	Relation	relation
));

/*
 * RelationGetSystemPort --
 *	Returns system port of a relation.
 *
 * Note:
 *	Assumes relation descriptor is valid.
 */
extern
SystemPort
RelationGetSystemPort ARGS((
	Relation	relation
));

/*
 * RelationHasReferenceCountZero --
 *	True iff relation reference count is zero.
 *
 * Note:
 *	Assumes relation descriptor is valid.
 */
extern
bool
RelationHasReferenceCountZero ARGS((
	Relation	relation
));

/*
 * RelationSetReferenceCount --
 *	Sets relation reference count.
 */
extern
void
RelationSetReferenceCount ARGS((
	Relation	relation,
	Count		count
));

/*
 * RelationIncrementReferenceCount --
 *	Increments relation reference count.
 */
extern
void
RelationIncrementReferenceCount ARGS((
	Relation	relation
));

/*
 * RelationDecrementReferenceCount --
 *	Decrements relation reference count.
 */
extern
void
RelationDecrementReferenceCount ARGS((
	Relation	relation
));

/*
 * RelationGetAccessMethodTupleForm --
 *	Returns access method attribute values for a relation.
 *
 * Note:
 *	Assumes relation descriptor is valid.
 */
extern
AccessMethodTupleForm
RelationGetAccessMethodTupleForm ARGS((
	Relation	relation
));

/*
 * RelationGetRelationTupleForm --
 *	Returns relation attribute values for a relation.
 *
 * Note:
 *	Assumes relation descriptor is valid.
 */
extern
RelationTupleForm
RelationGetRelationTupleForm ARGS((
	Relation	relation
));

/*
 * RelationGetTupleDescriptor --
 *	Returns tuple descriptor for a relation.
 *
 * Note:
 *	Assumes relation descriptor is valid.
 */
extern
TupleDescriptor
RelationGetTupleDescriptor ARGS((
	Relation	relation
));

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
 * RelationSetIndexStrategy --
 *	Sets index strategy for a relation.
 *
 * Note:
 *	Assumes relation descriptor is a valid pointer to sufficient space.
 *	Assumes index strategy is valid.
 */
extern
void
RelationSetIndexStrategy ARGS((
	Relation	relation,
	IndexStrategy	strategy
));

/* 
 * RelationGetRelationId --
 *
 *  returns the object id of the relation
 *
 */

extern
ObjectId
RelationGetRelationId ARGS((
	Relation	relation
));

/*
 * RelationGetFile --
 *
 *    Returns the open File decscriptor
 */

extern
File
RelationGetFile ARGS((
	Relation	relation
));


/*
 * RelationGetRelationName --
 *
 *    Returns a Relation Name
 */

extern
Name
RelationGetRelationName ARGS((
	Relation	relation
));

extern
AttributeNumber
RelationGetNumberOfAttributes ARGS((
	Relation relation
));				  
#endif	/* !defined(RelIncluded) */
