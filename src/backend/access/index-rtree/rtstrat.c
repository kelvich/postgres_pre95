/*
 *  rtstrat.c -- strategy map data for rtrees.
 */
#include "tmp/c.h"

#include "utils/rel.h"

#include "access/isop.h"
#include "access/istrat.h"
#include "access/rtree.h"

RcsId("$Header$");

/*
 *  Note:  negate, commute, and negatecommute all assume that operators are
 *	   ordered as follows in the strategy map:
 *
 *	contained-by, intersects, disjoint, contains, not-contained-by,
 *	not-contains, equal
 *
 *  The negate, commute, and negatecommute arrays are used by the planner
 *  to plan indexed scans over data that appears in the qualificiation in
 *  a boolean negation, or whose operands appear in the wrong order.  For
 *  example, if the operator "<%" means "contains", and the user says
 *
 *	where not rel.box <% "(10,10,20,20)"::box
 *
 *  the planner can plan an index scan by noting that rtree indices have
 *  an operator in their operator class for negating <%.
 *
 *  Similarly, if the user says something like
 *
 *	where "(10,10,20,20)"::box <% rel.box
 *
 *  the planner can see that the rtree index on rel.box has an operator in
 *  its opclass for commuting <%, and plan the scan using that operator.
 *  This added complexity in the access methods makes the planner a lot easier
 *  to write.
 */

/* if a op b, what operator tells us if (not a op b)? */
static StrategyNumber	RTNegate[7] = {
	RTNotCBStrategyNumber,		/* !contained-by */
	RTDisjointStrategyNumber,	/* !intersects */
	RTIntersectsStrategyNumber,	/* !disjoint */
	RTNotCStrategyNumber,		/* !contains */
	RTContainedByStrategyNumber,	/* !not-contained-by */
	RTContainsStrategyNumber,	/* !not-contains */
	InvalidStrategy			/* don't know how to negate = */
};

/* if a op_1 b, what is the operator op_2 such that b op_2 a? */
static StrategyNumber	RTCommute[7] = {
	RTContainsStrategyNumber,	/* commute contained-by */
	InvalidStrategy,		/* intersects commutes itself */
	InvalidStrategy,		/* disjoint commutes itself */
	RTContainedByStrategyNumber,	/* commute contains */
	RTNotCStrategyNumber,		/* commute not-contained-by */
	RTNotCBStrategyNumber,		/* commute not-contains */
	InvalidStrategy			/* equal commutes itself */
};

/* if a op_1 b, what is the operator op_2 such that (b !op_2 a)? */
static StrategyNumber	RTNegateCommute[7] = {
	RTNotCStrategyNumber,
	RTDisjointStrategyNumber,
	RTIntersectsStrategyNumber,
	RTNotCBStrategyNumber,
	RTContainsStrategyNumber,
	RTContainedByStrategyNumber,
	InvalidStrategy
};

/*
 *  Now do the TermData arrays.  These exist in case the user doesn't give
 *  us a full set of operators for a particular operator class.  The idea
 *  is that by making multiple comparisons using any one of the supplied
 *  operators, we can decide whether two n-dimensional polygons are equal.
 *  For example, if a contains b and b contains a, we may conclude that
 *  a and b are equal.
 *
 *  The presence of the TermData arrays in all this is a historical accident.
 *  Early in the development of the POSTGRES access methods, it was believed
 *  that writing functions was harder than writing arrays.  This is wrong;
 *  TermData is hard to understand and hard to get right.  In general, when
 *  someone populates a new operator class, the populate it completely.  If
 *  Mike Hirohama had forced Cimarron Taylor to populate the strategy map
 *  for btree int2_ops completely in 1988, you wouldn't have to deal with
 *  all this now.  Too bad for you.
 *
 *  Since you can't necessarily do this in all cases (for example, you can't
 *  do it given only "intersects" or "disjoint"), TermData arrays for some
 *  operators don't appear below.
 *
 *  Note that if you DO supply all the operators required in a given opclass
 *  by inserting them into the pg_opclass system catalog, you can get away
 *  without doing all this TermData stuff.  Since the rtree code is intended
 *  to be a reference for access method implementors, I'm doing TermData
 *  correctly here.
 *
 *  Note on style:  these are all actually of type StrategyTermData, but
 *  since those have variable-length data at the end of the struct we can't
 *  properly initialize them if we declare them to be what they are.
 */

