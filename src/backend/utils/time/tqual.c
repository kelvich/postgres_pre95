
/*
 * 
 * POSTGRES Data Base Management System
 * 
 * Copyright (c) 1988 Regents of the University of California
 * 
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for educational, research, and non-profit purposes and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of the University of California not be used in advertising
 * or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Permission to incorporate this
 * software into commercial products can be obtained from the Campus
 * Software Office, 295 Evans Hall, University of California, Berkeley,
 * Ca., 94720 provided only that the the requestor give the University
 * of California a free licence to any derived software for educational
 * and research purposes.  The University of California makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 * 
 */



/*
 * tqual.c --
 *	POSTGRES time qualification code.
 */

/* #define TQUALDEBUG	1 */

#include "c.h"

#include "htup.h"
#include "log.h"
#include "tim.h"
#include "xcxt.h"
#include "xid.h"
#include "xlog.h"

#include "tqual.h"

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

/*
 * XXX Transaction system override hacks start here
 */
#ifndef	GOODAMI

static TransactionIdData	HeapSpecialTransactionIdData;

static TransactionId	HeapSpecialTransactionId = InvalidTransactionId;
static CommandId	HeapSpecialCommandId = FirstCommandId;

void
setheapoverride(on)
	bool	on;
{
	if (on) {
		HeapSpecialTransactionId = &HeapSpecialTransactionIdData;
		TransactionIdStore(GetCurrentTransactionId(),
			(Pointer)HeapSpecialTransactionId);
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
		return (AbsoluteTimeIsBefore(((InternalTimeQual)qual)->start,
			GetCurrentTransactionStartTime()));
	}

	/* TimeQualOlder or TimeQualRange */
	if (((InternalTimeQual)qual)->mode & TimeQualOlder) {
		return (AbsoluteTimeIsBefore(((InternalTimeQual)qual)->end,
			GetCurrentTransactionStartTime()));
	}

	/* TimeQualNewer */
	if (((InternalTimeQual)qual)->mode & TimeQualNewer) {
		return (AbsoluteTimeIsBefore(((InternalTimeQual)qual)->start,
			GetCurrentTransactionStartTime()));
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
			AbsoluteTimeIsBefore(
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

	qual->mode = TimeQualEvery;

	if (TimeIsValid(startTime)) {
		qual->start = startTime;
		qual->mode |= TimeQualNewer;
	}
	if (TimeIsValid(endTime)) {
		qual->end = endTime;
		qual->mode |= TimeQualOlder;
	}

	return ((TimeQual)qual);
}

/* This does not make any sense
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
*/

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
	if (!AbsoluteTimeIsValid(tuple->t_tmin)) {
		if (TransactionIdIsCurrentTransactionId(tuple->t_xmin) &&
				!TransactionIdIsValid(tuple->t_xmax)) {
			return (true);
		}

		if (!TransactionIdDidCommit(tuple->t_xmin)) {
			return (false);
		}
	}
	/* the tuple was inserted validly */

	if (AbsoluteTimeIsValid(tuple->t_tmax)) {
		return (false);
	}

	if (!TransactionIdIsValid(tuple->t_xmax)) {
		return (true);
	}

	if (TransactionIdIsCurrentTransactionId(tuple->t_xmax)) {
		return (false);
	}

	return ((bool)!TransactionIdDidCommit(tuple->t_xmax));
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
	if (!AbsoluteTimeIsValid(tuple->t_tmin)) {

#ifndef	ELOGTIME
		/* XXX this should be removable with anything breaking */
		/* system bootstrapping might break without this */
		if (TransactionIdIsCurrentTransactionId(tuple->t_xmin) &&
				CommandIdIsCurrentCommandId(tuple->t_cmin)) {
			return (false);
		}
#endif	/* !defined(ELOGTIME) */

		if (TransactionIdIsCurrentTransactionId(tuple->t_xmin) &&
				!CommandIdIsCurrentCommandId(tuple->t_cmin)) {

			if (!TransactionIdIsValid(tuple->t_xmax)) {
				return (true);
			}

			Assert(TransactionIdIsCurrentTransactionId(
				tuple->t_xmax));

			if (CommandIdIsCurrentCommandId(tuple->t_cmax)) {
				return (true);
			}
		}

		if (!TransactionIdDidCommit(tuple->t_xmin)) {
			return (false);
		}
	}
	/* the tuple was inserted validly */

	if (AbsoluteTimeIsValid(tuple->t_tmax)) {
		return (false);
	}

	if (!TransactionIdIsValid(tuple->t_xmax)) {
		return (true);
	}

	if (TransactionIdIsCurrentTransactionId(tuple->t_xmax)) {
		return (CommandIdIsCurrentCommandId(tuple->t_cmin));
	}

	return ((bool)!TransactionIdDidCommit(tuple->t_xmax));
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
	if (!AbsoluteTimeIsValid(tuple->t_tmin)) {

		if (!TransactionIdDidCommit(tuple->t_xmin)) {
			return (false);
		}

		tuple->t_tmin = TransactionIdGetCommitTime(tuple->t_xmin);
	}

	if (TimeIsBefore(TimeQualGetSnapshotTime(qual), tuple->t_tmin)) {
		return (false);
	}
	/* the tuple was inserted validly before the snapshot time */

	if (!AbsoluteTimeIsValid(tuple->t_tmax)) {

		if (!TransactionIdIsValid(tuple->t_xmax) ||
				!TransactionIdDidCommit(tuple->t_xmax)) {

			return (true);
		}

		tuple->t_tmax = TransactionIdGetCommitTime(tuple->t_xmax);
	}

	return ((bool)
		!TimeIsBefore(tuple->t_tmax, TimeQualGetSnapshotTime(qual)));
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
	if (!AbsoluteTimeIsValid(tuple->t_tmin)) {

		if (!TransactionIdDidCommit(tuple->t_xmin)) {
			return (false);
		}

		tuple->t_tmin = TransactionIdGetCommitTime(tuple->t_xmin);
	}

	if (TimeIsBefore(TimeQualGetEndTime(qual), tuple->t_tmin)) {
		return (false);
	}
	/* the tuple was inserted validly before the range end */

	if (!AbsoluteTimeIsValid(TimeQualGetStartTime(qual))) {
		return (true);
	}

	if (!AbsoluteTimeIsValid(tuple->t_tmax)) {

		if (!TransactionIdIsValid(tuple->t_xmax) ||
				!TransactionIdDidCommit(tuple->t_xmax)) {

			return (true);
		}

		tuple->t_tmax = TransactionIdGetCommitTime(tuple->t_xmax);
	}

	return ((bool)!TimeIsBefore(tuple->t_tmax, TimeQualGetStartTime(qual)));
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

#ifndef	ELOGTIME
		/* XXX this should be removable with anything breaking */
		/* system bootstrapping might break without this */
		if (TransactionIdIsCurrentTransactionId(tuple->t_xmin) &&
				CommandIdIsCurrentCommandId(tuple->t_cmin)) {
			return (false);
		}
#endif	/* !defined(ELOGTIME) */

		if (TransactionIdIsCurrentTransactionId(tuple->t_xmin) &&
				!CommandIdIsCurrentCommandId(tuple->t_cmin)) {

			if (!TransactionIdIsValid(tuple->t_xmax)) {
				return (true);
			}

			Assert(TransactionIdIsCurrentTransactionId(
				tuple->t_xmax));

			if (CommandIdIsCurrentCommandId(tuple->t_cmax)) {
				return (true);
			}
		}

		if (!TransactionIdDidCommit(tuple->t_xmin)) {
			return (false);
		}

		tuple->t_tmin = TransactionIdGetCommitTime(tuple->t_xmin);
	}
	/* the tuple was inserted validly */

	if (!AbsoluteTimeIsValid(TimeQualGetStartTime(qual))) {
		return (true);
	}

	if (!AbsoluteTimeIsValid(tuple->t_tmax)) {

		if (!TransactionIdIsValid(tuple->t_xmax)) {
			return (true);
		}

		if (TransactionIdIsCurrentTransactionId(tuple->t_xmax)) {
			return (CommandIdIsCurrentCommandId(tuple->t_cmin));
		}

		if (!TransactionIdDidCommit(tuple->t_xmax)) {
			return (true);
		}

		tuple->t_tmax = TransactionIdGetCommitTime(tuple->t_xmax);
	}

	return ((bool)!TimeIsBefore(tuple->t_tmax, TimeQualGetStartTime(qual)));
}
