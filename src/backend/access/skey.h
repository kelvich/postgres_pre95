/*
 * skey.h --
 *	POSTGRES scan key definitions.
 *
 * Note:
 *	Needs more accessor/assignment routines.
 *
 * Identification:
 *	$Header$
 */

#ifndef	SKey	/* Include this file only once. */
#define SKey	1

#include "c.h"

#include "attnum.h"
#include "datum.h"
#include "regproc.h"

typedef uint16	ScanKeySize;

typedef struct ScanKeyEntryData {
	bits16		flags;
	AttributeNumber	attributeNumber;
	RegProcedure	procedure;
	Datum		argument;
} ScanKeyEntryData;

#define CheckIfNull		0x1
#define UnaryProcedure		0x2
#define NegateResult		0x4
#define CommuteArguments	0x8

typedef ScanKeyEntryData	*ScanKeyEntry;

typedef struct ScanKeyData {
	ScanKeyEntryData	data[1];	/* VARIABLE LENGTH ARRAY */
} ScanKeyData;

typedef ScanKeyData	*ScanKey;

/*
 * ScanKeyIsValid --
 *	True iff the scan key is valid.
 */
extern
bool
ScanKeyIsValid ARGS((
	ScanKey	key
));

/*
 * ScanKeyEntryIsValid --
 *	True iff the scan key entry is valid.
 */
extern
bool
ScanKeyEntryIsValid ARGS((
	ScanKeyEntry	entry
));

/*
 * ScanKeyEntryIsLegal --
 *	True iff the scan key entry is legal.
 */
extern
bool
ScanKeyEntryIsLegal ARGS((
	ScanKeyEntry	entry
));

/*
 * ScanKeyEntrySetIllegal --
 *	Marks a scan key entry as illegal.
 */
extern
void
ScanKeyEntrySetIllegal ARGS((
	ScanKeyEntry	entry
));

/*
 * ScanKeyEntryInitialize --
 *	Initializes an scan key entry.
 *
 * Note:
 *	Assumes the scan key entry is valid.
 *	Assumes the intialized scan key entry will be legal.
 */
extern
void
ScanKeyEntryInitialize ARGS((
	ScanKeyEntry	entry,
	bits16		flags,
	AttributeNumber	attributeNumber,
	RegProcedure	procedure,
	Datum		argument
));

#endif	/* !defined(SKey) */
