/*
 * skey.h --
 *	POSTGRES scan key definitions.
 *
 * Note:
 *	Needs more accessor/assignment routines.
 */

#ifndef	SKeyIncluded		/* Include this file only once */
#define SKeyIncluded	1

/*
 * Identification:
 */
#define SKEY_H	"$Header$"

#ifndef POSTGRES_H
#include "postgres.h"
#endif
#ifndef	ATTNUM_H
# include "attnum.h"
#endif
#ifndef	DATUM_H
# include "datum.h"
#endif
#ifndef	REGPROC_H
# include "regproc.h"
#endif

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

#define ScanUnmarked		0x01
#define ScanUncheckedPrevious	0x02
#define ScanUncheckedNext	0x04

typedef ScanKeyEntryData	*ScanKeyEntry;

typedef struct ScanKeyData {
	ScanKeyEntryData	data[1];	/* VARIABLE LENGTH ARRAY */
} ScanKeyData;

typedef ScanKeyData	*ScanKey;

/* ----------------
 *	ScanKeyPtr is used in the executor to keep track of
 *	an array of ScanKey's.  This is needed because a single
 *	scan may use several indices and each index has its own
 *	ScanKey. -cim 9/11/89
 * ----------------
 */
typedef ScanKey		*ScanKeyPtr;

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

/* ----------------------------------------------------------------
 *		    old access.h definitions
 * ----------------------------------------------------------------
 */
struct	skey {
	int16	sk_flags;	/* flags */
	int16	sk_attnum;	/* domain number */
	OID	sk_opr;		/* procedure OID */
	DATUM	sk_data;	/* data to compare */
};

#define	SK_ISNULL	01
#define	SK_UNARY	02
#define	SK_NEGATE	04
#define	SK_COMMUTE	010

#endif	/* !defined(SKeyIncluded) */
