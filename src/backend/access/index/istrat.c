/* ----------------------------------------------------------------
 *   FILE
 *	istrat.c
 *	
 *   DESCRIPTION
 *	index scan strategy manipulation code and 
 *	index strategy manipulation operator code.
 *
 *   INTERFACE ROUTINES
 *	StrategyNumberIsValid
 *	StrategyNumberIsInBounds
 *	StrategyMapIsValid
 *	IndexStrategyIsValid
 *	StrategyMapGetScanKeyEntry
 *	IndexStrategyGetStrategyMap
 *	AttributeNumberGetIndexStrategySize
 *	StrategyTransformMapIsValid
 *	StrategyOperatorIsValid
 *	StrategyTermIsValid
 *	StrategyExpressionIsValid
 *	StrategyEvaluationIsValid
 *	RelationGetStrategy
 *	RelationInvokeStrategy
 *	IndexStrategyInitialize
 *	
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#include "tmp/postgres.h"

RcsId("$Header$");

#include "access/att.h"
#include "access/attnum.h"
#include "access/heapam.h"
#include "access/istrat.h"
#include "access/isop.h"
#include "access/itup.h"	/* for MaxIndexAttributeNumber */
#include "access/skey.h"
#include "access/tqual.h"	/* for NowTimeQual */

#include "utils/log.h"
#include "utils/rel.h"

#include "catalog/catname.h"
#include "catalog/pg_amop.h"
#include "catalog/pg_index.h"
#include "catalog/pg_proc.h"

#include "internal.h"

/* ----------------------------------------------------------------
 *	           misc strategy support routines
 * ----------------------------------------------------------------
 */

/* ----------------
 *	StrategyNumberIsValid
 * ----------------
 */
bool
StrategyNumberIsValid(strategyNumber)
    StrategyNumber	strategyNumber;
{
    return (bool)
	(strategyNumber != InvalidStrategy);
}

/* ----------------
 *	StrategyNumberIsInBounds
 * ----------------
 */
bool
StrategyNumberIsInBounds(strategyNumber, maxStrategyNumber)
    StrategyNumber	strategyNumber;
    StrategyNumber	maxStrategyNumber;
{
    return (bool)
	(InvalidStrategy < strategyNumber &&
	 strategyNumber <= maxStrategyNumber);
}

/* ----------------
 *	StrategyMapIsValid
 * ----------------
 */
bool
StrategyMapIsValid(map)
    StrategyMap	map;
{
    return (bool) PointerIsValid(map);
}

/* ----------------
 *	IndexStrategyIsValid
 * ----------------
 */
bool
IndexStrategyIsValid(indexStrategy)
    IndexStrategy indexStrategy;
{
    return (bool) PointerIsValid(indexStrategy);
}

/* ----------------
 *	StrategyMapGetScanKeyEntry
 * ----------------
 */
ScanKeyEntry
StrategyMapGetScanKeyEntry(map, strategyNumber)
    StrategyMap		map;
    StrategyNumber	strategyNumber;
{
    Assert(StrategyMapIsValid(map));
    Assert(StrategyNumberIsValid(strategyNumber));

    return (&map->entry[strategyNumber - 1]);
}

/* ----------------
 *	IndexStrategyGetStrategyMap
 * ----------------
 */
StrategyMap
IndexStrategyGetStrategyMap(indexStrategy, maxStrategyNum, attrNum)
    IndexStrategy	indexStrategy;
    StrategyNumber	maxStrategyNum;
    AttributeNumber	attrNum;
{
    Assert(IndexStrategyIsValid(indexStrategy));
    Assert(StrategyNumberIsValid(maxStrategyNum));
    Assert(AttributeNumberIsValid(attrNum));

    maxStrategyNum = AMStrategies(maxStrategyNum);	/* XXX */

    return
	&indexStrategy->strategyMapData[maxStrategyNum * (attrNum - 1)];
}

/* ----------------
 *	AttributeNumberGetIndexStrategySize
 * ----------------
 */
Size
AttributeNumberGetIndexStrategySize(maxAttributeNumber, maxStrategyNumber)
    AttributeNumber	maxAttributeNumber;
    StrategyNumber	maxStrategyNumber;
{
    maxStrategyNumber = AMStrategies(maxStrategyNumber);	/* XXX */

    return
	maxAttributeNumber * maxStrategyNumber * sizeof (ScanKeyData);
}

