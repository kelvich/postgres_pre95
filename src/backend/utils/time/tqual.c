/*
 * tqual.c --
 *	POSTGRES time qualification code.
 */

/* #define TQUALDEBUG	1 */

#include "tmp/postgres.h"

#include "access/htup.h"
#include "access/xcxt.h"
#include "access/xlog.h"
#include "access/xact.h"
#include "access/transam.h"
#include "utils/log.h"
#include "utils/nabstime.h"

#include "access/tqual.h"

RcsId("$Header$");

/*
 * TimeQualMode --
 *	Mode indicator for treatment of time qualifications.
 */
typedef uint16	TimeQualMode;

#define TimeQualAt	0x1
#define TimeQualNewer	0x2
#define TimeQualOlder	0x4
#define TimeQualAll	0x8

#define TimeQualMask	0xf

#define TimeQualEvery	0x0
#define TimeQualRange	(TimeQualNewer | TimeQualOlder)
#define TimeQualAllAt	(TimeQualAt | TimeQualAll)

typedef struct TimeQualData {
	AbsoluteTime	start;
	AbsoluteTime	end;
	TimeQualMode	mode;
} TimeQualData;

typedef TimeQualData	*InternalTimeQual;

static TimeQualData	SelfTimeQualData;
TimeQual		SelfTimeQual = (Pointer)&SelfTimeQualData;

extern bool		PostgresIsInitialized;

/*
 * XXX Transaction system override hacks start here
 */
#ifndef	GOODAMI

static TransactionId	HeapSpecialTransactionId = InvalidTransactionId;
static CommandId	HeapSpecialCommandId = FirstCommandId;

void
setheapoverride(on)
	bool	on;
{
	if (on) {
		TransactionIdStore(GetCurrentTransactionId(),
				   &HeapSpecialTransactionId);
		HeapSpecialCommandId = GetCurrentCommandId();
	} else {
		HeapSpecialTransactionId = InvalidTransactionId;
	}
}

/* static */
bool
heapisoverride()
{
	if (!TransactionIdIsValid(HeapSpecialTransactionId)) {
		return (false);
	}

	if (!TransactionIdEquals(GetCurrentTransactionId(),
			HeapSpecialTransactionId) ||
			GetCurrentCommandId() != HeapSpecialCommandId) {
		HeapSpecialTransactionId = InvalidTransactionId;

		return (false);
	}
	return (true);
}

#endif	/* !defined(GOODAMI) */
/*
 * XXX Transaction system override hacks end here
 */

/*
 * HeapTupleSatisfiesItself --
 *	True iff heap tuple is valid for "itself."
 *
 * Note:
 *	Assumes heap tuple is valid.
 */
static
bool
HeapTupleSatisfiesItself ARGS((
	HeapTuple	tuple
));

/*
 * HeapTupleSatisfiesNow --
 *	True iff heap tuple is valid "now."
 *
 * Note:
 *	Assumes heap tuple is valid.
 */
static
bool
HeapTupleSatisfiesNow ARGS((
	HeapTuple	tuple
));

/*
 * HeapTupleSatisfiesSnapshotInternalTimeQual --
 *	True iff heap tuple is valid at the snapshot time qualification.
 *
 * Note:
 *	Assumes heap tuple is valid.
 *	Assumes internal time qualification is valid snapshot qualification.
 */
static
bool
HeapTupleSatisfiesSnapshotInternalTimeQual ARGS((
	HeapTuple		tuple,
	InternalTimeQual	qual
));

/*
 * HeapTupleSatisfiesUpperBoundedInternalTimeQual --
 *	True iff heap tuple is valid within a upper bounded time qualification.
 *
 * Note:
 *	Assumes heap tuple is valid.
 *	Assumes time qualification is valid ranged qualification with fixed
 *	upper bound.
 */
static
bool
HeapTupleSatisfiesUpperBoundedInternalTimeQual ARGS((
	HeapTuple		tuple,
	InternalTimeQual	qual
));

