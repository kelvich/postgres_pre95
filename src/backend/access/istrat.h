/*
 * istrat.h --
 *	POSTGRES index strategy definitions.
 */

#ifndef	IStratIncluded		/* Include this file only once */
#define IStratIncluded	1

/*
 * Identification:
 */
#define ISTRAT_H	"$Header$"

#include "tmp/postgres.h"
#include "access/attnum.h"
#include "access/skey.h"

typedef uint16	StrategyNumber;

#define InvalidStrategy	0

#ifndef	CorrectStrategies		/* XXX this should be removable */
#define AMStrategies(foo)	12
#else	/* !defined(CorrectStrategies) */
#define AMStrategies(foo)	(foo)
#endif	/* !defined(CorrectStrategies) */

typedef struct StrategyMapData {
	ScanKeyEntryData	entry[1];	/* VARIABLE LENGTH ARRAY */
} StrategyMapData;	/* VARIABLE LENGTH STRUCTURE */

typedef StrategyMapData	*StrategyMap;

typedef struct IndexStrategyData {
	StrategyMapData	strategyMapData[1];	/* VARIABLE LENGTH ARRAY */
} IndexStrategyData;	/* VARIABLE LENGTH STRUCTURE */

typedef IndexStrategyData	*IndexStrategy;

/*
 * StrategyNumberIsValid --
 *	True iff the strategy number is valid.
 */
#define StrategyNumberIsValid(strategyNumber) \
    ((bool) ((strategyNumber) != InvalidStrategy))

/*
 * StrategyNumberIsInBounds --
 *	True iff strategy number is within given bounds.
 *
 * Note:
 *	Assumes StrategyNumber is an unsigned type.
 *	Assumes the bounded interval to be (0,max].
 */
#define StrategyNumberIsInBounds(strategyNumber, maxStrategyNumber) \
    ((bool)(InvalidStrategy < (strategyNumber) && \
	    (strategyNumber) <= (maxStrategyNumber)))

/*
 * StrategyMapIsValid --
 *	True iff the index strategy mapping is valid.
 */
#define	StrategyMapIsValid(map) PointerIsValid(map)

/*
 * IndexStrategyIsValid --
 *	True iff the index strategy is valid.
 */
#define	IndexStrategyIsValid(s)	PointerIsValid(s)

/*
 * StrategyMapGetScanKeyEntry --
 *	Returns a scan key entry of a index strategy mapping member.
 *
 * Note:
 *	Assumes that the index strategy mapping is valid.
 *	Assumes that the index strategy number is valid.
 *	Bounds checking should be done outside this routine.
 */
extern
ScanKeyEntry
StrategyMapGetScanKeyEntry ARGS((
	StrategyMap	map,
	StrategyNumber	strategyNumber
));

/*
 * IndexStrategyGetStrategyMap --
 *	Returns an index strategy mapping of an index strategy.
 *
 * Note:
 *	Assumes that the index strategy is valid.
 *	Assumes that the number of index strategies is valid.
 *	Bounds checking should be done outside this routine.
 */
extern
StrategyMap
IndexStrategyGetStrategyMap ARGS((
	IndexStrategy	indexStrategy,
	StrategyNumber	maxStrategyNumber,
	AttributeNumber	attributeNumber
));

/*
 * AttributeNumberGetIndexStrategySize --
 *	Computes the size of an index strategy.
 */
extern
Size
AttributeNumberGetIndexStrategySize ARGS((
	AttributeNumber	maxAttributeNumber,
	StrategyNumber	maxStrategyNumber
));

/*
 * IndexSupportInitialize --
 *	Initializes an index strategy and associated support procedures.
 */
extern
void
IndexSupportInitialize ARGS((
	IndexStrategy	indexStrategy,
	RegProcedure	*indexSupport,
	ObjectId	indexObjectId,
	ObjectId	accessMethodObjectId,
	StrategyNumber	maxStrategyNumber
	StrategyNumber	maxSupportNumber
));

#endif	/* !defined(IStratIncluded) */