/* ----------------
 *	StrategyTransformMapIsValid
 * ----------------
 */
bool
StrategyTransformMapIsValid(transform)
    StrategyTransformMap	transform;
{
    return (bool) PointerIsValid(transform);
}

/* ----------------
 *	StrategyOperatorIsValid
 * ----------------
 */
bool
StrategyOperatorIsValid(operator, maxStrategy)
    StrategyOperator	operator;
    StrategyNumber	maxStrategy;
{
    return (bool)
	(PointerIsValid(operator) &&
	 StrategyNumberIsInBounds(operator->strategy, maxStrategy) &&
	 !(operator->flags & ~(NegateResult | CommuteArguments)));
}

/* ----------------
 *	StrategyTermIsValid
 * ----------------
 */
bool
StrategyTermIsValid(term, maxStrategy)
    StrategyTerm	term;
    StrategyNumber	maxStrategy;
{
    Index	index;

    if (! PointerIsValid(term) || term->degree == 0)
	return false;

    for (index = 0; index < term->degree; index += 1) {
	if (! StrategyOperatorIsValid(&term->operatorData[index],
				      maxStrategy)) {

	    return false;
	}
    }

    return true;
}

/* ----------------
 *	StrategyExpressionIsValid
 * ----------------
 */
bool
StrategyExpressionIsValid(expression, maxStrategy)
    StrategyExpression	expression;
    StrategyNumber		maxStrategy;
{
    StrategyTerm	*termP;

    if (!PointerIsValid(expression))
	return true;

    if (!StrategyTermIsValid(expression->term[0], maxStrategy))
	return false;

    termP = &expression->term[1];
    while (StrategyTermIsValid(*termP, maxStrategy))
	termP += 1;

    return (bool)
	(! PointerIsValid(*termP));
}

/* ----------------
 *	StrategyEvaluationIsValid
 * ----------------
 */
bool
StrategyEvaluationIsValid(evaluation)
    StrategyEvaluation	evaluation;
{
    Index	index;

    if (! PointerIsValid(evaluation) ||
	! StrategyNumberIsValid(evaluation->maxStrategy) ||
	! StrategyTransformMapIsValid(evaluation->negateTransform) ||
	! StrategyTransformMapIsValid(evaluation->commuteTransform) ||
	! StrategyTransformMapIsValid(evaluation->negateCommuteTransform)) {

	return false;
    }

    for (index = 0; index < evaluation->maxStrategy; index += 1) {
	if (! StrategyExpressionIsValid(evaluation->expression[index],
					evaluation->maxStrategy)) {

	    return false;
	}
    }
    return true;
}

/* ----------------
 *	StrategyTermEvaluate
 * ----------------
 */
static
bool
StrategyTermEvaluate(term, map, left, right)
    StrategyTerm	term;
    StrategyMap		map;
    Datum		left;
    Datum		right;
{
    Index		index;
    bool		result;
    StrategyOperator	operator;
    ScanKeyEntry	entry;

    for (index = 0, operator = &term->operatorData[0];
	 index < term->degree; index += 1, operator += 1) {

	entry = &map->entry[operator->strategy - 1];

	Assert(RegProcedureIsValid(entry->procedure));

	switch (operator->flags ^ entry->flags) {
	case 0x0:
	    result = (bool) fmgr(entry->procedure, left, right);
	    break;
	    
	case NegateResult:
	    result = (bool) !fmgr(entry->procedure, left, right);
	    break;
	    
	case CommuteArguments:
	    result = (bool) fmgr(entry->procedure, right, left);
	    break;
	    
	case NegateResult | CommuteArguments:
	    result = (bool) !fmgr(entry->procedure, right, left);
	    break;

	default:
	    elog(FATAL, "StrategyTermEvaluate: impossible case %d",
		 operator->flags ^ entry->flags);
	}

	if (!result)
	    return result;
    }
    
    return result;
}


/* ----------------
 *	RelationGetStrategy
 * ----------------
 */