/*
 * HeapTupleSatisfiesUpperUnboundedInternalTimeQual --
 *	True iff heap tuple is valid within a upper bounded time qualification.
 *
 * Note:
 *	Assumes heap tuple is valid.
 *	Assumes time qualification is valid ranged qualification with no
 *	upper bound.
 */
static
bool
HeapTupleSatisfiesUpperUnboundedInternalTimeQual ARGS((
	HeapTuple		tuple,
	InternalTimeQual	qual
));

bool
TimeQualIsValid(qual)
	TimeQual	qual;
{
	bool	hasStartTime;

	if (!PointerIsValid(qual) || qual == SelfTimeQual) {
		return (true);
	}

#ifndef	GOODAMI
	if (qual == LispSelfTimeQual) {
		return (true);
	}
#endif	/* !defined(GOODAMI) */

	if (((InternalTimeQual)qual)->mode & ~TimeQualMask) {
		return (false);
	}

	if (((InternalTimeQual)qual)->mode & TimeQualAt) {
		return (AbsoluteTimeIsValid(((InternalTimeQual)qual)->start));
	}

	hasStartTime = false;

	if (((InternalTimeQual)qual)->mode & TimeQualNewer) {
		if (!AbsoluteTimeIsValid(((InternalTimeQual)qual)->start)) {
			return (false);
		}
		hasStartTime = true;
	}

	if (((InternalTimeQual)qual)->mode & TimeQualOlder) {
		if (!TimeIsValid(((InternalTimeQual)qual)->end)) {
			return (false);
		}
		if (hasStartTime) {
			return ((bool)!TimeIsBefore(
				((InternalTimeQual)qual)->end,
				((InternalTimeQual)qual)->start));
		}
	}
	return (true);
}

bool
TimeQualIsLegal(qual)
	TimeQual	qual;
{
	Assert(TimeQualIsValid(qual));

	if (qual == NowTimeQual || qual == SelfTimeQual) {
		return (true);
	}

#ifndef	GOODAMI
	if (qual == LispSelfTimeQual) {
		return (true);
	}
#endif	/* !defined(GOODAMI) */

	/* TimeQualAt */
	if (((InternalTimeQual)qual)->mode & TimeQualAt) {
		AbsoluteTime a, b;

		a = ((InternalTimeQual)qual)->start;
		b = GetCurrentTransactionStartTime();

		if (AbsoluteTimeIsAfter(a, b))
			return (false);
		else
			return (true);
	}

	/* TimeQualOlder or TimeQualRange */
	if (((InternalTimeQual)qual)->mode & TimeQualOlder) {
		AbsoluteTime a, b;

		a = ((InternalTimeQual)qual)->end;
		b = GetCurrentTransactionStartTime();

		if (AbsoluteTimeIsAfter(a, b))
			return (false);
		else
			return (true);
	}

	/* TimeQualNewer */
	if (((InternalTimeQual)qual)->mode & TimeQualNewer) {
		AbsoluteTime a, b;

		a = ((InternalTimeQual)qual)->start;
		b = GetCurrentTransactionStartTime();

		if (AbsoluteTimeIsAfter(a, b))
			return (false);
		else
			return (true);
	}

	/* TimeQualEvery */
	return (true);
}

bool
TimeQualIncludesNow(qual)
	TimeQual	qual;
{
	Assert(TimeQualIsValid(qual));

	if (qual == NowTimeQual || qual == SelfTimeQual) {
		return (true);
	}

#ifndef	GOODAMI
	if (qual == LispSelfTimeQual) {
		return (false);
	}
#endif	/* !defined(GOODAMI) */

	if (((InternalTimeQual)qual)->mode & TimeQualAt) {
		return (false);
	}
	if (((InternalTimeQual)qual)->mode & TimeQualOlder &&
			!AbsoluteTimeIsAfter(
				((InternalTimeQual)qual)->end,
				GetCurrentTransactionStartTime())) {

		return (false);
	}
	return (true);
}

