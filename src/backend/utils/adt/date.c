
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
 * date.c --
 *	Functions for the built-in type "AbsoluteTime".
 *	Functions for the built-in type "RelativeTime".
 *	Functions for the built-in type "TimeInterval".
 *
 * Notes:
 *	This code is actually (almost) unused.
 *	It needs to be integrated with Time and struct trange.
 *
 * XXX	This code needs to be rewritten to work with the "new" definitions
 * XXX	in h/tim.h.  Look for int32's, int, long, etc. in the code.  The
 * XXX	definitions in h/tim.h may need to be rethought also.
 */

/*
#include "postgres.h"
*/

#include "c.h"

#include "log.h"
#include "tim.h"

RcsId("$Header$");

#include <ctype.h>
#include <stdio.h>
#include <sys/time.h>
#include <strings.h>

typedef struct { 
	int32	status;
	Time	data[2];
} TimeIntervalData;
typedef TimeIntervalData *TimeInterval;

#define	TM_YEAR_BASE	1900		/* compatible to UNIX time */
#define	EPOCH_YEAR	1970		/* compatible to UNIX time */
#define	YEAR_MAX	2038		/* otherwise overflow */
#define	YEAR_MIN	1902		/* otherwise overflow */
#define	DAYS_PER_LYEAR	366
#define	DAYS_PER_NYEAR	365
#define	HOURS_PER_DAY	24
#define	MINS_PER_HOUR	60
#define	SECS_PER_MIN	60
#define	MAX_LONG	2147483647	/* 2^31 */

/* absolute time definitions */
#define	TIME_NOW_STR		"now"	   /* represents time now */
#ifndef INVALID_ABSTIME
#define	INVALID_ABSTIME		MAX_LONG
#endif	!INVALID_ABSTIME
	/* -2145916801 invalid time representation */
	/*   Dec 31 23:59:59 1901       */
#define	INVALID_ABSTIME_STR 	"Undefined AbsTime"
#define	INVALID_ABSTIME_STR_LEN	(sizeof(INVALID_ABSTIME_STR)-1)
#define	INVALID_MONTH		11
#define	INVALID_DAY		31
#define	INVALID_HOUR		23
#define	INVALID_MIN		59
#define	INVALID_SEC		59
#define	INVALID_YEAR		1901
#define	MAX_ABSTIME		2145916800	/* Jan  1 00:00:00 2038 */ 
#define	MIN_ABSTIME		(-2145916800)	/* Jan  1 00:00:00 1902 */

/* relative time definitions */
#define	MAX_RELTIME		2144448000
	/* about 68 years, compatible to absolute time */
#define	INVALID_RELTIME		MAX_LONG
	/* invalid reltime representation */
#define	INVALID_RELTIME_STR	"Undefined RelTime"
#define	INVALID_RELTIME_STR_LEN (sizeof(INVALID_RELTIME_STR)-1)
#define	RELTIME_LABEL		'@'
#define	RELTIME_PAST		"ago"
#define	DIRMAXLEN		(sizeof(RELTIME_PAST)-1)

#define	InAbsTimeInterval(T) \
	((int32)(T) <= MAX_ABSTIME && (int32)(T) >= MIN_ABSTIME)
#define	InRelTimeInterval(T) \
	((int32)(T) <= MAX_RELTIME && (int32)(T) >= - MAX_RELTIME)
#define	IsCharDigit(C)		isdigit(C)
#define	IsCharA_Z(C)		isalpha(C)		
#define	IsSpace(C)		((C) == ' ')
#define	IsNull(C)		((C) == NULL)

#define	T_INTERVAL_INVAL   0	/* data represents no valid interval */
#define	T_INTERVAL_VALID   1	/* data represents a valid interval */
#define	T_INTERVAL_UNCOM   2	/* data represents an uncomplete interval */
#define	T_INTERVAL_LEN     47	/* 2+20+1+1+1+20+2 : ['...' '...']  */
#define	INVALID_INTERVAL_STR		"Undefined Range"
#define	INVALID_INTERVAL_STR_LEN	(sizeof(INVALID_INTERVAL_STR)-1)

static char	*month_name[] = {
	"Jan","Feb","Mar","Apr","May","Jun","Jul",
	"Aug","Sep","Oct","Nov","Dec" };

static int	day_tab[2][12] = {
	{31,28,31,30,31,30,31,31,30,31,30,31},
	{31,29,31,30,31,30,31,31,30,31,30,31}  };

static	char 	*unit_tab[] = {
	"second", "seconds", "minute", "minutes",
	"hour", "hours", "day", "days", "week", "weeks",
	"month", "months", "year", "years"};
#define UNITMAXLEN 7	/* max length of a unit name */

/* table of seconds per unit (month = 30 days, year = 365 days)  */
static	int	sec_tab[] = { 
	1,1, 60, 60,
	3600, 3600, 86400, 86400, 604800,  604800,
	2592000,  2592000,  31536000,  31536000 };

