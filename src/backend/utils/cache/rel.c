/*
 * rel.c --
 *	POSTGRES relation descriptor code.
 */

/* #define RELREFDEBUG	1 */

#include "tmp/postgres.h"
#include "tmp/miscadmin.h"

RcsId("$Header$");

#include "access/istrat.h"
#include "access/tupdesc.h"

#include "utils/rel.h"
#include "storage/fd.h"

/* 
 *	RelationIsValid is now a macro in rel.h -cim 4/27/91
 */

/* ----------------
 *	RelationGetSystemPort
 * ----------------
 */
File
RelationGetSystemPort(relation)
    Relation	relation;
{
    return (relation->rd_fd);
}

/* ----------------
 *	RelationHasReferenceCountZero
 * ----------------
 */
bool
RelationHasReferenceCountZero(relation)
    Relation	relation;
{
    return ((bool)(relation->rd_refcnt == 0));
}

/* ----------------
 *	RelationSetReferenceCount
 * ----------------
 */
void
RelationSetReferenceCount(relation, count)
    Relation	relation;
    Count		count;
{
#ifdef	RELREFDEBUG
    elog(-2, "Set(%.16s[%d->%d])", RelationGetRelationName(relation),
	 relation->rd_refcnt, count);
#endif

    relation->rd_refcnt = count;
}

/* ----------------
 *	RelationIncrementReferenceCount
 * ----------------
 */
void
RelationIncrementReferenceCount(relation)
    Relation	relation;
{
#ifdef	RELREFDEBUG
    elog(-2, "Increment(%.16s[%d->%d])", RelationGetRelationName(relation),
	 relation->rd_refcnt, 1 + relation->rd_refcnt);
#endif

    relation->rd_refcnt += 1;
}

/* ----------------
 *	RelationDecrementReferenceCount
 * ----------------
 */
void
RelationDecrementReferenceCount(relation)
    Relation	relation;
{
#ifdef	RELREFDEBUG
    elog(-2, "Decrement(%.16s[%d->%d])", RelationGetRelationName(relation),
	 relation->rd_refcnt, -1 + relation->rd_refcnt);
#endif
    
    relation->rd_refcnt -= 1;
}

/* ----------------
 *	RelationGetAccessMethodTupleForm
 * ----------------
 */
AccessMethodTupleForm
RelationGetAccessMethodTupleForm(relation)
    Relation	relation;
{
    return (relation->rd_am);
}

/* ----------------
 *	RelationGetRelationTupleForm
 * ----------------
 */
RelationTupleForm
RelationGetRelationTupleForm(relation)
    Relation	relation;
{
    return (relation->rd_rel);
}


/* ----------------
 *	RelationGetTupleDescriptor
 * ----------------
 */
TupleDescriptor
RelationGetTupleDescriptor(relation)
    Relation	relation;
{
    return (&relation->rd_att);
}

/* ----------------
 *	RelationGetIndexStrategy
 * ----------------
 */
IndexStrategy
RelationGetIndexStrategy(relation)
    Relation	relation;
{
    return (IndexStrategy)
	relation->rd_att.data[ relation->rd_rel->relnatts];
}

/* ----------------
 *	RelationSetIndexSupport
 *
 *	This routine saves two pointers -- one to the IndexStrategy, and
 *	one to the RegProcs that support the indexed access method.  These
 *	pointers are stored in the space following the attribute data in the
 *	reldesc.
 * ----------------
 */
void
RelationSetIndexSupport(relation, strategy, support)
    Relation	relation;
    IndexStrategy	strategy;
    RegProcedure	*support;
{
    IndexStrategy	*relationIndexStrategyP;

    Assert(PointerIsValid(relation));
    Assert(IndexStrategyIsValid(strategy));

    relationIndexStrategyP = (IndexStrategy *)
	&relation->rd_att.data[relation->rd_rel->relnatts];

    *relationIndexStrategyP = strategy;
    relationIndexStrategyP = (IndexStrategy *)
	(((char *) relationIndexStrategyP) + sizeof(relationIndexStrategyP));
    *relationIndexStrategyP = (IndexStrategy) support;
}

/* ----------------
 *	RelationGetRelationId
 * ----------------
 */
ObjectId
RelationGetRelationId(relation)
    Relation	relation;
{
    return(relation->rd_id);
}

/* ----------------
 *	RelationGetRelationName
 * ----------------
 */
Name
RelationGetRelationName(relation)
    Relation	relation;
{
    return(&relation->rd_rel->relname);
}

/* ----------------
 *	RelationGetFile
 * ----------------
 */
File
RelationGetFile(relation)
    Relation	relation;
{
    return(relation->rd_fd);
}

/* ----------------
 *	RelationGetNumberOfAttributes
 * ----------------
 */
AttributeNumber
RelationGetNumberOfAttributes(relation)
    Relation	relation;
{
    return(relation->rd_rel->relnatts);
}

