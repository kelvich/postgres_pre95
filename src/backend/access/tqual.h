/*
 * tqual.h --
 *	POSTGRES time qualification definitions.
 *
 * Note:
 *	It may be desirable to allow time qualifications to indicate
 *	relative times.
 */

#ifndef	TQualIncluded		/* Include this file only once */
#define TQualIncluded	1

/*
 * Identification:
 */
#define TQUAL_H	"$Header$"

#ifndef C_H
#include "c.h"
#endif

#ifndef	HTUP_H
# include "htup.h"
#endif
#ifndef	TIM_H
# include "tim.h"
#endif

typedef struct TimeQualSpace {
	char	data[12];
} TimeQualSpace;

typedef Pointer	TimeQual;

/* Tuples valid as of StartTransactionCommand */
#define	NowTimeQual	((TimeQual) NULL)

/* As above, plus updates in this command */
extern TimeQual	SelfTimeQual;

#ifndef	GOODAMI
#define LispSelfTimeQual	((TimeQual) 1)
#endif	/* !defined(GOODAMI) */

/*
 * TimeQualIsValid --
 *	True iff time qualification is valid.
 */
extern
bool
TimeQualIsValid ARGS((
	TimeQual	qual
));

/*
 * TimeQualIsLegal --
 *	True iff time qualification is legal.
 *	I.e., true iff time qualification does not intersects the future,
 *	relative to the transaction start time.
 *
 * Note:
 *	Assumes time qualification is valid.
 */
extern
bool
TimeQualIsLegal ARGS((
	TimeQual	qual
));

/*
 * TimeQualIncludesNow --
 *	True iff time qualification includes "now."
 *
 * Note:
 *	Assumes time qualification is valid.
 */
extern
bool
TimeQualIncludesNow ARGS((
	TimeQual	qual
));

/*
 * TimeQualIncludesPast --
 *	True iff time qualification includes some time in the past.
 *
 * Note:
 *	Assumes time qualification is valid.
 *	XXX may not be needed?
 */
extern
bool
TimeQualIncludesPast ARGS((
	TimeQual	qual
));

/*
 * TimeQualIsSnapshot --
 *	True iff time qualification is a snapshot qualification.
 *
 * Note:
 *	Assumes time qualification is valid.
 */
extern
bool
TimeQualIsSnapshot ARGS((
	TimeQual	qual
));

/*
 * TimeQualIsRanged --
 *	True iff time qualification is a ranged qualification.
 *
 * Note:
 *	Assumes time qualification is valid.
 */
extern
bool
TimeQualIsRanged ARGS((
	TimeQual	qual
));

/*
 * TimeQualIndicatesDisableValidityChecking --
 *	True iff time qualification indicates validity checking should be
 *	disabled.
 *
 * Note:
 *	XXX This should not be implemented since this does not make sense.
 */
extern
bool
TimeQualIndicatesDisableValidityChecking ARGS((
	TimeQual	qual
));

/*
 * TimeQualGetSnapshotTime --
 *	Returns time for a snapshot time qual.
 *
 * Note:
 *	Assumes time qual is valid snapshot time qual.
 */
extern
Time
TimeQualGetSnapshotTime ARGS((
	TimeQual	qual
));

/*
 * TimeQualGetStartTime --
 *	Returns start time for a ranged time qual.
 *
 * Note:
 *	Assumes time qual is valid ranged time qual.
 */
extern
Time
TimeQualGetStartTime ARGS((
	TimeQual	qual
));

/*
 * TimeQualEndTime --
 *	Returns end time for a ranged time qual.
 *
 * Note:
 *	Assumes time qual is valid ranged time qual.
 */
extern
Time
TimeQualGetEndTime ARGS((
	TimeQual	qual
));

/*
 * TimeFormSnapshotTimeQual --
 *	Returns snapshot time qual for a time.
 *
 * Note:
 *	Assumes time is valid.
 */
extern
TimeQual
TimeFormSnapshotTimeQual ARGS((
	Time	time
));

/*
 * TimeFormRangedTimeQual --
 *	Returns ranged time qual for a pair of times.
 *
 * Note:
 *	If start time is invalid, it is regarded as the epoch.
 *	If end time is invalid, it is regarded as "now."
 *	Assumes start time is before (or the same as) end time.
 */
extern
TimeQual
TimeFormRangedTimeQual ARGS((
	Time	startTime,
	Time	endTime
));

/*
 * TimeFormDebuggingTimeQual --
 *	Returns debugging snapshot time qual for a time.
 *
 * Note:
 *	Removed since it does not make sense semantically.
 */

/*
 * HeapTupleSatisfiesTimeQual --
 *	True iff heap tuple satsifies a time qual.
 *
 * Note:
 *	Assumes heap tuple is valid.
 *	Assumes time qual is valid.
 */
extern
bool
HeapTupleSatisfiesTimeQual ARGS((	
	HeapTuple	tuple,
	TimeQual	qual
));

#endif	/* !defined(TQualIncluded) */
