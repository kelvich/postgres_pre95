/*
 * getabsdate - parse almost any absolute date getdate(3) can (& some it can't)
 *
 * NOTE:  In porting this to postgres I condensed 6 files to 3.  So the abstime
 *	  adt is made up of the files nabstime.c dateconv.c and datetok.c
 *	  -mer 6 Feb 1992
 *
 * $Header$
 */

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/timeb.h>
#include "tmp/postgres.h"
#include "utils/nabstime.h"

#define MAXDATEFIELDS 25

#define ISSPACE(c) ((c) == ' ' || (c) == '\n' || (c) == '\t')

/* this is fast but dirty.  note the return's in the middle. */
#define GOBBLE_NUM(cp, c, x, ip) \
	(c) = *(cp)++; \
	if ((c) < '0' || (c) > '9') \
		return -1;		/* missing digit */ \
	(x) = (c) - '0'; \
	(c) = *(cp)++; \
	if ((c) >= '0' && (c) <= '9') { \
		(x) = 10*(x) + (c) - '0'; \
		(c) = *(cp)++; \
	} \
	if ((c) != ':' && (c) != '\0' && !ISSPACE(c)) \
		return -1;		/* missing colon */ \
	*(ip) = (x)			/* N.B.: no semi-colon here */

#define EPOCH 1970
#define DAYS_PER_400YRS	(time_t)146097
#define DAYS_PER_4YRS	(time_t)1461
#define SECS_PER_DAY	86400
#define SECS_PER_HOUR	3600
#define DIVBY4(n) ((n) >> 2)
#define YRNUM(c, y) (DIVBY4(DAYS_PER_400YRS*(c)) + DIVBY4(DAYS_PER_4YRS*(y)))
#define DAYNUM(c,y,mon,d)	(YRNUM((c), (y)) + mdays[mon] + (d))
#define EPOCH_DAYNUM	DAYNUM(19, 69, 10, 1)	/* really January 1, 1970 */


/* definitions for squeezing values into "value" */
#define ABS_SIGNBIT 0200
#define VALMASK 0177
#define NEG(n)		((n)|ABS_SIGNBIT)
#define SIGNEDCHAR(c)	((c)&ABS_SIGNBIT? -((c)&VALMASK): (c))
#define FROMVAL(tp)	(-SIGNEDCHAR((tp)->value) * 10)	/* uncompress */
#define TOVAL(tp, v)	((tp)->value = ((v) < 0? NEG((-(v))/10): (v)/10))
#define IsLeapYear(yr) ((yr%4) == 0)

char nmdays[] = {
	0, 31, 28, 31,  30, 31, 30,  31, 31, 30,  31, 30, 31
};
/* days since start of year. mdays[0] is March, mdays[11] is February */
static short mdays[] = {
	0, 31, 61, 92, 122, 153, 184, 214, 245, 275, 306, 337
};

/* exports */
extern int dtok_numparsed;

/*
 * parse and convert absolute date in timestr (the normal interface)
 *
 * Returns the number of seconds since epoch (January 1 1970 GMT)
 */
AbsoluteTime
nabstimein(timestr)
char *timestr;
{
    int tz = 0;
    struct tm date;

    if (IsEpochStr(timestr))
	return EPOCH_ABSTIME;
    if (IsNowStr(timestr))
	return GetCurrentTransactionStartTime();

    return (prsabsdate(timestr, NULL, &date, &tz) < 0) ?
		INVALID_ABSTIME :
		dateconv(&date, tz);
}

int
IsNowStr(tstr)
    char *tstr;
{
    while (ISSPACE(*tstr))
	tstr++;

    return ((tstr[0] == 'n' || tstr[0] == 'N') &&
	    (tstr[1] == 'o' || tstr[1] == 'O') &&
	    (tstr[2] == 'w' || tstr[2] == 'W')); 
}