bool
TimeQualIncludesPast(qual)
	TimeQual	qual;
{
	Assert(TimeQualIsValid(qual));

	if (qual == NowTimeQual || qual == SelfTimeQual) {
		return (false);
	}

#ifndef	GOODAMI
	if (qual == LispSelfTimeQual) {
		return (false);
	}
#endif	/* !defined(GOODAMI) */

	/* otherwise, must check archive (setting locks as appropriate) */
	return (true);
}

bool
TimeQualIsSnapshot(qual)
	TimeQual	qual;
{
	Assert(TimeQualIsValid(qual));

	if (qual == NowTimeQual || qual == SelfTimeQual) {
		return (false);
	}

#ifndef	GOODAMI
	if (qual == LispSelfTimeQual) {
		return (false);
	}
#endif	/* !defined(GOODAMI) */

	return ((bool)!!(((InternalTimeQual)qual)->mode & TimeQualAt));
}

bool
TimeQualIsRanged(qual)
	TimeQual	qual;
{
	Assert(TimeQualIsValid(qual));

	if (qual == NowTimeQual || qual == SelfTimeQual) {
		return (false);
	}

#ifndef	GOODAMI
	if (qual == LispSelfTimeQual) {
		return (false);
	}
#endif	/* !defined(GOODAMI) */

	return ((bool)!(((InternalTimeQual)qual)->mode & TimeQualAt));
}

bool
TimeQualIndicatesDisableValidityChecking(qual)
	TimeQual	qual;
{
	Assert (TimeQualIsValid(qual));

	if (qual == NowTimeQual || qual == SelfTimeQual) {
		return (false);
	}

#ifndef	GOODAMI
	if (qual == LispSelfTimeQual) {
		return (false);
	}
#endif	/* !defined(GOODAMI) */

	if (((InternalTimeQual)qual)->mode & TimeQualAll) {
		return (true);
	}
	return (false);
}

Time
TimeQualGetSnapshotTime(qual)
	TimeQual	qual;
{
	Assert(TimeQualIsSnapshot(qual));

	return (((InternalTimeQual)qual)->start);
}

Time
TimeQualGetStartTime(qual)
	TimeQual	qual;
{
	Assert(TimeQualIsRanged(qual));

	return (((InternalTimeQual)qual)->start);
}

Time
TimeQualGetEndTime(qual)
	TimeQual	qual;
{
	Assert(TimeQualIsRanged(qual));

	return (((InternalTimeQual)qual)->end);
}

TimeQual
TimeFormSnapshotTimeQual(time)
	AbsoluteTime	time;
{
	InternalTimeQual	qual;

	Assert(TimeIsValid(time));

	qual = LintCast(InternalTimeQual, palloc(sizeof *qual));

	qual->start = time;
	qual->end = InvalidAbsoluteTime;
	qual->mode = TimeQualAt;

	return ((TimeQual)qual);
}

TimeQual
TimeFormRangedTimeQual(startTime, endTime)
	AbsoluteTime	startTime;
	AbsoluteTime	endTime;
{
	InternalTimeQual	qual;

	qual = LintCast(InternalTimeQual, palloc(sizeof *qual));

	qual->start = startTime;
	qual->end = endTime;
	qual->mode = TimeQualEvery;

	if (TimeIsValid(startTime)) {
		qual->mode |= TimeQualNewer;
	}
	if (TimeIsValid(endTime)) {
		qual->mode |= TimeQualOlder;
	}

	return ((TimeQual)qual);
}

#if 0
TimeQual
TimeFormDebuggingTimeQual(time)
	Time	time;
{
	InternalTimeQual	qual;

	Assert(TimeIsValid(time));

	qual = LintCast(InternalTimeQual, palloc(sizeof *qual));

	qual->start = time;
	qual->mode = TimeQualAllAt;

	return ((TimeQual)qual);
}
#endif 0

/*
 * Note:
 *	XXX Many of the checks may be simplified and still remain correct.
 *	XXX Partial answers to the checks may be cached in an ItemId.
 */