StrategyNumber
RelationGetStrategy(relation, attributeNumber, evaluation, procedure)
    Relation		relation;
    AttributeNumber	attributeNumber;
    StrategyEvaluation	evaluation;
    RegProcedure	procedure;
{
    StrategyNumber	strategy;
    StrategyMap		strategyMap;
    ScanKeyEntry	entry;
    Index		index;
    AttributeNumber 	numattrs;

    Assert(RelationIsValid(relation));
    numattrs = RelationGetNumberOfAttributes(relation);

    Assert(relation->rd_rel->relkind == 'i');	/* XXX use accessor */
    Assert(AttributeNumberIsValid(attributeNumber));
    Assert(OffsetIsInBounds(attributeNumber, 1, 1 + numattrs) );

    Assert(StrategyEvaluationIsValid(evaluation));
    Assert(RegProcedureIsValid(procedure));

    strategyMap =
	IndexStrategyGetStrategyMap(RelationGetIndexStrategy(relation),
				    evaluation->maxStrategy,
				    attributeNumber);

    /* get a strategy number for the procedure ignoring flags for now */
    for (index = 0; index < evaluation->maxStrategy; index += 1) {
	if (strategyMap->entry[index].procedure == procedure) {
	    break;
	}
    }

    if (index == evaluation->maxStrategy)
	return InvalidStrategy;

    strategy = 1 + index;
    entry = StrategyMapGetScanKeyEntry(strategyMap, strategy);

    Assert(!(entry->flags & ~(NegateResult | CommuteArguments)));

    switch (entry->flags & (NegateResult | CommuteArguments)) {
    case 0x0:
	return strategy;

    case NegateResult:
	strategy = evaluation->negateTransform->strategy[strategy - 1];
	break;

    case CommuteArguments:
	strategy = evaluation->commuteTransform->strategy[strategy - 1];
	break;

    case NegateResult | CommuteArguments:
	strategy = evaluation->negateCommuteTransform->strategy[strategy - 1];
	break;

    default:
	elog(FATAL, "RelationGetStrategy: impossible case %d", entry->flags);
    }


    if (! StrategyNumberIsInBounds(strategy, evaluation->maxStrategy)) {
	if (! StrategyNumberIsValid(strategy)) {
	    elog(WARN, "RelationGetStrategy: corrupted evaluation");
	}
    }

    return strategy;
}

/* ----------------
 *	RelationInvokeStrategy
 * ----------------
 */
bool		/* XXX someday, this may return Datum */
RelationInvokeStrategy(relation, evaluation, attributeNumber,
		       strategy, left, right)

    Relation		relation;
    StrategyEvaluation	evaluation;
    AttributeNumber	attributeNumber;
    StrategyNumber	strategy;
    Datum		left;
    Datum		right;
{
    StrategyNumber	newStrategy;
    StrategyMap		strategyMap;
    ScanKeyEntry	entry;
    StrategyTermData	termData;
    AttributeNumber     numattrs;

    Assert(RelationIsValid(relation));
    Assert(relation->rd_rel->relkind == 'i');	/* XXX use accessor */
    numattrs = RelationGetNumberOfAttributes(relation);
	
    Assert(StrategyEvaluationIsValid(evaluation));
    Assert(AttributeNumberIsValid(attributeNumber));
    Assert(OffsetIsInBounds(attributeNumber, 1, 1 + numattrs));
    Assert(StrategyNumberIsInBounds(strategy, evaluation->maxStrategy));

    termData.degree = 1;

    strategyMap =
	IndexStrategyGetStrategyMap(RelationGetIndexStrategy(relation),
				    evaluation->maxStrategy,
				    attributeNumber);

    entry = StrategyMapGetScanKeyEntry(strategyMap, strategy);

    if (RegProcedureIsValid(entry->procedure)) {
	termData.operatorData[0].strategy = strategy;
	termData.operatorData[0].flags = 0x0;

	return
	    StrategyTermEvaluate(&termData, strategyMap, left, right);
    }


    newStrategy = evaluation->negateTransform->strategy[strategy - 1];
    if (newStrategy != strategy && StrategyNumberIsValid(newStrategy)) {
	
	entry = StrategyMapGetScanKeyEntry(strategyMap, newStrategy);

	if (RegProcedureIsValid(entry->procedure)) {
	    termData.operatorData[0].strategy = newStrategy;
	    termData.operatorData[0].flags = NegateResult;

	    return
		StrategyTermEvaluate(&termData, strategyMap, left, right);
	}
    }

    newStrategy = evaluation->commuteTransform->strategy[strategy - 1];
    if (newStrategy != strategy && StrategyNumberIsValid(newStrategy)) {
	
	entry = StrategyMapGetScanKeyEntry(strategyMap, newStrategy);

	if (RegProcedureIsValid(entry->procedure)) {
	    termData.operatorData[0].strategy = newStrategy;
	    termData.operatorData[0].flags = CommuteArguments;

	    return
		StrategyTermEvaluate(&termData, strategyMap, left, right);
	}
    }

    newStrategy = evaluation->negateCommuteTransform->strategy[strategy - 1];
    if (newStrategy != strategy && StrategyNumberIsValid(newStrategy)) {
	
	entry = StrategyMapGetScanKeyEntry(strategyMap, newStrategy);

	if (RegProcedureIsValid(entry->procedure)) {
	    termData.operatorData[0].strategy = newStrategy;
	    termData.operatorData[0].flags = NegateResult | CommuteArguments;

	    return
		StrategyTermEvaluate(&termData, strategyMap, left, right);
	}
    }

    if (PointerIsValid(evaluation->expression[strategy - 1])) {
	StrategyTerm		*termP;

	termP = &evaluation->expression[strategy - 1]->term[0];
	while (PointerIsValid(*termP)) {
	    Index	index;

	    for (index = 0; index < (*termP)->degree; index += 1) {
		entry = StrategyMapGetScanKeyEntry(strategyMap,
				   (*termP)->operatorData[index].strategy);

		if (! RegProcedureIsValid(entry->procedure)) {
		    break;
		}
	    }

	    if (index == (*termP)->degree) {
		return
		    StrategyTermEvaluate(*termP, strategyMap, left, right);
	    }

	    termP += 1;
	}
    }

    elog(WARN, "RelationInvokeStrategy: cannot evaluate strategy %d",
	 strategy);
    /* NOTREACHED */
}