/* maximal values (in seconds) per unit which can be represented */
static	int	unit_max_quantity[] = {
	2144448000, 2144448000, 35740800, 35740800,
	595680, 595680, 24820, 24820, 3545, 3545,
	827, 827, 68, 68 };


	    /* ========== USER I/O ROUTINES ========== */


/*
 *	abstimein	- converts a time string to an internal format
 *			  checks syntax and range of datetime,
 *			  returns time as long integer in respect of GMT
 */
AbsoluteTime	
abstimein(datetime)
	char	*datetime;
{
	extern	int		timeinsec();
	extern 	AbsoluteTime	timenow();

	struct tm	*brokentime;
	int		error;
	AbsoluteTime	time;

	brokentime = (struct tm *) palloc (sizeof(struct tm)); 
	error = isabstime(datetime, brokentime);
	switch (error) {
	case 0:	/* syntax error in time format */
		time = INVALID_ABSTIME; /* invalid time representation */
		break;
	case 2:
		time = timenow();
		break;
	case 1:
		error = timeinsec(brokentime, &time); /* error not used */
		/* time in respect of GMT !  */
		break;
	}
	if (!TimeIsValid((Time)time)) {
		elog(WARN, "abstimein: cannot handle time of UNIX epoch, yet");
		return(0);
	} else if (time == INVALID_ABSTIME) {
		time = InvalidTime;
	}
	return(time);
}


/*
 *	abstimeout	- converts the internal time format to a string
 */
char	*
abstimeout(datetime)
	AbsoluteTime	datetime;
	/* seconds in respect of Greenwich Mean Time GMT!! */
{
	extern	struct tm	*gmtime();
	extern	char		*asctime();
	extern	char		*index();
	extern	char		*strcpy();
	struct tm		*brokentime;
	/*points to static structure in gmtime*/
	char			*datestring, *p;
	int			i;

	datestring = (char *) palloc(30); 
	if (datetime == INVALID_ABSTIME)
		(void) strcpy(datestring,INVALID_ABSTIME_STR);
	else {
		brokentime = gmtime((long *) &datetime);
		/* no correction of timezone and dst!!*/
		(void) strcpy(datestring,asctime(brokentime));
		for (i=0; i<=3; i++)		 /* skip name of day */
			datestring=datestring++;
		/* change new-line character "\n" at the end to NULL */
		p = index(datestring,'\n');
		*p = NULL;
	}
	return(datestring);
}


/*
 *	reltimein	- converts a reltime string in an internal format
 */
int32	/* RelativeTime */
reltimein(timestring)
	char	*timestring;
{
	int		error;
	int32 /* RelativeTime */	timeinsec;
	int		sign, unitnr;
	long		quantity;

	error = isreltime(timestring, &sign, &quantity, &unitnr);

#ifdef	DATEDEBUG
	elog(DEBUG, "reltimein: isreltime(%s) returns error=%d, %d, %d, %d",
		timestring, error, sign, quantity, unitnr);
#endif	/* !DATEDEBUG */

	if (error != 1) {
		if (error != 2)
			(void) printf("\n\tSyntax error.");
		timeinsec = INVALID_RELTIME;  /*invalid time representation */
	} else {
		/* this check is necessary, while no control on overflow */
		if (quantity > unit_max_quantity[unitnr] || quantity < 0) {
#ifdef	DATEDEBUG
			elog(DEBUG, "reltimein: illegal quantity %d (< %d)",
				quantity, unit_max_quantity[unitnr]);
#endif	/* DATEDEBUG */
			timeinsec = INVALID_RELTIME; /* illegal quantity */
		} else {
			timeinsec = sign * quantity * sec_tab[unitnr];
#ifdef	DATEDEBUG
			elog(DEBUG, "reltimein: computed timeinsec %d",
				timeinsec);
#endif	/* DATEDEBUG */
			if (timeinsec > MAX_RELTIME || 
					timeinsec < - MAX_RELTIME) {
				timeinsec = INVALID_RELTIME;
			}
		}
	}
/*
	if (timeinsec == INVALID_RELTIME)
		(void) printf("\n\tInvalid RelTime");
*/
	if (timeinsec == InvalidTime) {
		elog(WARN, "reltimein: cannot handle reltime of 0 secs, yet");
		return(0);
	} else if (timeinsec == INVALID_ABSTIME) {
		timeinsec = InvalidTime;
	}
	return(timeinsec);
}


/*
 *	reltimeout	- converts the internal format to a reltime string
 */
