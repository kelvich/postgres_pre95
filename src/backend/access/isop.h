
; /*
; * 
; * POSTGRES Data Base Management System
; * 
; * Copyright (c) 1988 Regents of the University of California
; * 
; * Permission to use, copy, modify, and distribute this software and its
; * documentation for educational, research, and non-profit purposes and
; * without fee is hereby granted, provided that the above copyright
; * notice appear in all copies and that both that copyright notice and
; * this permission notice appear in supporting documentation, and that
; * the name of the University of California not be used in advertising
; * or publicity pertaining to distribution of the software without
; * specific, written prior permission.  Permission to incorporate this
; * software into commercial products can be obtained from the Campus
; * Software Office, 295 Evans Hall, University of California, Berkeley,
; * Ca., 94720 provided only that the the requestor give the University
; * of California a free licence to any derived software for educational
; * and research purposes.  The University of California makes no
; * representations about the suitability of this software for any
; * purpose.  It is provided "as is" without express or implied warranty.
; * 
; */



/*
 * isop.h --
 *	POSTGRES index strategy manipulation operator definitions.
 *
 * Identification:
 *	$Header$
 */

#ifndef	ISOpIncluded	/* Include this file only once */
#define ISOpIncluded	1

#include "c.h"

#include "istrat.h"

typedef struct StrategyTransformMapData {
	StrategyNumber	strategy[1];	/* VARIABLE LENGTH ARRAY */
} StrategyTransformMapData;	/* VARIABLE LENGTH STRUCTURE */

typedef StrategyTransformMapData	*StrategyTransformMap;

typedef struct StrategyOperatorData {
	StrategyNumber	strategy;
	bits16		flags;		/* scan qualification flags h/skey.h */
} StrategyOperatorData;

typedef StrategyOperatorData	*StrategyOperator;

typedef struct StrategyTermData {	/* conjunctive term */
	uint16			degree;
	StrategyOperatorData	operatorData[1];	/* VARIABLE LENGTH */
} StrategyTermData;	/* VARIABLE LENGTH STRUCTURE */

typedef StrategyTermData	*StrategyTerm;

typedef struct StrategyExpressionData {	/* disjunctive normal form */
	StrategyTerm	term[1];	/* VARIABLE LENGTH ARRAY */
} StrategyExpressionData;	/* VARIABLE LENGTH STRUCTURE */

typedef StrategyExpressionData	*StrategyExpression;

typedef struct StrategyEvaluationData {
	StrategyNumber		maxStrategy;
	StrategyTransformMap	negateTransform;
	StrategyTransformMap	commuteTransform;
	StrategyTransformMap	negateCommuteTransform;
	StrategyExpression	expression[12];	/* XXX VARIABLE LENGTH */
} StrategyEvaluationData;	/* VARIABLE LENGTH STRUCTURE */

typedef StrategyEvaluationData	*StrategyEvaluation;

/*
 * StrategyTransformMapIsValid --
 *	Returns true iff strategy transformation map is valid.
 */
extern
bool
StrategyTransformMapIsValid ARGS((
	StrategyTransformMap	transform
));

/*
 * StrategyOperatorIsValid --
 *	Returns true iff strategy operator is valid.
 */
extern
bool
StrategyOperatorMapIsValid ARGS((
	StrategyOperator	operator,
	Strategy		maxStrategy
));

/*
 * StrategyTermIsValid --
 *	Returns true iff strategy term is valid.
 */
extern
bool
StrategyTermIsValid ARGS((
	StrategyTerm	term,
	Strategy	maxStrategy
));

/*
 * StrategyExpressionIsValid --
 *	Returns true iff strategy expression is valid.
 */
extern
bool
StrategyExpressionIsValid ARGS((
	StrategyExpression	expression
	Strategy		maxStrategy
));

/*
 * StrategyEvaluationIsValid --
 *	Returns true iff strategy evaluation information is valid.
 */
extern
bool
StrategyEvaluationIsValid ARGS((
	StrategyEvaluation	evaluation
));

/*
 * RelationGetStrategy --
 *	Returns the strategy for an index relation and registered procedure.
 *	Returns InvalidStrategyNumber if registered procedure is unknown.
 *
 * Note:
 *	Assumes relation is valid index relation.
 *	Assumes attribute number is in valid range.
 *	Assumes maximum strategy is valid.
 *	Assumes strategy transformation maps are valid.
 *	Assumes registered procedure is valid.
 */
extern
StrategyNumber
RelationGetStrategy ARGS((
	Relation		relation,
	AttributeNumber		attributeNumber,
	StrategyNumber		maxStrategy,
	StrategyTransformMap	negateTransform,
	StrategyTransformMap	commuteTransform,
	StrategyTransformMap	negateCommuteTransform,
	RegProcedure		procedure
));

/*
 * RelationInvokeStrategy --
 *	Returns the result of evaluating a strategy for an index relation.
 *
 * Note:
 *	Assumes relation is a valid index relation.
 *	Assumes strategy evalution information is valid.
 *	Assumes attribute number is in valid range.
 *	Assumes strategy is valid.
 */
extern
bool		/* XXX someday, this may return Datum */
RelationInvokeStrategy ARGS((
	Relation		relation,
	StrategyEvaluation	evaluation,
	AttributeNumber		attributeNumber,
	StrategyNumber		strategy,
	Datum			left,
	Datum			right
));

#endif	/* !defined(ISOpIncluded) */
