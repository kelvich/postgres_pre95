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
 *
 *      Many of the RelationGet...() functions are now macros in rel.h
 *		-mer 3/2/92
 */

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