/* ----------------
 *	OperatorRelationFillScanKeyEntry
 * ----------------
 */
static
void
OperatorRelationFillScanKeyEntry(operatorRelation, operatorObjectId, entry)
    Relation		operatorRelation;
    ObjectId		operatorObjectId;
    ScanKeyEntry	entry;
{
    HeapScanDesc	scan;
    ScanKeyData		scanKeyData;
    HeapTuple		tuple;

    scanKeyData.data[0].flags = 0;
    scanKeyData.data[0].attributeNumber = ObjectIdAttributeNumber;
    scanKeyData.data[0].procedure = ObjectIdEqualRegProcedure;
    scanKeyData.data[0].argument = ObjectIdGetDatum(operatorObjectId);

    scan = heap_beginscan(operatorRelation, false, NowTimeQual,
			  1, &scanKeyData);

    tuple = heap_getnext(scan, false, (Buffer *)NULL);
    if (! HeapTupleIsValid(tuple)) {
	elog(WARN, "OperatorObjectIdFillScanKeyEntry: unknown operator %lu",
	     (uint32) operatorObjectId);
    }

    entry->flags = 0;
    entry->procedure = ((OperatorTupleForm) HeapTupleGetForm(tuple))->oprcode;

    if (! RegProcedureIsValid(entry->procedure)) {
	elog(WARN,
	     "OperatorObjectIdFillScanKeyEntry: no procedure for operator %lu",
	     (uint32) operatorObjectId);
    }

    heap_endscan(scan);
}


/* ----------------
 *	IndexStrategyInitialize
 * ----------------
 */