/* if you only have "contained-by", how do you determine equality? */
static uint16 RTContainedByTermData[] = {
	2,					/* make two comparisons */
	RTContainedByStrategyNumber,		/* use "a contained-by b" */
	0x0,					/* without any magic */
	RTContainedByStrategyNumber,		/* then use contained-by, */
	CommuteArguments			/* swapping a and b */
};

/* if you only have "contains", how do you determine equality? */
static uint16 RTContainsTermData[] = {
	2,					/* make two comparisons */
	RTContainsStrategyNumber,		/* use "a contains b" */
	0x0,					/* without any magic */
	RTContainsStrategyNumber,		/* then use contains again, */
	CommuteArguments			/* swapping a and b */
};

/* if you only have "not-contained-by", how do you determine equality? */
static uint16 RTNotCBTermData[] = {
	2,					/* make two comparisons */
	RTNotCBStrategyNumber,			/* "a not-contained-by b" ... */
	NegateResult,				/* ... with result negated, */
	RTNotCBStrategyNumber,			/* then the same thing */
	NegateResult|CommuteArguments		/* with a and b swapped */
};

/* if you only have "not-contains", how do you determine equality? */
static uint16 RTNotCTermData[] = {
	2,					/* make two comparisons */
	RTNotCStrategyNumber,			/* "a not-contains b" ... */
	NegateResult,				/* ... with result negated, */
	RTNotCStrategyNumber,			/* then the same thing */
	NegateResult|CommuteArguments		/* with a and b swapped */
};

/* now put all that together in one place for the planner */
static StrategyTerm RTEqualExpressionData[] = {
	(StrategyTerm) RTContainedByTermData,
	(StrategyTerm) RTContainsTermData,
	(StrategyTerm) RTNotCBTermData,
	(StrategyTerm) RTNotCTermData,
	NULL
};

/*
 *  If you were sufficiently attentive to detail, you would go through
 *  the ExpressionData pain above for every one of the seven strategies
 *  we defined.  I am not.  Now we declare the StrategyEvaluationData
 *  structure that gets shipped around to help the planner and the access
 *  method decide what sort of scan it should do, based on (a) what the
 *  user asked for, (b) what operators are defined for a particular opclass,
 *  and (c) the reams of information we supplied above.
 *
 *  The idea of all of this initialized data is to make life easier on the
 *  user when he defines a new operator class to use this access method.
 *  By filling in all the data, we let him get away with leaving holes in his
 *  operator class, and still let him use the index.  The added complexity
 *  in the access methods just isn't worth the trouble, though.
 */

static StrategyEvaluationData RTEvaluationData = {
	7,					/* # of strategies */
	(StrategyTransformMap) RTNegate,	/* how to do (not qual) */
	(StrategyTransformMap) RTCommute,	/* how to swap operands */
	(StrategyTransformMap) RTNegateCommute,	/* how to do both */
	NULL,					/* express contained-by */
	NULL,					/* express intersects */
	NULL,					/* express disjoint */
	NULL,					/* express contains */
	NULL,					/* express not-contained-by */
	NULL,					/* express not-contains */
	(StrategyExpression) RTEqualExpressionData	/* express equal */
};

StrategyNumber
RelationGetRTStrategy(r, attnum, proc)
	Relation r;
	AttributeNumber attnum;
	RegProcedure proc;
{
	return (RelationGetStrategy(r, attnum, &RTEvaluationData, proc));
}

bool
RelationInvokeRTStrategy(r, attnum, s, left, right)
	Relation r;
	AttributeNumber attnum;
	StrategyNumber s;
	Datum left;
	Datum right;
{
	return (RelationInvokeStrategy(r, &RTEvaluationData, attnum, s,
				       left, right));
}