bool
HeapTupleSatisfiesTimeQual(tuple, qual)
	HeapTuple	tuple;
	TimeQual	qual;
{
	Assert(HeapTupleIsValid(tuple));
	Assert(TimeQualIsValid(qual));

	if (qual == SelfTimeQual || heapisoverride()
#ifndef	GOODAMI
			|| qual == LispSelfTimeQual
#endif	/* !defined(GOODAMI) */
			) {
		return (HeapTupleSatisfiesItself(tuple));
	}

	if (qual == NowTimeQual) {
		return (HeapTupleSatisfiesNow(tuple));
	}

	if (!TimeQualIsLegal(qual)) {
		elog(WARN, "HeapTupleSatisfiesTimeQual: illegal time qual");
	}

	if (TimeQualIndicatesDisableValidityChecking(qual)) {
		elog(WARN, "HeapTupleSatisfiesTimeQual: no disabled validity checking (yet)");
	}

	if (TimeQualIsSnapshot(qual)) {
		return (HeapTupleSatisfiesSnapshotInternalTimeQual(tuple,
			(InternalTimeQual)qual));
	}

	if (TimeQualIncludesNow(qual)) {
		return (HeapTupleSatisfiesUpperUnboundedInternalTimeQual(tuple,
			(InternalTimeQual)qual));
	}

	return (HeapTupleSatisfiesUpperBoundedInternalTimeQual(tuple,
		(InternalTimeQual)qual));
}

/*
 * The satisfaction of "itself" requires the following:
 *
 * ((Xmin == my-transaction && (Xmax is null [|| Xmax != my-transaction)])
 * ||
 *
 * (Xmin is committed &&
 *	(Xmax is null || (Xmax != my-transaction && Xmax is not committed)))
 */
static
bool
HeapTupleSatisfiesItself(tuple)
	HeapTuple	tuple;
{
    /*
     * XXX Several evil casts are made in this routine.  Casting XID to be 
     * TransactionId works only because TransactionId->data is the first
     * (and only) field of the structure.
     */
    if (!AbsoluteTimeIsValid(tuple->t_tmin)) {
	if (TransactionIdIsCurrentTransactionId((TransactionId)tuple->t_xmin) &&
	    !TransactionIdIsValid((TransactionId)tuple->t_xmax)) {
			return (true);
	}

	if (!TransactionIdDidCommit((TransactionId)tuple->t_xmin)) {
			return (false);
	}
    }
    /* the tuple was inserted validly */

    if (AbsoluteTimeIsReal(tuple->t_tmax)) {
	return (false);
    }

    if (!TransactionIdIsValid((TransactionId)tuple->t_xmax)) {
	return (true);
    }

    if (TransactionIdIsCurrentTransactionId((TransactionId)tuple->t_xmax)) {
	return (false);
    }

    return ((bool)!TransactionIdDidCommit((TransactionId)tuple->t_xmax));
}

/*
 * The satisfaction of "now" requires the following:
 *
 * ((Xmin == my-transaction && Cmin != my-command &&
 *	(Xmax is null || (Xmax == my-transaction && Cmax != my-command)))
 * ||
 *
 * (Xmin is committed &&
 *	(Xmax is null || (Xmax == my-transaction && Cmax == my-command) ||
 *		(Xmax is not committed && Xmax != my-transaction))))
 */