void
IndexStrategyInitialize(indexStrategy, indexObjectId,
			accessMethodObjectId, maxStrategyNumber)
    
    IndexStrategy	indexStrategy;
    ObjectId		indexObjectId;
    ObjectId		accessMethodObjectId;
    StrategyNumber	maxStrategyNumber;
{
    Relation		relation;
    Relation		operatorRelation;
    HeapScanDesc	scan;
    HeapTuple		tuple;
    ScanKeyEntryData	entry[2];
    StrategyMap		map;
    AttributeNumber	attributeNumber;
    AttributeOffset	attributeIndex;
    AttributeNumber	maxAttributeNumber;
    ObjectId		operatorClassObjectId[ MaxIndexAttributeNumber ];

    maxStrategyNumber = AMStrategies(maxStrategyNumber);

    entry[0].flags = 0;
    entry[0].attributeNumber = IndexRelationIdAttributeNumber;
    entry[0].procedure = ObjectIdEqualRegProcedure;
    entry[0].argument = ObjectIdGetDatum(indexObjectId);

    relation = heap_openr(IndexRelationName);
    scan = heap_beginscan(relation, false, NowTimeQual, 1, (ScanKey)entry);
    tuple = heap_getnext(scan, false, (Buffer *)NULL);
    if (! HeapTupleIsValid(tuple))
	elog(WARN, "IndexStrategyInitialize: corrupted catalogs");

    /*
     * XXX note that the following assumes the INDEX tuple is well formed and
     * that the key[] and class[] are 0 terminated.
     */
    attributeIndex = 0;
    for (;;) {
	IndexTupleForm	iform;
	ObjectId	objectId;

	iform = (IndexTupleForm) HeapTupleGetForm(tuple);
	objectId = iform->indkey[attributeIndex];

	if (! ObjectIdIsValid(objectId)) {
	    if (attributeIndex == 0) {
		elog(WARN, "InitializeIndexStrategy: invalid IDEX");
	    }
	    break;
	}

	objectId = iform->indclass[attributeIndex];
	if (!ObjectIdIsValid(objectId))
	    elog(WARN, "InitializeIndexStrategy: corrupted INDEX tuple");

	operatorClassObjectId[attributeIndex] = objectId;
	attributeIndex += 1;
    }
    
    maxAttributeNumber = attributeIndex;

    heap_endscan(scan);
    heap_close(relation);

    entry[0].flags = 0;
    entry[0].attributeNumber =
	AccessMethodOperatorAccessMethodIdAttributeNumber;
    entry[0].procedure = ObjectIdEqualRegProcedure;
    entry[0].argument =  ObjectIdGetDatum(accessMethodObjectId);

    entry[1].flags = 0;
    entry[1].attributeNumber =
	AccessMethodOperatorOperatorClassIdAttributeNumber;
    entry[1].procedure = ObjectIdEqualRegProcedure;

    relation = heap_openr(AccessMethodOperatorRelationName);
    operatorRelation = heap_openr(OperatorRelationName);

    for (attributeNumber = maxAttributeNumber; attributeNumber > 0;
	 attributeNumber -= 1) {

	StrategyNumber	strategy;

	entry[1].argument =
	    ObjectIdGetDatum(operatorClassObjectId[attributeNumber - 1]);

	map = IndexStrategyGetStrategyMap(indexStrategy,
					  maxStrategyNumber,
					  attributeNumber);

	for (strategy = 1; strategy <= maxStrategyNumber; strategy += 1)
	    ScanKeyEntrySetIllegal(StrategyMapGetScanKeyEntry(map, strategy));

	scan = heap_beginscan(relation, false, NowTimeQual, 2, (ScanKey)entry);

	while (tuple = heap_getnext(scan, false, (Buffer *)NULL),
	       HeapTupleIsValid(tuple)) {

	    AccessMethodOperatorTupleForm form;

	    form = (AccessMethodOperatorTupleForm) HeapTupleGetForm(tuple);

	    OperatorRelationFillScanKeyEntry(operatorRelation,
					     form->amopoprid,
		          StrategyMapGetScanKeyEntry(map, form->amopstrategy));
	}

	heap_endscan(scan);
    }

    heap_close(operatorRelation);
    heap_close(relation);
}

/* ----------------
 *	IndexStrategyDisplay
 * ----------------
 */
#ifdef	ISTRATDEBUG
int
IndexStrategyDisplay(indexStrategy, numberOfStrategies, numberOfAttributes)
    IndexStrategy	indexStrategy;
    StrategyNumber	numberOfStrategies;
    AttributeNumber	numberOfAttributes;
{
    StrategyMap	strategyMap;
    AttributeNumber	attributeNumber;
    StrategyNumber	strategyNumber;

    for (attributeNumber = 1; attributeNumber <= numberOfAttributes;
	 attributeNumber += 1) {

	strategyMap = IndexStrategyGetStrategyMap(indexStrategy,
						  numberOfStrategies,
						  attributeNumber);

	for (strategyNumber = 1;
	     strategyNumber <= AMStrategies(numberOfStrategies);
	     strategyNumber += 1) {

	    printf(":att %d\t:str %d\t:opr 0x%x(%d)\n",
		   attributeNumber, strategyNumber,
		   strategyMap->entry[strategyNumber - 1].procedure,
		   strategyMap->entry[strategyNumber - 1].procedure);
	}
    }
}
#endif	/* defined(ISTRATDEBUG) */

