
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
 * istrat.h --
 *	POSTGRES index strategy definitions.
 *
 * Identification:
 *	$Header$
 */

#ifndef	IStratIncluded	/* Include this file only once. */
#define IStratIncluded	1

#include "c.h"

#include "attnum.h"
#include "skey.h"
#include "oid.h"

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
extern
bool
StrategyNumberIsValid ARGS((
	StrategyNumber	strategyNumber
));

/*
 * StrategyNumberIsInBounds --
 *	True iff strategy number is within given bounds.
 *
 * Note:
 *	Assumes StrategyNumber is an unsigned type.
 *	Assumes the bounded interval to be (0,max].
 */
extern
bool
StrategyNumberIsInBounds ARGS((
	StrategyNumber	strategyNumber,
	StrategyNumber	maxStrategyNumber
));

/*
 * StrategyMapIsValid --
 *	True iff the index strategy mapping is valid.
 */
extern
bool
StrategyMapIsValid ARGS((
	StrategyMap	map
));

/*
 * IndexStrategyIsValid --
 *	True iff the index strategy is valid.
 */
extern
bool
IndexStrategyIsValid ARGS((
	IndexStrategy	indexStrategy
));

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
 * IndexStrategyInitialize --
 *	Initializes an index strategy.
 */
extern
void
IndexStrategyInitialize ARGS((
	IndexStrategy	indexStrategy,
	ObjectId	indexObjectId,
	ObjectId	accessMethodObjectId,
	StrategyNumber	maxStrategyNumber
));

#endif	/* !defined(IStratIncluded) */