char	*
reltimeout(timevalue)
	int32 /* RelativeTime */	timevalue;
{	extern char	*sprintf();
	extern int	strlen();
	char		*timestring;
	long		quantity;
	register int	i;
	int		unitnr;

	timestring = (char *) palloc(Max(strlen(INVALID_RELTIME_STR),
					 UNITMAXLEN));
	(void) strcpy(timestring,INVALID_RELTIME_STR);
	if (timevalue == INVALID_RELTIME)
		return(timestring);
	if (timevalue == 0)
		i = 1; /* unit = 'seconds' */
	else
		for (i = 12; i >= 0; i = i-2)
			if ((timevalue % sec_tab[i]) == 0)
				break;  /* appropriate unit found */
	unitnr = i;
	quantity = (timevalue / sec_tab[unitnr]);
	if (quantity > 1 || quantity < -1)
		unitnr++;  /* adjust index for PLURAL of unit */
	timestring = (char *) palloc(Max(strlen(INVALID_RELTIME_STR),
					 UNITMAXLEN));
	if (quantity >= 0)
		(void) sprintf( timestring, "%c %u %s", RELTIME_LABEL,
			       quantity, unit_tab[unitnr]);
	else
		(void) sprintf( timestring, "%c %u %s %s", RELTIME_LABEL,
			       (quantity * -1), unit_tab[unitnr], RELTIME_PAST);
	return(timestring);
}


/*
 *	tintervalin	- converts an interval string to an internal format
 */
TimeInterval	
tintervalin(intervalstr)
	char	*intervalstr;
{	
	int 		error;
	AbsoluteTime	i_start, i_end;
	TimeInterval	interval;

	interval = (TimeInterval) palloc(sizeof(TimeIntervalData));
	error = istinterval(intervalstr, &i_start, &i_end);
	if (error == 0)	/* not a valid interval NOT IMPLEMENTED */
		interval->status = T_INTERVAL_INVAL;
	else {
		interval->data[0] = Min(i_start, i_end);
		interval->data[1] = Max(i_start, i_end);
		interval->status = T_INTERVAL_VALID;
		/* suppose valid range*/
		if (interval->data[0] == INVALID_ABSTIME ||
		    interval->data[1] == INVALID_ABSTIME)
			interval->status = T_INTERVAL_UNCOM;  /* uncomplete */
		if (interval->data[0] == INVALID_ABSTIME &&
		    interval->data[1] == INVALID_ABSTIME)
			interval->status = T_INTERVAL_INVAL;  /* undefined  */
	}
	return(interval);
}


/*
 *	tintervalout	- converts an internal interval format to a string
 *
 */
char	*
tintervalout(interval)
	TimeInterval	interval;
{	extern	char	*abstimeout();
	extern	char	*strcat();
	extern	char	*strcpy();
	char	*i_str, *p;

	i_str = (char	*) palloc( T_INTERVAL_LEN );  /* ['...' '...'] */
	p = (char *) palloc(T_INTERVAL_LEN);
	(void) strcpy(i_str,"[\'");
	if (interval->status == T_INTERVAL_INVAL)
		(void) strcat(i_str,INVALID_INTERVAL_STR);
	else {
		p = abstimeout(interval->data[0]);
		(void) strcat(i_str,p);
		(void) strcat(i_str,"\' \'");
		p = abstimeout(interval->data[1]);
		(void) strcat(i_str,abstimeout(interval->data[1]));
	}
	(void) strcat(i_str,"\']\0");
	return(i_str);
}


	     /* ========== PUBLIC ROUTINES ========== */


/*
 *	timemi		- returns the value of (AbsTime_t1 - RelTime_t2)
 */
AbsoluteTime
timemi(AbsTime_t1, RelTime_t2)
	AbsoluteTime	AbsTime_t1;
	RelativeTime	RelTime_t2;
{
       AbsoluteTime	t;

       if (InAbsTimeInterval(AbsTime_t1) && InRelTimeInterval(RelTime_t2)) {
	       t = AbsTime_t1 - RelTime_t2;
	       if (InAbsTimeInterval(t))
		       return(t);
       }
       return(INVALID_ABSTIME);
}


/*
 *	timepl		- returns the avlue of (AbsTime_t1 + RelTime_t2)
 */
AbsoluteTime
timepl(AbsTime_t1,RelTime_t2)
	AbsoluteTime	AbsTime_t1;
	RelativeTime	RelTime_t2;
{
	AbsoluteTime	t;

	if (InAbsTimeInterval(AbsTime_t1) && InRelTimeInterval(RelTime_t2)) {
		t = AbsTime_t1 + RelTime_t2;
		if (InAbsTimeInterval(t))
			return(t);
	}
	return(INVALID_ABSTIME);
}


/*
 *	ininterval	- returns 1, iff absolute date is in the interval
 */
int
ininterval(t, interval)
	int32	/* AbsoluteTime */	t;
	TimeInterval	interval;
{
	if (interval->status == T_INTERVAL_VALID) {
		if (t >= (int32)interval->data[0] &&
				t <= (int32)interval->data[1]) {

			return(1);	/* t in valid interval */
		}
	}
	return(0);
}

/*
 *	intervalrel	- returns  relative time corresponding to interval
 */
RelativeTime
intervalrel(interval)
	TimeInterval	interval;
{
	if (interval->status == T_INTERVAL_VALID)
		return(timemi(interval->data[1], interval->data[0]));
	else
		return(INVALID_RELTIME);
}