int
IsEpochStr(tstr)
    char *tstr;
{
    while (ISSPACE(*tstr))
	tstr++;
    return ((tstr[0] == 'e' || tstr[0] == 'E') &&
	    (tstr[1] == 'p' || tstr[1] == 'P') &&
	    (tstr[2] == 'o' || tstr[2] == 'O') &&
	    (tstr[3] == 'c' || tstr[3] == 'C') &&
	    (tstr[4] == 'h' || tstr[4] == 'H'));
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


/* return -1 on failure */
int
parsetime(time, tm)
register char *time;
register struct tm *tm;
{
	register char c;
	register int x;

	tm->tm_sec = 0;
	GOBBLE_NUM(time, c, x, &tm->tm_hour);
	if (c != ':')
		return -1;		/* only hour; too short */
	GOBBLE_NUM(time, c, x, &tm->tm_min);
	if (c != ':')
		return 0;		/* no seconds; okay */
	GOBBLE_NUM(time, c, x, &tm->tm_sec);
	/* this may be considered too strict.  garbage at end of time? */
	return (c == '\0' || ISSPACE(c)? 0: -1);
}


/*
 * split - divide a string into fields, like awk split()
 */
int				/* number of fields, including overflow */
split(string, fields, nfields, sep)
char *string;
char *fields[];			/* list is not NULL-terminated */
int nfields;			/* number of entries available in fields[] */
char *sep;			/* "" white, "c" single char, "ab" [ab]+ */
{
	register char *p = string;
	register char c;			/* latest character */
	register char sepc = sep[0];
	register char sepc2;
	register int fn;
	register char **fp = fields;
	register char *sepp;
	register int trimtrail;

	/* white space */
	if (sepc == '\0') {
		while ((c = *p++) == ' ' || c == '\t')
			continue;
		p--;
		trimtrail = 1;
		sep = " \t";	/* note, code below knows this is 2 long */
		sepc = ' ';
	} else
		trimtrail = 0;
	sepc2 = sep[1];		/* now we can safely pick this up */

	/* catch empties */
	if (*p == '\0')
		return(0);

	/* single separator */
	if (sepc2 == '\0') {
		fn = nfields;
		for (;;) {
			*fp++ = p;
			fn--;
			if (fn == 0)
				break;
			while ((c = *p++) != sepc)
				if (c == '\0')
					return(nfields - fn);
			*(p-1) = '\0';
		}
		/* we have overflowed the fields vector -- just count them */
		fn = nfields;
		for (;;) {
			while ((c = *p++) != sepc)
				if (c == '\0')
					return(fn);
			fn++;
		}
		/* not reached */
	}

	/* two separators */
	if (sep[2] == '\0') {
		fn = nfields;
		for (;;) {
			*fp++ = p;
			fn--;
			while ((c = *p++) != sepc && c != sepc2)
				if (c == '\0') {
					if (trimtrail && **(fp-1) == '\0')
						fn++;
					return(nfields - fn);
				}
			if (fn == 0)
				break;
			*(p-1) = '\0';
			while ((c = *p++) == sepc || c == sepc2)
				continue;
			p--;
		}
		/* we have overflowed the fields vector -- just count them */
		fn = nfields;
		while (c != '\0') {
			while ((c = *p++) == sepc || c == sepc2)
				continue;
			p--;
			fn++;
			while ((c = *p++) != '\0' && c != sepc && c != sepc2)
				continue;
		}
		/* might have to trim trailing white space */
		if (trimtrail) {
			p--;
			while ((c = *--p) == sepc || c == sepc2)
				continue;
			p++;
			if (*p != '\0') {
				if (fn == nfields+1)
					*p = '\0';
				fn--;
			}
		}
		return(fn);
	}

	/* n separators */
	fn = 0;
	for (;;) {
		if (fn < nfields)
			*fp++ = p;
		fn++;
		for (;;) {
			c = *p++;
			if (c == '\0')
				return(fn);
			sepp = sep;
			while ((sepc = *sepp++) != '\0' && sepc != c)
				continue;
			if (sepc != '\0')	/* it was a separator */
				break;
		}
		if (fn < nfields)
			*(p-1) = '\0';
		for (;;) {
			c = *p++;
			sepp = sep;
			while ((sepc = *sepp++) != '\0' && sepc != c)
				continue;
			if (sepc == '\0')	/* it wasn't a separator */
				break;
		}
		p--;
	}

	/* not reached */
}

/*
 * Given an AbsoluteTime return the English text version of the date
 *
 * e.g. January 1 1970
 */
char *
nabstimeout(time)
    AbsoluteTime time;
{
    char *outStr;
    struct tm timeVals;
    struct tm *timeValp;
    char month[16];
    char weekday[16];

    outStr = (char *)palloc(64);

    if (time == EPOCH_ABSTIME)
    {
	strcpy(outStr, "epoch");
	return outStr;
    }
    if (time == INVALID_ABSTIME)
    {
	strcpy(outStr, "Invalid Abstime");
	return outStr;
    }

    timeValp = localtime((time_t *)&time);
    MonthNumToStr(timeValp->tm_mon, month);
    WeekdayToStr(timeValp->tm_wday, weekday);
    sprintf(outStr,
	    "%s %s %d %2.2d:%2.2d:%2.2d %d %s",
	    weekday,
	    month,
	    timeValp->tm_mday,
	    timeValp->tm_hour,
	    timeValp->tm_min,
	    timeValp->tm_sec,
	    (timeValp->tm_year >= 69) ? timeValp->tm_year+1900 :
					timeValp->tm_year+2000,
	    timeValp->tm_zone);
    return(outStr);
}

int
MonthNumToStr(mnum, mstr)
    int mnum;
    char *mstr;
{
    switch(mnum)
    {
	case 0:
	    strcpy(mstr, "Jan");
	    break;
	case 1:
	    strcpy(mstr, "Feb");
	    break;
	case 2:
	    strcpy(mstr, "Mar");
	    break;
	case 3:
	    strcpy(mstr, "Apr");
	    break;
	case 4:
	    strcpy(mstr, "May");
	    break;
	case 5:
	    strcpy(mstr, "Jun");
	    break;
	case 6:
	    strcpy(mstr, "Jul");
	    break;
	case 7:
	    strcpy(mstr, "Aug");
	    break;
	case 8:
	    strcpy(mstr, "Sep");
	    break;
	case 9:
	    strcpy(mstr, "Oct");
	    break;
	case 10:
	    strcpy(mstr, "Nov");
	    break;
	case 11:
	    strcpy(mstr, "Dec");
	    break;
	default:
	    strcpy(mstr, "Bad Month");
	    break;
    }
    return 1;
}

int
WeekdayToStr(wday, wstr)
    int wday;
    char *wstr;
{
    switch(wday)
    {
	case 0:
	    strcpy(wstr, "Sun");
	    break;
	case 1:
	    strcpy(wstr, "Mon");
	    break;
	case 2:
	    strcpy(wstr, "Tues");
	    break;
	case 3:
	    strcpy(wstr, "Wed");
	    break;
	case 4:
	    strcpy(wstr, "Thurs");
	    break;
	case 5:
	    strcpy(wstr, "Fri");
	    break;
	case 6:
	    strcpy(wstr, "Sat");
	    break;
	default:
	    strcpy(wstr, "Bad Weekday");
	    break;
    }
    return 1;
}

/* turn a (struct tm) and a few variables into a time_t, with range checking */
AbsoluteTime
dateconv(tm, zone)
register struct tm *tm;
int zone;
{
	tm->tm_wday = tm->tm_yday = 0;

	/* validate, before going out of range on some members */
	if (tm->tm_year < 0 || tm->tm_mon < 1 || tm->tm_mon > 12 ||
	    tm->tm_mday < 1 || tm->tm_hour < 0 || tm->tm_hour >= 24 ||
	    tm->tm_min < 0 || tm->tm_min > 59 ||
	    tm->tm_sec < 0 || tm->tm_sec > 59)
		return -1;

	/*
	 * zone should really be -zone, and tz should be set to tp->value, not
	 * -tp->value.  Or the table could be fixed.
	 */
	tm->tm_min += zone;		/* mktime lets it be out of range */

	/* convert to seconds */
	return qmktime(tm);
}


/*
 * near-ANSI qmktime suitable for use by dateconv; not necessarily as paranoid
 * as ANSI requires, and it may not canonicalise the struct tm.  Ignores tm_wday
 * and tm_yday.
 */
time_t
qmktime(tp)
register struct tm *tp;
{
	register int mon = tp->tm_mon;
	register int day = tp->tm_mday, year = tp->tm_year;
	register time_t daynum;
	register int century;
	time_t nrdaynum;

	/* If it was a 2 digit year */
	if (year < 100)
		year += 1900;
	if (year < EPOCH)
		return -1;		/* can't represent early date */

	/*
	 * validate day against days-per-month table, with leap-year
	 * correction
	 */
	if (day > nmdays[mon])
		if (mon != 2 || year % 4 == 0 &&
		    (year % 100 != 0 || year % 400 == 0) && day > 29)
			return -1;	/* day too large for month */

	/* split year into century and year-of-century */
	century = year / 100;
	year %= 100;
	/*
	 * We calculate the day number exactly, assuming the calendar has
	 * always had the current leap year rules.  (The leap year rules are
	 * to compensate for the fact that the Earth's revolution around the
	 * Sun takes 365.2425 days).  We first need to rotate months so March
	 * is 0, since we want the last month to have the reduced number of
	 * days.
	 */
	if (mon > 2)
		mon -= 3;
	else {
		mon += 9;
		if (year == 0) {
			century--;
			year = 99;
		} else
			--year;
	}
	daynum = -EPOCH_DAYNUM + DAYNUM(century, year, mon, day);

	/* convert to seconds */
	nrdaynum = daynum =
		tp->tm_sec + (tp->tm_min +(daynum*24 + tp->tm_hour)*60)*60;

	/* daylight correction */
	if (tp->tm_isdst < 0)		/* unknown; find out */
		tp->tm_isdst = localtime(&nrdaynum)->tm_isdst;
	if (tp->tm_isdst > 0)
		daynum -= 60*60;

	return daynum < 0? -1: daynum;
}

/* Globals */
static int dtok_numparsed;

/* forwards */
extern datetkn datetktbl[];
extern unsigned szdatetktbl;

datetkn *
datetoktype(s, bigvalp)
char *s;
int *bigvalp;
{
	register char *cp = s;
	register char c = *cp;
	static datetkn t;
	register datetkn *tp = &t;

	if (isascii(c) && isdigit(c)) {
		register int len = strlen(cp);

		if (len > 3 && (cp[1] == ':' || cp[2] == ':'))
			tp->type = TIME;
		else {
			if (bigvalp != NULL)
				/* won't fit in tp->value */
				*bigvalp = atoi(cp);
			if (len == 4)
				tp->type = YEAR;
			else if (++dtok_numparsed == 1)
				tp->type = DAY;
			else
				tp->type = YEAR;
		}
	} else if (c == '-' || c == '+') {
		register int val = atoi(cp + 1);
		register int hr =  val / 100;
		register int min = val % 100;

		val = hr*60 + min;
		if (c == '-')
			val = -val;
		tp->type = TZ;
		TOVAL(tp, val);
	} else {
		char lowtoken[TOKMAXLEN+1];
		register char *ltp = lowtoken, *endltp = lowtoken+TOKMAXLEN;

		/* copy to lowtoken to avoid modifying s */
		while ((c = *cp++) != '\0' && ltp < endltp)
			*ltp++ = (isascii(c) && isupper(c)? tolower(c): c);
		*ltp = '\0';
		tp = datebsearch(lowtoken, datetktbl, szdatetktbl);
		if (tp == NULL) {
			tp = &t;
			tp->type = IGNORE;
		}
	}
	return tp;
}

/*
 * Binary search -- from Knuth (6.2.1) Algorithm B.  Special case like this
 * is WAY faster than the generic bsearch().
 */
datetkn *
datebsearch(key, base, nel)
register char *key;
register datetkn *base;
unsigned int nel;
{
	register datetkn *last = base + nel - 1, *position;
	register int result;

	while (last >= base) {
		position = base + ((last - base) >> 1);
		result = key[0] - position->token[0];
		if (result == 0) {
			result = strncmp(key, position->token, TOKMAXLEN);
			if (result == 0)
				return position;
		}
		if (result < 0)
			last = position - 1;
		else
			base = position + 1;
	}
	return 0;
}


/*
 * to keep this table reasonably small, we divide the lexval for TZ and DTZ
 * entries by 10 and truncate the text field at MAXTOKLEN characters.
 * the text field is not guaranteed to be NUL-terminated.
 */
static datetkn datetktbl[] = {
/*	text		token	lexval */
	"acsst",	DTZ,	63,		/* Cent. Australia */
	"acst",		TZ,	57,		/* Cent. Australia */
	"adt",		DTZ,	NEG(18),	/* Atlantic Daylight Time */
	"aesst",	DTZ,	66,		/* E. Australia */
	"aest",		TZ,	60,		/* Australia Eastern Std Time */
	"ahst",		TZ,	60,		/* Alaska-Hawaii Std Time */
	"am",		AMPM,	AM,
	"apr",		MONTH,	4,
	"april",	MONTH,	4,
	"ast",		TZ,	NEG(24),	/* Atlantic Std Time (Canada) */
	"at",		IGNORE,	0,		/* "at" (throwaway) */
	"aug",		MONTH,	8,
	"august",	MONTH,	8,
	"awsst",	DTZ,	54,		/* W. Australia */
	"awst",		TZ,	48,		/* W. Australia */
	"bst",		TZ,	6,		/* British Summer Time */
	"bt",		TZ,	18,		/* Baghdad Time */
	"cadt",		DTZ,	63,		/* Central Australian DST */
	"cast",		TZ,	57,		/* Central Australian ST */
	"cat",		TZ,	NEG(60),	/* Central Alaska Time */
	"cct",		TZ,	48,		/* China Coast */
	"cdt",		DTZ,	NEG(30),	/* Central Daylight Time */
	"cet",		TZ,	6,		/* Central European Time */
	"cetdst",	DTZ,	12,		/* Central European Dayl.Time */
	"cst",		TZ,	NEG(36),	/* Central Standard Time */
	"dec",		MONTH,	12,
	"decemb",	MONTH,	12,
	"dnt",		TZ,	6,		/* Dansk Normal Tid */
	"dst",		IGNORE,	0,
	"east",		TZ,	NEG(60),	/* East Australian Std Time */
	"edt",		DTZ,	NEG(24),	/* Eastern Daylight Time */
	"eet",		TZ,	12,		/* East. Europe, USSR Zone 1 */
	"eetdst",	DTZ,	18,		/* Eastern Europe */
	"est",		TZ,	NEG(30),	/* Eastern Standard Time */
	"feb",		MONTH,	2,
	"februa",	MONTH,	2,
	"fri",		IGNORE,	5,
	"friday",	IGNORE,	5,
	"fst",		TZ,	6,		/* French Summer Time */
	"fwt",		DTZ,	12,		/* French Winter Time  */
	"gmt",		TZ,	0,		/* Greenwish Mean Time */
	"gst",		TZ,	60,		/* Guam Std Time, USSR Zone 9 */
	"hdt",		DTZ,	NEG(54),	/* Hawaii/Alaska */
	"hmt",		DTZ,	18,		/* Hellas ? ? */
	"hst",		TZ,	NEG(60),	/* Hawaii Std Time */
	"idle",		TZ,	72,		/* Intl. Date Line, East */
	"idlw",		TZ,	NEG(72),	/* Intl. Date Line, West */
	"ist",		TZ,	12,		/* Israel */
	"it",		TZ,	22,		/* Iran Time */
	"jan",		MONTH,	1,
	"januar",	MONTH,	1,
	"jst",		TZ,	54,		/* Japan Std Time,USSR Zone 8 */
	"jt",		TZ,	45,		/* Java Time */
	"jul",		MONTH,	7,
	"july",		MONTH,	7,
	"jun",		MONTH,	6,
	"june",		MONTH,	6,
	"kst",		TZ,	54,		/* Korea Standard Time */
	"ligt",		TZ,	60,		/* From Melbourne, Australia */
	"mar",		MONTH,	3,
	"march",	MONTH,	3,
	"may",		MONTH,	5,
	"mdt",		DTZ,	NEG(36),	/* Mountain Daylight Time */
	"mest",		DTZ,	12,		/* Middle Europe Summer Time */
	"met",		TZ,	6,		/* Middle Europe Time */
	"metdst",	DTZ,	12,		/* Middle Europe Daylight Time*/
	"mewt",		TZ,	6,		/* Middle Europe Winter Time */
	"mez",		TZ,	6,		/* Middle Europe Zone */
	"mon",		IGNORE,	1,
	"monday",	IGNORE,	1,
	"mst",		TZ,	NEG(42),	/* Mountain Standard Time */
	"mt",		TZ,	51,		/* Moluccas Time */
	"ndt",		DTZ,	NEG(15),	/* Nfld. Daylight Time */
	"nft",		TZ,	NEG(21),	/* Newfoundland Standard Time */
	"nor",		TZ,	6,		/* Norway Standard Time */
	"nov",		MONTH,	11,
	"novemb",	MONTH,	11,
	"nst",		TZ,	NEG(21),	/* Nfld. Standard Time */
	"nt",		TZ,	NEG(66),	/* Nome Time */
	"nzdt",		DTZ,	78,		/* New Zealand Daylight Time */
	"nzst",		TZ,	72,		/* New Zealand Standard Time */
	"nzt",		TZ,	72,		/* New Zealand Time */
	"oct",		MONTH,	10,
	"octobe",	MONTH,	10,
	"on",		IGNORE,	0,		/* "on" (throwaway) */
	"pdt",		DTZ,	NEG(42),	/* Pacific Daylight Time */
	"pm",		AMPM,	PM,
	"pst",		TZ,	NEG(48),	/* Pacific Standard Time */
	"sadt",		DTZ,	63,		/* S. Australian Dayl. Time */
	"sast",		TZ,	57,		/* South Australian Std Time */
	"sat",		IGNORE,	6,
	"saturd",	IGNORE,	6,
	"sep",		MONTH,	9,
	"sept",		MONTH,	9,
	"septem",	MONTH,	9,
	"set",		TZ,	NEG(6),		/* Seychelles Time ?? */
	"sst",		DTZ,	12,		/* Swedish Summer Time */
	"sun",		IGNORE,	0,
	"sunday",	IGNORE,	0,
	"swt",		TZ,	6,		/* Swedish Winter Time  */
	"thu",		IGNORE,	4,
	"thur",		IGNORE,	4,
	"thurs",	IGNORE,	4,
	"thursd",	IGNORE,	4,
	"tue",		IGNORE,	2,
	"tues",		IGNORE,	2,
	"tuesda",	IGNORE,	2,
	"ut",		TZ,	0,
	"utc",		TZ,	0,
	"wadt",		DTZ,	48,		/* West Australian DST */
	"wast",		TZ,	42,		/* West Australian Std Time */
	"wat",		TZ,	NEG(6),		/* West Africa Time */
	"wdt",		DTZ,	54,		/* West Australian DST */
	"wed",		IGNORE,	3,
	"wednes",	IGNORE,	3,
	"weds",		IGNORE,	3,
	"wet",		TZ,	0,		/* Western Europe */
	"wetdst",	DTZ,	6,		/* Western Europe */
	"wst",		TZ,	48,		/* West Australian Std Time */
	"ydt",		DTZ,	NEG(48),	/* Yukon Daylight Time */
	"yst",		TZ,	NEG(54),	/* Yukon Standard Time */
	"zp4",		TZ,	NEG(24),	/* GMT +4  hours. */
	"zp5",		TZ,	NEG(30),	/* GMT +5  hours. */
	"zp6",		TZ,	NEG(36),	/* GMT +6  hours. */
};

#if	0
/*
 * these time zones are orphans, i.e. the name is also used by a more
 * likely-to-appear time zone
 */
	"at",		TZ,	NEG(12),	/* Azores Time */
	"bst",		TZ,	NEG(18),	/* Brazil Std Time */
	"bt",		TZ,	NEG(66),	/* Bering Time */
	"edt",		TZ,	66,		/* Australian Eastern DaylTime*/
	"est",		TZ,	60,		/* Australian Eastern Std Time*/
	"ist",		TZ,	33,		/* Indian Standard Time */
	"nst",		TZ,	51,		/* North Sumatra Time */
	"sst",		TZ,	42,		/* South Sumatra, USSR Zone 6 */
	"sst",		TZ,	48,		/* Singapore Std Time */
	"wet",		TZ,	6,		/* Western European Time */
/* military timezones are deprecated by RFC 1123 section 5.2.14 */
	"a",		TZ,	6,		/* UTC+1h */
	"b",		TZ,	12,		/* UTC+2h */
	"c",		TZ,	18,		/* UTC+3h */
	"d",		TZ,	24,		/* UTC+4h */
	"e",		TZ,	30,		/* UTC+5h */
	"f",		TZ,	36,		/* UTC+6h */
	"g",		TZ,	42,		/* UTC+7h */
	"h",		TZ,	48,		/* UTC+8h */
	"i",		TZ,	54,		/* UTC+9h */
	"k",		TZ,	60,		/* UTC+10h */
	"l",		TZ,	66,		/* UTC+11h */
	"m",		TZ,	72,		/* UTC+12h */
	"n",		TZ,	NEG(6),		/* UTC-1h */
	"o",		TZ,	NEG(12),	/* UTC-2h */
	"p",		TZ,	NEG(18),	/* UTC-3h */
	"q",		TZ,	NEG(24),	/* UTC-4h */
	"r",		TZ,	NEG(30),	/* UTC-5h */
	"s",		TZ,	NEG(36),	/* UTC-6h */
	"t",		TZ,	NEG(42),	/* UTC-7h */
	"u",		TZ,	NEG(48),	/* UTC-8h */
	"v",		TZ,	NEG(54),	/* UTC-9h */
	"w",		TZ,	NEG(60),	/* UTC-10h */
	"x",		TZ,	NEG(66),	/* UTC-11h */
	"y",		TZ,	NEG(72),	/* UTC-12h */
	"z",		TZ,	0,		/* UTC */
#endif

static unsigned int szdatetktbl = sizeof datetktbl / sizeof datetktbl[0];
 

/*
 *  AbsoluteTimeIsBefore -- true iff time1 is before time2.
 *
 *	Since we store AbsoluteTimes as unsigned quantities, the comparison
 *	below would fail for times before the Unix epoch.  We cast them to
 *	signed quantities for the sake of this comparison, even though it
 *	violates the type-hiding abstraction.
 *
 *	The above comment isn't really valid anymore since the change
 *	to absolute time storage. -mer 15 Feb. 1992
 */

bool
AbsoluteTimeIsBefore(time1, time2)
	AbsoluteTime	time1;
	AbsoluteTime	time2;
{
    Assert(AbsoluteTimeIsValid(time1));
    Assert(AbsoluteTimeIsValid(time2));

    if ((time1 == time2) || (time2 == EPOCH_ABSTIME))
	return false;
    if (time1 == EPOCH_ABSTIME)
	return true;

    return ((bool)((int32) time1 < (int32) time2));
}

bool
AbsoluteTimeIsAfter(time1, time2)
	AbsoluteTime	time1;
	AbsoluteTime	time2;
{
    Assert(AbsoluteTimeIsValid(time1));
    Assert(AbsoluteTimeIsValid(time2));

    if ((time1 == time2) || (time1 == EPOCH_ABSTIME))
	return false;
    if (time2 == EPOCH_ABSTIME)
	return true;

    return ((bool) ((int32) time1 > (int32) time2));
}


/*
 *	abstimeeq	- returns 1, iff arguments are equal
 *	abstimene	- returns 1, iff arguments are not equal
 *	abstimelt	- returns 1, iff t1 less than t2
 *	abstimegt	- returns 1, iff t1 greater than t2
 *	abstimele	- returns 1, iff t1 less than or equal to t2
 *	abstimege	- returns 1, iff t1 greater than or equal to t2
 *
 */
int32
abstimeeq(t1,t2)
	AbsoluteTime	t1,t2;
{ return(t1 == t2); }

int32
abstimene(t1, t2)
	AbsoluteTime	t1,t2;
{ return(t1 != t2); }

int32
abstimelt(t1, t2)
	int32 /* AbsoluteTime */	t1,t2;
{ return(t1 < t2); }

int32
abstimegt(t1, t2)
	int32 /* AbsoluteTime */	t1,t2;
{ return(t1 > t2); }

int32
abstimele(t1, t2)
	int32 /* AbsoluteTime */	t1,t2;
{ return(t1 <= t2); }

int32
abstimege(t1, t2)
	int32 /* AbsoluteTime */	t1,t2;
{ return(t1 >= t2); }


/*
 *	isabstime	- returns 1, iff datestring is of type abstime
 *				(and returns the convertion in brokentime)
 *			  returns 0 for any syntax error
 *			  returns 2 for time 'now'
 *
 */
int
isabstime(datestring, brokentime)
	char	*datestring;
	struct tm	*brokentime;
{
    int tz = 0;
    struct tm dummy;

    if (brokentime == NULL)
	brokentime = &dummy;

    if (IsNowStr(datestring))
	return 2;
    if (prsabsdate(datestring, NULL, brokentime, &tz) >= 0)
	return 1;
    return 0;
}
