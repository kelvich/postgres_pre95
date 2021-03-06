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
#include "access/nbtree.h"

RcsId("$Header$");

/*
 * Note:
 *	StrategyNegate, StrategyCommute, and StrategyNegateCommute
 *	assume <, <=, ==, >=, > ordering.
 */
static StrategyNumber	BTNegate[5] = {
    BTGreaterEqualStrategyNumber,
    BTGreaterStrategyNumber,
    InvalidStrategy,
    BTLessStrategyNumber,
    BTLessEqualStrategyNumber
};

static StrategyNumber	BTCommute[5] = {
    BTGreaterStrategyNumber,
    BTGreaterEqualStrategyNumber,
    InvalidStrategy,
    BTLessEqualStrategyNumber,
    BTLessStrategyNumber
};

static StrategyNumber	BTNegateCommute[5] = {
    BTLessEqualStrategyNumber,
    BTLessStrategyNumber,
    InvalidStrategy,
    BTGreaterStrategyNumber,
    BTGreaterEqualStrategyNumber
};

static uint16	BTLessTermData[] = {		/* XXX type clash */
    2,
    BTLessStrategyNumber,
    NegateResult,
    BTLessStrategyNumber,
    NegateResult | CommuteArguments
};

static uint16	BTLessEqualTermData[] = {	/* XXX type clash */
    2,
    BTLessEqualStrategyNumber,
    0x0,
    BTLessEqualStrategyNumber,
    CommuteArguments
};

static uint16	BTGreaterEqualTermData[] = {	/* XXX type clash */
    2,
    BTGreaterEqualStrategyNumber,
    0x0,
    BTGreaterEqualStrategyNumber,
    CommuteArguments
};

static uint16	BTGreaterTermData[] = {		/* XXX type clash */
    2,
    BTGreaterStrategyNumber,
    NegateResult,
    BTGreaterStrategyNumber,
    NegateResult | CommuteArguments
};

static StrategyTerm	BTEqualExpressionData[] = {
    (StrategyTerm)BTLessTermData,		/* XXX */
    (StrategyTerm)BTLessEqualTermData,		/* XXX */
    (StrategyTerm)BTGreaterEqualTermData,	/* XXX */
    (StrategyTerm)BTGreaterTermData,		/* XXX */
    NULL
};

static StrategyEvaluationData	BTEvaluationData = {
    /* XXX static for simplicity */

    BTMaxStrategyNumber,
    (StrategyTransformMap)BTNegate,	/* XXX */
    (StrategyTransformMap)BTCommute,	/* XXX */
    (StrategyTransformMap)BTNegateCommute,	/* XXX */

    NULL, NULL, (StrategyExpression)BTEqualExpressionData, NULL, NULL
	    /* XXX */
};

/* ----------------------------------------------------------------
 *	RelationGetBTStrategy
 * ----------------------------------------------------------------
 */

StrategyNumber
_bt_getstrat(rel, attno, proc)
    Relation	rel;
    AttributeNumber	attno;
    RegProcedure	proc;
{
    StrategyNumber	strat;

    strat = RelationGetStrategy(rel, attno, &BTEvaluationData, proc);

    Assert(StrategyNumberIsValid(strat));

    return (strat);
}

bool
_bt_invokestrat(rel, attno, strat, left, right)
    Relation rel;
    AttributeNumber attno;
    StrategyNumber strat;
    Datum left;
    Datum right;
{
    return (RelationInvokeStrategy(rel, &BTEvaluationData, attno, strat, 
				   left, right));
}
