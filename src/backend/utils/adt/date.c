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

#include <ctype.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/timeb.h>
#include <strings.h>

#include "tmp/postgres.h"
#include "tmp/miscadmin.h"
#include "utils/log.h"
#include "utils/nabstime.h"

RcsId("$Header$");

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
#define	TIME_EPOCH_STR		"epoch"	   /* Jan 1 00:00:00 1970 GMT */
#define TIME_EPOCH_STR_LEN	(sizeof(TIME_EPOCH_STR)-1)
#ifndef INVALID_ABSTIME
#define	INVALID_ABSTIME		MAX_LONG
#endif	!INVALID_ABSTIME
	/* -2145916801 invalid time representation */
	/*   Dec 31 23:59:59 1901       */
#define	INVALID_ABSTIME_STR 	"Undefined AbsTime"
#define	INVALID_ABSTIME_STR_LEN	(sizeof(INVALID_ABSTIME_STR)-1)

/*
 *  Unix epoch is Jan  1 00:00:00 1970.  Postgres knows about times
 *  sixty-eight years on either side of that.
 */ 

#define	MAX_ABSTIME		2147483647	/* Jan 19 03:14:07 2038 GMT */
#define	MIN_ABSTIME		(0)		/* Jan  1 00:00:00 1970 */

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


struct timeb *TimeDifferenceFromGMT = NULL;
static bool TimeDiffIsInited = false;
static char *timezonename = NULL;

/*
 * Function prototypes -- internal to this file only
 */