static
bool
HeapTupleSatisfiesNow(tuple)
	HeapTuple	tuple;
{
    AbsoluteTime curtime;

    if (AMI_OVERRIDE)
	return true;
    /*
     *  If the transaction system isn't yet initialized, then we assume
     *  that transactions committed.  We only look at system catalogs
     *  during startup, so this is less awful than it seems, but it's
     *  still pretty awful.
     */

    if (!PostgresIsInitialized)
	return ((bool)(TransactionIdIsValid((TransactionId)tuple->t_xmin) &&
		       !TransactionIdIsValid((TransactionId)tuple->t_xmax)));

    /*
     * XXX Several evil casts are made in this routine.  Casting XID to be 
     * TransactionId works only because TransactionId->data is the first
     * (and only) field of the structure.
     */
    if (!AbsoluteTimeIsValid(tuple->t_tmin)) {

	if (TransactionIdIsCurrentTransactionId((TransactionId)tuple->t_xmin)
	    && CommandIdIsCurrentCommandId(tuple->t_cmin)) {

	    return (false);
	}

	if (TransactionIdIsCurrentTransactionId((TransactionId)tuple->t_xmin)
	    && !CommandIdIsCurrentCommandId(tuple->t_cmin)) {

	    if (!TransactionIdIsValid((TransactionId)tuple->t_xmax)) {
		return (true);
	    }

	    Assert(TransactionIdIsCurrentTransactionId((TransactionId)tuple->t_xmax));

	    if (CommandIdIsCurrentCommandId(tuple->t_cmax)) {
		return (true);
	    }
	}

	/*
	 * this call is VERY expensive - requires a log table lookup.
	 */

	if (!TransactionIdDidCommit((TransactionId)tuple->t_xmin)) {
	    return (false);
	}

	tuple->t_tmin = TransactionIdGetCommitTime(tuple->t_xmin);
    }

    curtime = GetCurrentTransactionStartTime();

    if (AbsoluteTimeIsAfter(tuple->t_tmin, curtime)) {
	return (false);
    }

    /* we can see the insert */
    if (AbsoluteTimeIsReal(tuple->t_tmax)) {
	if (AbsoluteTimeIsAfter(tuple->t_tmax, curtime))
	    return (true);

	return (false);
    }

    if (!TransactionIdIsValid((TransactionId)tuple->t_xmax)) {
	return (true);
    }

    if (TransactionIdIsCurrentTransactionId((TransactionId)tuple->t_xmax)) {
	return (false);
    }

    if (!TransactionIdDidCommit((TransactionId)tuple->t_xmax)) {
	return (true);
    }

    tuple->t_tmax = TransactionIdGetCommitTime(tuple->t_xmax);

    if (AbsoluteTimeIsAfter(tuple->t_tmax, curtime))
	return (true);

    return (false);
}

/*
 * The satisfaction of Rel[T] requires the following:
 *
 * (Xmin is committed && Tmin <= T &&
 *	(Xmax is null || (Xmax is not committed && Xmax != my-transaction) ||
 *		Tmax >= T))
 */
static
bool
HeapTupleSatisfiesSnapshotInternalTimeQual(tuple, qual)
	HeapTuple		tuple;
	InternalTimeQual	qual;
{
    /*
     * XXX Several evil casts are made in this routine.  Casting XID to be 
     * TransactionId works only because TransactionId->data is the first
     * (and only) field of the structure.
     */
    if (!AbsoluteTimeIsValid(tuple->t_tmin)) {

	if (!TransactionIdDidCommit((TransactionId)tuple->t_xmin)) {
	    return (false);
	}

	tuple->t_tmin = TransactionIdGetCommitTime(tuple->t_xmin);
    }

    if (TimeIsBefore(TimeQualGetSnapshotTime((TimeQual)qual), tuple->t_tmin)) {
	return (false);
    }
    /* the tuple was inserted validly before the snapshot time */

    if (!AbsoluteTimeIsReal(tuple->t_tmax)) {

	if (!TransactionIdIsValid((TransactionId)tuple->t_xmax) ||
	    !TransactionIdDidCommit((TransactionId)tuple->t_xmax)) {

	    return (true);
	}

	tuple->t_tmax = TransactionIdGetCommitTime(tuple->t_xmax);
    }

    return ((bool)
	AbsoluteTimeIsAfter(tuple->t_tmax,
			    TimeQualGetSnapshotTime((TimeQual)qual)));
}

/*
 * The satisfaction of [T1,T2] requires the following:
 *
 * (Xmin is committed && Tmin <= T2 &&
 *	(Xmax is null || (Xmax is not committed && Xmax != my-transaction) ||
 *		T1 is null || Tmax >= T1))
 */
