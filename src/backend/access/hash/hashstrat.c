/*
 *  btstrat.c -- Srategy map entries for the btree indexed access method
 */

#include "tmp/c.h"
#include "tmp/postgres.h"

#include "storage/page.h"
#include "storage/bufpage.h"

#include "utils/log.h"
#include "utils/rel.h"
#include "utils/excid.h"

#include "access/genam.h"
#include "access/isop.h"
#include "access/hash.h"

RcsId("$Header$");

/* 
 *  only one valid strategy for hash tables: equality. 
 */

static StrategyNumber	HTNegate[1] = {
    InvalidStrategy
};

static StrategyNumber	HTCommute[1] = {
    HTEqualStrategyNumber
};

static StrategyNumber	HTNegateCommute[1] = {
    InvalidStrategy
};

static StrategyEvaluationData	HTEvaluationData = {
    /* XXX static for simplicity */

    HTMaxStrategyNumber,
    (StrategyTransformMap)HTNegate,
    (StrategyTransformMap)HTCommute,
    (StrategyTransformMap)HTNegateCommute,
    NULL	/* eleven pointers are not initialized here XXX */
};

/* ----------------------------------------------------------------
 *	RelationGetHashStrategy
 * ----------------------------------------------------------------
 */

StrategyNumber
_hash_getstrat(rel, attno, proc)
    Relation	rel;
    AttributeNumber	attno;
    RegProcedure	proc;
{
    StrategyNumber	strat;

    strat = RelationGetStrategy(rel, attno, &HTEvaluationData, proc);

    Assert(StrategyNumberIsValid(strat));

    return (strat);
}

bool
_hash_invokestrat(rel, attno, strat, left, right)
    Relation rel;
    AttributeNumber attno;
    StrategyNumber strat;
    Datum left;
    Datum right;
{
    return (RelationInvokeStrategy(rel, &HTEvaluationData, attno, strat, 
				   left, right));
}

