int isabstime ARGS((char *datestring , struct tm *brokentime ));
int correct_month ARGS((char month [], int *num ));
int isleap ARGS((int year ));
int timeinsec ARGS((struct tm *t , AbsoluteTime *seconds ));
int isreltime ARGS((char *timestring , int *sign , long *quantity , int *unitnr ));
int correct_unit ARGS((char unit [], int *unptr ));
int correct_dir ARGS((char direction [], int *signptr ));
int istinterval ARGS((char *i_string , AbsoluteTime *i_start , AbsoluteTime *i_end ));
char *timenowout ARGS((void ));
AbsoluteTime timenow ARGS((void ));
RelativeTime intervalrel ARGS((TimeInterval interval ));
int ininterval ARGS((int32 t , TimeInterval interval ));
AbsoluteTime timemi ARGS((AbsoluteTime AbsTime_t1 , RelativeTime RelTime_t2 ));
char *tintervalout ARGS((TimeInterval interval ));
TimeInterval tintervalin ARGS((char *intervalstr ));
int32 reltimeeq ARGS((RelativeTime t1 , RelativeTime t2 ));
int32 reltimene ARGS((RelativeTime t1 , RelativeTime t2 ));
int32 reltimelt ARGS((int32 t1 , int32 t2 ));
int32 reltimegt ARGS((int32 t1 , int32 t2 ));
int32 reltimele ARGS((int32 t1 , int32 t2 ));
int32 reltimege ARGS((int32 t1 , int32 t2 ));
int32 intervaleq ARGS((TimeInterval i1 , TimeInterval i2 ));
int32 intervalleneq ARGS((TimeInterval i , RelativeTime t ));
int32 intervallenne ARGS((TimeInterval i , RelativeTime t ));
int32 intervallenlt ARGS((TimeInterval i , RelativeTime t ));
int32 intervallengt ARGS((TimeInterval i , RelativeTime t ));
int32 intervallenle ARGS((TimeInterval i , RelativeTime t ));
int32 intervallenge ARGS((TimeInterval i , RelativeTime t ));
int32 intervalct ARGS((TimeInterval i1 , TimeInterval i2 ));
int32 intervalov ARGS((TimeInterval i1 , TimeInterval i2 ));
AbsoluteTime intervalstart ARGS((TimeInterval i ));
AbsoluteTime intervalend ARGS((TimeInterval i ));

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
	int		which;
	AbsoluteTime	time;
	struct tm	brokentime;

	if ( TimeDiffIsInited == false ) {
	    TimeDifferenceFromGMT = (struct timeb *)
	      malloc( sizeof(struct timeb));
	    ftime(TimeDifferenceFromGMT);
	    timezonename = (char *) timezone ( TimeDifferenceFromGMT->timezone,
			        TimeDifferenceFromGMT->dstflag ) ;
	    TimeDiffIsInited = true;
	}

	which = isabstime(datetime, &brokentime);

	switch (which) {

	case 3:							/* epoch */
		time = MIN_ABSTIME;
		break;

	case 2:							/* now */
		time = GetCurrentTransactionStartTime();
		break;

	case 1:							/* from user */
		(void) timeinsec(&brokentime, &time);

		/* user inputs in local time, we need to store things
		 * in GMT so that moving databases across timezones
		 * (amongst other things) behave predictably
		 */

		time = time + 
		  (TimeDifferenceFromGMT->timezone * 60 ) -
		    ( TimeDifferenceFromGMT->dstflag * 3600 ) ;
		  
		
		break;

	case 0:							/* error */
		time = INVALID_ABSTIME;
		break;
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

	if ( TimeDiffIsInited == false ) {
	    TimeDifferenceFromGMT = (struct timeb *)
	      malloc( sizeof(struct timeb));
	    ftime(TimeDifferenceFromGMT);
	    timezonename = (char *) timezone ( TimeDifferenceFromGMT->timezone,
			        TimeDifferenceFromGMT->dstflag ) ;
	    TimeDiffIsInited = true;
	}

	if (datetime == INVALID_ABSTIME) {
	    datestring = (char *) palloc ( INVALID_ABSTIME_STR_LEN +1 );
	    (void) strcpy(datestring,INVALID_ABSTIME_STR);
	} else if ( datetime == 0 ) {
	    datestring = (char *)palloc ( TIME_EPOCH_STR_LEN + 1 );
	    (void) strcpy(datestring, TIME_EPOCH_STR );
	} else   {
	    /* Using localtime instead of gmtime
	     * does the correct conversion !!!
	     */
	    char *temp_date_string = NULL;

	    brokentime = localtime((long *) &datetime);
	    temp_date_string = asctime(brokentime);

	    /* add ten to the length because we want to append
	     * the local timezone spec which might be up to GMT+xx:xx
	     */

	    datestring = (char *)palloc ( strlen ( temp_date_string ) + 10 );

	    (void) strcpy(datestring, 
			  temp_date_string + 4 );    /* skip name of day */

	    /* change new-line character "\n" at the end to timezone 
	       followed by a null */
	    p = index(datestring,'\n');
	    *p = ' ';

	    strcpy ( p+1 , timezonename );
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
		p = nabstimeout(interval->data[0]);
		(void) strcat(i_str,p);
		(void) strcat(i_str,"\' \'");
		p = nabstimeout(interval->data[1]);
		(void) strcat(i_str,nabstimeout(interval->data[1]));
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
 *
 *      Now AbsoluteTime is time since Jan 1 1970 -mer 7 Feb 1992
 */
AbsoluteTime	
timenow()
{
	struct timeval tp;
	struct timezone tzp;

	(void) gettimeofday(&tp, &tzp);
	return((AbsoluteTime)tp.tv_sec);
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
{
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
 * These are support routines for the old abstime implementation.  As
 * far as I know they are no longer used...
 *
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
		/* collect days, maybe also the Feb 29. */
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
	int		localSign;
	int		localUnitNumber;
	long		localQuantity;

	if (!PointerIsValid(sign)) {
		sign = &localSign;
	}
	if (!PointerIsValid(unitnr)) {
		unitnr = &localUnitNumber;
	}
	if (!PointerIsValid(quantity)) {
		quantity = &localQuantity;
	}
	unit[0] = '\0';
	direction[0] = '\0';
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
	*i_start = nabstimein(p);	/* first absolute date */
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
	*i_end = nabstimein(p);		/* second absolute date */
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