static
bool
HeapTupleSatisfiesUpperBoundedInternalTimeQual(tuple, qual)
	HeapTuple		tuple;
	InternalTimeQual	qual;
{
    /*
     * XXX Several evil casts are made in this routine.  Casting XID to be 
     * TransactionId works only because TransactionId->data is the first
     * (and only) field of the structure.
     */
    if (!AbsoluteTimeIsValid(tuple->t_tmin)) {

	if (!TransactionIdDidCommit((TransactionId)tuple->t_xmin)) {
	    return (false);
	}

	tuple->t_tmin = TransactionIdGetCommitTime(tuple->t_xmin);
    }

    if (TimeIsBefore(TimeQualGetEndTime((TimeQual)qual), tuple->t_tmin)) {
	return (false);
    }
    /* the tuple was inserted validly before the range end */

    if (!AbsoluteTimeIsValid(TimeQualGetStartTime((TimeQual)qual))) {
	return (true);
    }

    if (!AbsoluteTimeIsReal(tuple->t_tmax)) {

	if (!TransactionIdIsValid((TransactionId)tuple->t_xmax) ||
	    !TransactionIdDidCommit((TransactionId)tuple->t_xmax)) {

	    return (true);
	}

	tuple->t_tmax = TransactionIdGetCommitTime(tuple->t_xmax);
    }

    return ((bool)AbsoluteTimeIsAfter(tuple->t_tmax,
				      TimeQualGetStartTime((TimeQual)qual)));
}

/*
 * The satisfaction of [T1,] requires the following:
 *
 * ((Xmin == my-transaction && Cmin != my-command &&
 *	(Xmax is null || (Xmax == my-transaction && Cmax != my-command)))
 * ||
 *
 * (Xmin is committed &&
 *	(Xmax is null || (Xmax == my-transaction && Cmax == my-command) ||
 *		(Xmax is not committed && Xmax != my-transaction) ||
 *		T1 is null || Tmax >= T1)))
 */
static
bool
HeapTupleSatisfiesUpperUnboundedInternalTimeQual(tuple, qual)
	HeapTuple		tuple;
	InternalTimeQual	qual;
{
    if (!AbsoluteTimeIsValid(tuple->t_tmin)) {

	if (TransactionIdIsCurrentTransactionId((TransactionId)tuple->t_xmin) &&
	    CommandIdIsCurrentCommandId(tuple->t_cmin)) {

	    return (false);
	}

	if (TransactionIdIsCurrentTransactionId((TransactionId)tuple->t_xmin) &&
	    !CommandIdIsCurrentCommandId(tuple->t_cmin)) {

	    if (!TransactionIdIsValid((TransactionId)tuple->t_xmax)) {
		return (true);
	    }

	    Assert(TransactionIdIsCurrentTransactionId((TransactionId)tuple->t_xmax));

	    return ((bool) !CommandIdIsCurrentCommandId(tuple->t_cmax));
	}

	if (!TransactionIdDidCommit((TransactionId)tuple->t_xmin)) {
	    return (false);
	}

	tuple->t_tmin = TransactionIdGetCommitTime(tuple->t_xmin);
    }
    /* the tuple was inserted validly */

    if (!AbsoluteTimeIsValid(TimeQualGetStartTime((TimeQual)qual))) {
	return (true);
    }

    if (!AbsoluteTimeIsReal(tuple->t_tmax)) {

	if (!TransactionIdIsValid((TransactionId)tuple->t_xmax)) {
	    return (true);
	}

	if (TransactionIdIsCurrentTransactionId((TransactionId)tuple->t_xmax)) {
	    return (CommandIdIsCurrentCommandId(tuple->t_cmin));
	}

	if (!TransactionIdDidCommit((TransactionId)tuple->t_xmax)) {
	    return (true);
	}

	tuple->t_tmax = TransactionIdGetCommitTime(tuple->t_xmax);
    }

    return ((bool)AbsoluteTimeIsAfter(tuple->t_tmax,
				      TimeQualGetStartTime((TimeQual)qual)));
}
