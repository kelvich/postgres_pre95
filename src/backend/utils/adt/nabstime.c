/*
 * getabsdate - parse almost any absolute date getdate(3) can (& some it can't)
 *
 * $Header$
 */

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/timeb.h>
#include "dateconv.h"
#include "datetok.h"

#define MAXDATEFIELDS 25

/* imports */
extern int parsetime();

/* forwards */
int prsabsdate();

/* exports */
extern int dtok_numparsed;

/*
 * parse and convert absolute date in timestr (the normal interface)
 */
time_t
getabsdate(timestr, now)
char *timestr;
struct timeb *now;
{
	int tz = 0;
	struct tm date;

	return prsabsdate(timestr, now, &date, &tz) < 0? -1:
		dateconv(&date, tz);
}

/*
 * just parse the absolute date in timestr and get back a broken-out date.
 */
int
prsabsdate(timestr, now, tm, tzp)
char *timestr;
struct timeb *now;
register struct tm *tm;
int *tzp;
{
	register int nf;
	char *fields[MAXDATEFIELDS];
	static char delims[] = "- \t\n/,";

	nf = split(timestr, fields, MAXDATEFIELDS, delims+1);
	if (nf > MAXDATEFIELDS)
		return -1;
	if (tryabsdate(fields, nf, now, tm, tzp) < 0) {
		register char *p = timestr;

		/*
		 * could be a DEC-date; glue it all back together, split it
		 * with dash as a delimiter and try again.  Yes, this is a
		 * hack, but so are DEC-dates.
		 */
		while (--nf > 0) {
			while (*p++ != '\0')
				;
			p[-1] = ' ';
		}
		nf = split(timestr, fields, MAXDATEFIELDS, delims);
		if (nf > MAXDATEFIELDS)
			return -1;
		if (tryabsdate(fields, nf, now, tm, tzp) < 0)
			return -1;
	}
	return 0;
}

/*
 * try to parse pre-split timestr as an absolute date
 */
int
tryabsdate(fields, nf, now, tm, tzp)
char *fields[];
int nf;
struct timeb *now;
register struct tm *tm;
int *tzp;
{
	register int i;
	register datetkn *tp;
	register long flg = 0, ty;
	int mer = HR24, bigval = -1;
	struct timeb ftz;

	if (now == NULL) {		/* default to local time (zone) */
		now = &ftz;
		(void) ftime(now);
	}
	*tzp = now->timezone;

	tm->tm_mday = tm->tm_mon = tm->tm_year = -1;	/* mandatory */
	tm->tm_hour = tm->tm_min = tm->tm_sec = 0;
	tm->tm_isdst = 0;
	dtok_numparsed = 0;

	for (i = 0; i < nf; i++) {
		if (fields[i][0] == '\0')
			continue;
		tp = datetoktype(fields[i], &bigval);
		ty = (1L << tp->type) & ~(1L << IGNORE);
		if (flg&ty)
			return -1;		/* repeated type */
		flg |= ty;
		switch (tp->type) {
		case YEAR:
			tm->tm_year = bigval;
			break;
		case DAY:
			tm->tm_mday = bigval;
			break;
		case MONTH:
			tm->tm_mon = tp->value;
			break;
		case TIME:
			if (parsetime(fields[i], tm) < 0)
				return -1;
			break;
		case DTZ:
#if 0
			tm->tm_isdst++;
#endif
			/* FALLTHROUGH */
		case TZ:
			*tzp = FROMVAL(tp);
			break;
		case IGNORE:
			break;
		case AMPM:
			mer = tp->value;
			break;
		default:
			return -1;	/* bad token type: CANTHAPPEN */
		}
	}
	if (tm->tm_year == -1 || tm->tm_mon == -1 || tm->tm_mday == -1)
		return -1;		/* missing component */
	if (mer == PM)
		tm->tm_hour += 12;
	return 0;
}