/*
 *	timenow		- returns  time "now", internal format
 */
AbsoluteTime	
timenow()
{
	extern	int		gettimeofday();
	extern	int		timeinsec();
	extern	struct tm 	*localtime();

	struct timeval *tp;
	struct timezone *tzp;
	struct tm	*brokentime;
	long		sec;

	tp = (struct timeval *) palloc(sizeof(struct timeval));
	tzp= (struct timezone *) palloc(sizeof(struct timezone));
	(void) gettimeofday(tp, tzp);
	sec = tp->tv_sec;	/* in GMT! represents NOT the local time!*/
	brokentime = (struct tm *) palloc(sizeof(struct tm));
	brokentime = localtime(&sec);
	(void) timeinsec(brokentime, (AbsoluteTime *) &sec);
	/* now local time in */
	/* respect of GMT!   */
	return(sec);
}


/*
 *	timenowout	- returns a pointer to "<current_date_and_time>"
 */
char	*
timenowout()
{
	extern			gettimeofday();
	extern	struct tm 	*localtime();
	extern	char		*asctime();

	struct timeval	*tp;
	struct timezone *tzp;
	struct tm	*brokentime;
	long		sec;
	int		i;
	char		*datestring;

	tp = (struct timeval *) palloc(sizeof(struct timeval));
	tzp= (struct timezone *) palloc(sizeof(struct timezone));
	(void) gettimeofday(tp, tzp);
	sec = tp->tv_sec;
	brokentime = (struct tm *) palloc(sizeof(struct tm));
	brokentime = localtime(&sec);
	datestring = asctime(brokentime);
	for (i=0; i<=3; i++)
		datestring=datestring++;
	return(datestring);
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
 *	reltimeeq	- returns 1, iff arguments are equal
 *	reltimene	- returns 1, iff arguments are not equal
 *	reltimelt	- returns 1, iff t1 less than t2
 *	reltimegt	- returns 1, iff t1 greater than t2
 *	reltimele	- returns 1, iff t1 less than or equal to t2
 *	reltimege	- returns 1, iff t1 greater than or equal to t2
 */
int32
reltimeeq(t1,t2)
	RelativeTime	t1,t2;
{ return(t1 == t2); }

int32
reltimene(t1, t2)
	RelativeTime	t1,t2;
{ return(t1 != t2); }

int32
reltimelt(t1, t2)
	int32 /* RelativeTime */	t1,t2;
{ return(t1 < t2); }

int32
reltimegt(t1, t2)
	int32 /* RelativeTime */	t1,t2;
{ return(t1 > t2); }

int32
reltimele(t1, t2)
	int32 /* RelativeTime */	t1,t2;
{ return(t1 <= t2); }

int32
reltimege(t1, t2)
	int32 /* RelativeTime */	t1,t2;
{ return(t1 >= t2); }


/*
 *	intervaleq	- returns 1, iff interval i1 is equal to interval i2
 */
int32
intervaleq(i1, i2)
	TimeInterval	i1, i2;
{
	Assert(0);	/* XXX */

	if (i1->status == T_INTERVAL_INVAL || i2->status == T_INTERVAL_INVAL)
		return(0);	/* invalid interval */
	return((i1->data[0] == i2->data[0]) &&
	       (i1->data[1] == i2->data[1]) );
}

/*
 *	intervalleneq	- returns 1, iff length of interval i is equal to
 *				reltime t
 */
int32
intervalleneq(i,t)
	TimeInterval	i;
	RelativeTime	t;
{
	extern	RelativeTime	intervalrel();

	Assert(0);	/* XXX */

	if ((i->status == T_INTERVAL_INVAL) || (t == INVALID_RELTIME))
		return(0);
	return ( (intervalrel(i) == t));
}

/*
 *	intervallenne	- returns 1, iff length of interval i is not equal
 *				to reltime t
 */
int32
intervallenne(i,t)
	TimeInterval	i;
	RelativeTime	t;
{
	extern	RelativeTime	intervalrel();

	Assert(0);	/* XXX */

	if ((i->status == T_INTERVAL_INVAL) || (t == INVALID_RELTIME))
		return(0);
	return ( (intervalrel(i) != t));
}

/*
 *	intervallenlt	- returns 1, iff length of interval i is less than
 *				reltime t
 */
int32
intervallenlt(i,t)
	TimeInterval	i;
	RelativeTime	t;
{
	extern	RelativeTime	intervalrel();

	Assert(0);	/* XXX */

	if ((i->status == T_INTERVAL_INVAL) || (t == INVALID_RELTIME))
		return(0);
	return ( (intervalrel(i) < t));
}

/*
 *	intervallengt	- returns 1, iff length of interval i is greater than
 *				reltime t
 */
int32
intervallengt(i,t)
	TimeInterval	i;
	RelativeTime	t;
{
	extern	RelativeTime	intervalrel();

	Assert(0);	/* XXX */

	if ((i->status == T_INTERVAL_INVAL) || (t == INVALID_RELTIME))
		return(0);
	return ( (intervalrel(i) > t));
}

/*
 *	intervallenle	- returns 1, iff length of interval i is less or equal
 *				    than reltime t
 */
int32
intervallenle(i,t)
	TimeInterval	i;
	RelativeTime	t;
{	
	extern	RelativeTime	intervalrel();

	Assert(0);	/* XXX */

	if ((i->status == T_INTERVAL_INVAL) || (t == INVALID_RELTIME))
		return(0);
	return ( (intervalrel(i) <= t));
}

/*
 *	intervallenge	- returns 1, iff length of interval i is greater or
 * 				equal than reltime t
 */
int32
intervallenge(i,t)
	TimeInterval	i;
	RelativeTime	t;
{	extern	RelativeTime	intervalrel();

	Assert(0);	/* XXX */

	if ((i->status == T_INTERVAL_INVAL) || (t == INVALID_RELTIME))
		return(0);
	return ( (intervalrel(i) >= t));
}

/*
 *	intervalct	- returns 1, iff interval i1 contains interval i2
 */
int32
intervalct(i1,i2)
	TimeInterval	i1, i2;
{
	Assert(0);	/* XXX */

	if (i1->status == T_INTERVAL_INVAL || i2->status == T_INTERVAL_INVAL)
		return(0);	/* invalid interval */
	return((i1->data[0] <= i2->data[0]) &&
	       (i1->data[1] >= i2->data[1]) );
}

/*
 *	intervalov	- returns 1, iff interval i1 (partially) overlaps i2
 */
int32
intervalov(i1, i2)
	TimeInterval	i1, i2;
{
	Assert(0);	/* XXX */

	if (i1->status == T_INTERVAL_INVAL || i2->status == T_INTERVAL_INVAL)
		return(0);	/* invalid interval */
	return(ininterval(i2->data[0], i1) ||
	       ininterval(i2->data[1], i1) );
}

/*
 *	intervalstart	- returns  the start of interval i
 */
AbsoluteTime
intervalstart(i)
	TimeInterval	i;
{
	return(i->data[0]);
}

/*
 *	intervalend	- returns  the end of interval i
 */
AbsoluteTime
intervalend(i)
	TimeInterval	i;
{
	return(i->data[1]);
}


	     /* ========== PRIVATE ROUTINES ========== */


/*
 *	isabstime	- returns 1, iff datestring is of type abstime
 *				(and returns the convertion in brokentime)
 *			  returns 0 for any syntax error
 *			  returns 2 for time 'now'
 *
 *	Absolute time:
 *
 *	Month ` ' Day [ ` ' Hour `:' Minute `:' Second ] ` ' Year
 *
 *	where
 *	Month is	`Jan', `Feb', ..., `Dec'
 *	Day is		` 1', ` 2', ..., `31'
 *	Hour is		`01', `02', ..., `24'
 *	Minute is	`00', `01', ..., `59'
 *	Second is	`00', `01', ..., `59'
 *	Year is		`1902', `1903', ..., `2038'
 *
 *	OR    `Undefined AbsTime'  (see also INVALID_ABSTIME_STR)
 *
 *	VALID dates from  Jan 1 00:00:00 1902  to Jan 1 00:00:00 2038.
 *
 */
int
isabstime(datestring, brokentime)
	char	*datestring;
	struct tm	*brokentime;
{
	extern int	strncmp();
	extern int	strlen();

	register char	*p;
	register char	c;
	char		month[3];
	int		day, yday, monthnum, year, hour,
			min, sec, yeardigits,leap;
	int		timeoption, i;

	p = datestring;
	/* skip leading blanks */
	while (c = *p) {
		if ( ! IsSpace(c))
			break;
		p++;
	}
	/* check whether invalid time representation or not */
	if (strncmp(INVALID_ABSTIME_STR, datestring,
		    INVALID_ABSTIME_STR_LEN) == 0) {
		brokentime->tm_sec = INVALID_SEC;
		brokentime->tm_min = INVALID_MIN;
		brokentime->tm_hour = INVALID_HOUR;
		brokentime->tm_mday = INVALID_DAY;
		brokentime->tm_mon = INVALID_MONTH;  	/* 0,...,11 */
		brokentime->tm_year = INVALID_YEAR - TM_YEAR_BASE;
		brokentime->tm_wday = 0;    /* not calculated! */
		brokentime->tm_yday = 0;    /* not calculated! */
		brokentime->tm_isdst = 0;   /* dst_in_effect not relevant! */
		return(1);
	}
	/* check whether time NOW required or not */
	if (strncmp(TIME_NOW_STR, datestring, strlen(TIME_NOW_STR)) == 0)
		return(2);		    /* time NOW required */
	/* handle month */
	for( i=0; i<=2; i++) {
		month[i] = c;
		p++;
		c = *p;
	}
	/* syntax test month*/
	if (c != ' ' || ! correct_month(month, &monthnum))
		return(0);		/*syntax error */
	/* Month correct*/
	p++;
	/* skip blanks between month and number of days*/
	while (c = *p) {
		if ( ! IsSpace(c))	break;
		p++;
	}
	/* syntax test day number*/
	day = 0;
	for (;;) {
		c = *p;
		if (isdigit(c)) {
			day = day * 10 + (c - '0');
			p++;
		} else {
			if (c == ' ' && day != 0)
				break;	/*maybe correct day number found*/
			else
				return(0);	/*syntax error*/
		}
	}
	/* test correct day number day after determination of year!*/
	p++;
	/* skip blanks between number of days and hour:min:sec option or year */
	while (c = *p) {
		if ( ! IsSpace(c))	break;
		p++;
	}
	/* search for hour:minute:sec option */
	hour=0;
	min=0;
	sec=0;
	for (i=0; i<=1; i++) {
		c = *p;
		if (isdigit(c)) {
			hour=hour * 10 + (c - '0');
			p++;
		} else
			return(0);	/*syntax error*/
	}
	c = *p;	/*if hour:min:sec option then c==':' */
	/* else c = third digit of year */
	timeoption = -1;
	if (c == ':')
		timeoption=1;
	if (isdigit(c))
		timeoption=0;
	if (timeoption == -1)	/* no hour:min:sec option and no correct year */
		return(0);	/* syntax error */
	if (timeoption) {
		/* then search for minutes and seconds */
		p++;
		for (i=0; i<=1; i++) {
			c = *p;
			if (isdigit(c)) {
				min=min * 10 + (c - '0');
				p++;
			} else
				return(0);	/*syntax error*/
		}
		c = *p;
		if (c != ':')
			return(0);	/* syntax error */
		p++;
		for (i=0; i<=1; i++) {
			c = *p;
			if (isdigit(c)) {
				sec=sec * 10 + (c - '0');
				p++;
			} else
				return(0);	/*syntax error*/
		}
		/* skip blanks between hour:min:sec option and year*/
		while (c = *p) {
			if ( ! IsSpace(c))	break;
			p++;
		}
	}
	/* search for year digits */
	if (timeoption) {
		yeardigits=4;
		year=0;
	} else {	/* no hour:min:sec option, only 4 digits of year */
		year=hour;	
		yeardigits=2;	/* first two digits in variable hour */
		hour=0; 	/* no  hour:min:sec  option !! */
	}
	/* collect rest digits of year */
	for (i=0; i <= (yeardigits - 1); i++) {
		c = *p;
		if (isdigit(c)) {
			year=year * 10 + (c - '0');
			p++;
		} else
			return(0);	/*syntax error */
	}
	/* year found */
	/* after the year digits a NULL have to be found */
	c = *p;
	if ( ! IsNull(c) )
		return(0);  /* syntax error at the end */
	/* test whether year is correct or not */
	if (year < YEAR_MIN || year > YEAR_MAX)
		return(0);		/*invalid number */
	/* test correct day number in respect of year and month  */
	leap = isleap(year);
	/*	leap = year%4 == 0 && year%100 != 0 || year%400 == 0; */
	if (day < 1 || day > day_tab[leap] [monthnum])
		return(0); 	/* invalid day number */
	/* calculate day-of-year */
	yday = day;
	for (i=0; i < monthnum; i++)
		yday += day_tab[leap] [i];
	/* define result values   LOCAL TIME*/
	brokentime->tm_sec = sec;
	brokentime->tm_min = min;
	brokentime->tm_hour = hour;
	brokentime->tm_mday = day;
	brokentime->tm_mon = monthnum;  	/* 0,...,11 */
	brokentime->tm_year = year - TM_YEAR_BASE;
	brokentime->tm_wday = 0;		/* not calculated! */
	brokentime->tm_yday = yday;
	brokentime->tm_isdst = 0;	/* daylight saving time not in effect?*/
	/* NOT relevant in this context! */
	return(1);
}


/*
 *	correct_month	- returns 1, iff month is a correct month descriptor
 *				     else 0.
 *	output parameter:
 *		num:	points to an integer which is an index 
 *			in table month_name[]
 */
int
correct_month(month, num)
	char	month[];
	int	*num;			/* number of month 0,...,11 */
{
	extern	int	strncmp();
	extern	char	*month_name[];
	int		i = 0;

	while (i <= 11) {
		if (strncmp(month, month_name[i],3) == 0) {
			*num = i;
			return(1);
		}
		i++;
	}
	return(0);  /* invalid month describtion */
}

/*
 *	isleap		- returns 1, iff year is a leap year
 */
int
isleap(year)
	int	year;
{
	return(year%4 == 0 && year%100 != 0 || year%400 == 0);
}


/*
 *	timeinsec	- returns 1, iff a valid date is given, otherwise 0
 *
 *	output parameter
 *		seconds:	date in seconds (in respect of GMT!)
 */
int
timeinsec(t, seconds)
	struct tm	*t;
	AbsoluteTime	*seconds;	/* absolute time in seconds */
{
	register long	sec;
	register int	year, mon;

	if (t == NULL)	
		return(0);
	sec = 0;
	*seconds = INVALID_ABSTIME;
	year = t->tm_year + TM_YEAR_BASE;
	mon = t->tm_mon + 1;  /* mon  1,...,12 */
	if (year >= EPOCH_YEAR) {
		/* collect days, maby also the Feb 29. */
		if (isleap(year) && mon > 2)
			++sec;
		for (--year; year >= EPOCH_YEAR; --year)
			sec += isleap(year) ? DAYS_PER_LYEAR : DAYS_PER_NYEAR;
		--mon;  /* mon  0,...,11 */
		while(--mon >= 0)
			sec += day_tab[0][mon];
		sec += t->tm_mday - 1;
		sec = HOURS_PER_DAY * sec + t->tm_hour;
		sec = MINS_PER_HOUR * sec + t->tm_min;
		sec = SECS_PER_MIN * sec + t->tm_sec;
		/* year >= EPOCH_YEAR ==> sec >= 0 , otherwise overflow!! */
		if (sec >= 0 && sec <= MAX_ABSTIME) {
			*seconds = sec;	/* seconds in respect of */
			return(1);  	/*  Greenwich Mean Time GMT */
		} else
			return(0);
	} else {	/* collect days, maby also the Feb 29. */
		if(isleap(year) && mon <= 2)
			++sec;
		for (++year; year < EPOCH_YEAR; ++year)
			sec += isleap(year) ? DAYS_PER_LYEAR : DAYS_PER_NYEAR;
		--mon;  /* mon  0,...,11 */
		while(++mon <= 11)
			sec += day_tab[0][mon];
		sec += day_tab[isleap(t->tm_year)][t->tm_mon] - t->tm_mday;
		sec = HOURS_PER_DAY * sec + (24 - (t->tm_hour + 1));
		sec = MINS_PER_HOUR * sec + (60 - (t->tm_min + 1));
		sec = SECS_PER_MIN * sec + (60 - t->tm_sec);
		sec = -sec; /* year was less then EPOCH_YEAR !! */
		/* year < EPOCH_YEAR ==> sec < 0 , otherwise overflow!! */
		if (sec < 0 && sec >= MIN_ABSTIME) {
			*seconds = sec;	/* seconds in respect of */
			return(1);  	/*  Greenwich Mean Time GMT */
		} else
			return(0);
	}
}


/*
 *	isreltime	- returns 1, iff datestring is of type reltime
 *				  2, iff datestring is 'invalid time' identifier
 *				  0, iff datestring contains a syntax error
 *
 *	output parameter:
 *		sign = -1, iff direction is 'ago'
 *			       else sign = 1.
 *		quantity   : quantity of unit
 *		unitnr     :	0 or 1 ... sec
 *				2 or 3 ... min
 *				4 or 5 ... hour
 *				6 or 7 ... day
 *				8 or 9 ... week
 *			       10 or 11... month
 *			       12 or 13... year
 *			  
 *
 *	Relative time:
 *
 *	`@' ` ' Quantity ` ' Unit [ ` ' Direction]
 *
 *	OR  `Undefined RelTime' 	(see also INVALID_RELTIME_STR)
 *
 *	where
 *	Quantity is	`1', `2', ...
 *	Unit is		`second', `minute', `hour', `day', `week',
 *			`month' (30-days), or `year' (365-days),
 *     			or PLURAL of these units.
 *	Direction is	`ago'
 *
 *	VALID time  less or equal   `@ 68 years' 
 *
 */
int
isreltime(timestring, sign, quantity, unitnr)
	char	*timestring;
	int	*sign, *unitnr;
	long	*quantity;
{
	extern	int	strncmp();
	extern 	char	*strcpy();
	register char	*p;
	register char	c;
	int		i;
	char		unit[UNITMAXLEN] ;
	char		direction[DIRMAXLEN];

	(void) strcpy (unit, "");
	(void) strcpy (direction, "");
	p = timestring;
	/* skip leading blanks */
	while (c = *p) {
		if (c != ' ')
			break;
		p++;
	}
	/* Test whether 'invalid time' identifier or not */
	if (!strncmp(INVALID_RELTIME_STR,p,strlen(INVALID_RELTIME_STR) + 1))
		return(2);	/* correct 'invalid time' identifier found */

	/* handle label of relative time */
	if (c != RELTIME_LABEL)
		return(0);	/*syntax error*/
	c = *++p;
	if (c != ' ')	return(0);	/*syntax error*/
	p++;
	/* handle the quantity */
	*quantity = 0;
	for (;;) {
		c = *p;
		if (isdigit(c)) {
			*quantity = *quantity * 10 + (c -'0');
			p++;
		} else {
			if (c == ' ' )
				break; 		/* correct quantity found */
			else
				return(0); 	/* syntax error */
		}
	}
	/* handle unit */
	p++;
	i = 0;
	for (;;) {
		c = *p;
		if (c >= 'a' && c <= 'z' && i <= (UNITMAXLEN - 1)) {
			unit[i] = c;
			p++;
			i++;
		} else {
			if ((c == ' ' || c == '\0')
			    && correct_unit(unit, unitnr))
				break;		/* correct unit found */
			else
				return(0);	/* syntax error */
		}
	}
	/* handle optional direction */
	if (c == ' ')
		p++;
	i = 0;
	*sign = 1;
	for (;;) {
		c = *p;
		if (c >= 'a' && c <= 'z' && i <= (DIRMAXLEN - 1)) {
			direction[i] = c;
			p++;
			i++;
		} else {
			if ((c == ' ' || c == NULL) && i == 0) {
				*sign = 1;
				break;  /* no direction specified */
			}
			if ((c == ' ' || c == '\0') && i != 0
			    && correct_dir(direction, sign))
				break;	  /* correct direction found */
			else
				return(0); 	/* syntax error*/
		}
	}
	return(1);
}

/*
 *	correct_unit	- returns 1, iff unit is a correct unit description
 *
 *	output parameter:
 *		unptr: points to an integer which is the appropriate unit number
 *		       (see function isreltime())
 */
int
correct_unit(unit, unptr)
	char	unit[];
	int	*unptr;
{
	int	j = 0;

	while (j >= 0) {
		if (strncmp(unit, unit_tab[j], strlen(unit_tab[j])) == 0) {
			*unptr = j;
			return(1);
		}
		j++;
	}
	return (0); /* invalid unit descriptor */
}

/*
 *	correct_dir	- returns 1, iff direction is a corecct identifier
 *
 *	output parameter:
 *		signptr: points to -1 if dir corresponds to past tense
 *			 else  to 1
 */
int
correct_dir(direction,signptr)
	char	direction[];
	int	*signptr;
{	
	*signptr = 1;
	if (strncmp(RELTIME_PAST, direction, strlen(RELTIME_PAST) + 1 )) {
		*signptr = -1;
		return(1);
	} else
		return (0);  /* invalid direction descriptor */
}


/*
 *	istinterval	- returns 1, iff i_string is a valid interval descr.
 *				  0, iff i_string is NOT a valid interval desc.
 *				  2, iff any time is INVALID_ABSTIME
 *
 *	output parameter:
 *		i_start, i_end: interval margins
 *
 *	Time interval:
 *	`[' {` '} `'' <AbsTime> `'' {` '} `'' <AbsTime> `'' {` '} `]'
 *
 *	OR  `Undefined Range'	(see also INVALID_INTERVAL_STR)
 *
 *	where <AbsTime> satisfies the syntax of absolute time.
 *
 *	e.g.  [  '  Jan 18 1902'   'Jan 1 00:00:00 1970']
 */
int
istinterval(i_string, i_start, i_end)
	char	*i_string;
	AbsoluteTime	*i_start, *i_end;
{
	extern	AbsoluteTime	abstimein();
	register char		*p,*p1;
	register char		c;

	p = i_string;
	/* skip leading blanks up to '[' */
	while (c = *p) {
		if ( IsSpace(c))
			p++;
		else if (c != '[')
			return(0); /* syntax error */
		else
			break;
	}
	p++;
	/* skip leading blanks up to "'" */
	while (c = *p) {
		if (IsSpace(c))
			p++;
		else if (c != '\'')
			return (0);	/* syntax error */
		else
			break;
	}
	p++;
	if (strncmp(INVALID_INTERVAL_STR,p,strlen(INVALID_INTERVAL_STR)) == 0)
		return(0);	/* undefined range, handled like a syntax err.*/
	/* search for the end of the first date and change it to a NULL*/
	p1 = p;
	while (c = *p1) {
		if ( c == '\'') {
			*p1 = NULL;
			break;
			}
		p1++;
	}
	/* get the first date */
	*i_start = abstimein(p);	/* first absolute date */
	/* rechange NULL at the end of the first date to a "'" */
	*p1 = '\'';
	p = ++p1;
	/* skip blanks up to "'", beginning of second date*/
	while (c = *p) {
		if (IsSpace(c))
			p++;
		else if (c != '\'')
			return (0);	/* syntax error */
		else
			break;
	}	
	p++;
	/* search for the end of the second date and change it to a NULL*/
	p1 = p;
	while (c = *p1) {
		if ( c == '\'') {
			*p1 = NULL;
			break;
		}
		p1++;
	}
	/* get the second date */
	*i_end = abstimein(p);		/* second absolute date */
	/* rechange NULL at the end of the first date to a ''' */
	*p1 = '\'';
	p = ++p1;
	/* skip blanks up to ']'*/
	while (c = *p) {
		if ( IsSpace(c))
			p++;
		else if (c != ']')
			return(0); /*syntax error */
		else
			break;
	}
	p++;
	c = *p;
	if ( ! IsNull(c))
		return (0);	/* syntax error */
	/* it seems to be a valid interval */
	return(1);	
}
