/* ----------------------------------------------------------------
 *   FILE
 *	date.c
 *
 *   DESCRIPTION
 *	Functions for the built-in type "AbsoluteTime".
 *	Functions for the built-in type "RelativeTime".
 *	Functions for the built-in type "TimeInterval".
 *
 *   NOTES
 *	This code is actually (almost) unused.
 *	It needs to be integrated with Time and struct trange.
 *
 * XXX	This code needs to be rewritten to work with the "new" definitions
 * XXX	in h/tim.h.  Look for int32's, int, long, etc. in the code.  The
 * XXX	definitions in h/tim.h may need to be rethought also.
 *
 * XXX  This code has been cleaned up some - avi 07/07/93
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#include <ctype.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <strings.h>
#include <time.h>

#include "tmp/postgres.h"
#include "tmp/miscadmin.h"
#include "utils/builtins.h"
#include "utils/log.h"
#include "utils/nabstime.h"

RcsId("$Header$");

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

#define	INVALID_ABSTIME_STR 	"Undefined AbsTime"
#define	INVALID_ABSTIME_STR_LEN	(sizeof(INVALID_ABSTIME_STR)-1)

#define	INVALID_RELTIME_STR	"Undefined RelTime"
#define	INVALID_RELTIME_STR_LEN (sizeof(INVALID_RELTIME_STR)-1)
#define	RELTIME_LABEL		'@'
#define	RELTIME_PAST		"ago"
#define	DIRMAXLEN		(sizeof(RELTIME_PAST)-1)

/*
 *  Unix epoch is Jan  1 00:00:00 1970.  Postgres knows about times
 *  sixty-eight years on either side of that.
 */ 

#define	IsCharDigit(C)		isdigit(C)
#define	IsCharA_Z(C)		isalpha(C)		
#define	IsSpace(C)		((C) == ' ')
#define	IsNull(C)		((C) == NULL)

#define	T_INTERVAL_INVAL   0	/* data represents no valid interval */
#define	T_INTERVAL_VALID   1	/* data represents a valid interval */
#define	T_INTERVAL_LEN     47	/* 2+20+1+1+1+20+2 : ['...' '...']  */
#define	INVALID_INTERVAL_STR		"Undefined Range"
#define	INVALID_INTERVAL_STR_LEN	(sizeof(INVALID_INTERVAL_STR)-1)

#define ABSTIMEMIN(t1, t2) abstimele((t1),(t2)) ? (t1) : (t2)
#define ABSTIMEMAX(t1, t2) abstimelt((t1),(t2)) ? (t2) : (t1)

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
#define NUNITS 14	/* number of different units */

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

int isreltime ARGS((char *timestring , int *sign , long *quantity , int *unitnr ));
int correct_unit ARGS((char unit [], int *unptr ));
int correct_dir ARGS((char direction [], int *signptr ));
int istinterval ARGS((char *i_string , AbsoluteTime *i_start , AbsoluteTime *i_end ));
char *timenowout ARGS((void ));
AbsoluteTime timenow ARGS((void ));
RelativeTime intervalrel ARGS((TimeInterval interval ));
int ininterval ARGS((int32 t , TimeInterval interval ));
AbsoluteTime timemi ARGS((AbsoluteTime AbsTime_t1 , RelativeTime RelTime_t2 ));
RelativeTime abstimemi ARGS((AbsoluteTime AbsTime_t1 , AbsoluteTime AbsTime_t2 ));
char *tintervalout ARGS((TimeInterval interval ));
TimeInterval tintervalin ARGS((char *intervalstr ));
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
		        }
	}

	return(timeinsec);
}


/*
 *	reltimeout	- converts the internal format to a reltime string
 */
char	*
reltimeout(timevalue)
	int32 /* RelativeTime */	timevalue;
{	char		*timestring;
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
	AbsoluteTime	i_start, i_end, t1, t2;
	TimeInterval	interval;

	interval = (TimeInterval) palloc(sizeof(TimeIntervalData));
	error = istinterval(intervalstr, &t1, &t2);
	if (error == 0)
		interval->status = T_INTERVAL_INVAL;
	if (t1 == INVALID_ABSTIME || t2 == INVALID_ABSTIME)
	        interval->status = T_INTERVAL_INVAL;  /* undefined  */
	else {
	        i_start = ABSTIMEMIN(t1, t2);
	        i_end = ABSTIMEMAX(t1, t2);
		interval->data[0] = i_start;
		interval->data[1] = i_end;
		interval->status = T_INTERVAL_VALID;
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
{
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
 *	mktinterval	- creates a time interval with endpoints t1 and t2
 */
TimeInterval mktinterval(t1, t2)
        AbsoluteTime	t1;
        AbsoluteTime	t2;
{
        AbsoluteTime	tstart = ABSTIMEMIN(t1, t2), tend = ABSTIMEMAX(t1, t2);
	TimeInterval interval;

	interval = (TimeInterval) palloc(sizeof(TimeIntervalData));
        if (t1 == INVALID_ABSTIME || t2 == INVALID_ABSTIME)
	        interval->status = T_INTERVAL_INVAL;
	else {
	        interval->status = T_INTERVAL_VALID;
		interval->data[0] = tstart;
		interval->data[1] = tend;
	}

	return interval;
}

/*
 *	timepl, timemi and abstimemi use the formula
 *		abstime + reltime = abstime
 *	so	abstime - reltime = abstime
 *	and	abstime - abstime = reltime
 */

/*
 *	timepl		- returns the value of (abstime t1 + relime t2)
 */
AbsoluteTime
timepl(t1, t2)
	AbsoluteTime	t1;
	RelativeTime	t2;
{
	if (t1 == CURRENT_ABSTIME)
	        t1 = GetCurrentTransactionStartTime();

        if (AbsoluteTimeIsReal(t1) &&
	    RelativeTimeIsValid(t2) &&
	    ((t2 > 0) ? (t1 < NOEND_ABSTIME - t2)
	            : (t1 > NOSTART_ABSTIME - t2))) 	/* prevent overflow */
	        return (t1 + t2);

	return(INVALID_ABSTIME);
}


/*
 *	timemi		- returns the value of (abstime t1 - reltime t2)
 */
AbsoluteTime
timemi(t1, t2)
	AbsoluteTime	t1;
	RelativeTime	t2;
{
	if (t1 == CURRENT_ABSTIME)
	        t1 = GetCurrentTransactionStartTime();

        if (AbsoluteTimeIsReal(t1) &&
	    RelativeTimeIsValid(t2) &&
	    ((t2 > 0) ? (t1 > NOSTART_ABSTIME + t2)
	              : (t1 < NOEND_ABSTIME + t2))) 	/* prevent overflow */
	        return (t1 - t2);

	return(INVALID_ABSTIME);
}


/*
 *	abstimemi	- returns the value of (abstime t1 - abstime t2)
 */
RelativeTime
abstimemi(t1, t2)
	AbsoluteTime	t1;
	AbsoluteTime	t2;
{
	if (t1 == CURRENT_ABSTIME)
	    t1 = GetCurrentTransactionStartTime();
	if (t2 == CURRENT_ABSTIME)
	    t2 = GetCurrentTransactionStartTime();

	if (AbsoluteTimeIsReal(t1) &&
	    AbsoluteTimeIsReal(t2))
	    return (t1 - t2);

        return(INVALID_RELTIME);
}


/*
 *	ininterval	- returns 1, iff absolute date is in the interval
 */
int
ininterval(t, interval)
	AbsoluteTime	t;
	TimeInterval	interval;
{
        AbsoluteTime t1, t2;

	if (interval->status == T_INTERVAL_VALID && t != INVALID_ABSTIME)
		return (abstimege(t, interval->data[0]) &&
			abstimele(t, interval->data[1]));
	return(0);
}

/*
 *	intervalrel	- returns  relative time corresponding to interval
 */
RelativeTime
intervalrel(interval)
	TimeInterval	interval;
{
        AbsoluteTime t1, t2;

	if (interval->status == T_INTERVAL_VALID)
		return(abstimemi(interval->data[1], interval->data[0]));
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
{
        if (t1 == INVALID_RELTIME || t2 == INVALID_RELTIME)
	        return 0;
	return(t1 == t2);
}

int32
reltimene(t1, t2)
	RelativeTime	t1,t2;
{
        if (t1 == INVALID_RELTIME || t2 == INVALID_RELTIME)
	        return 0;
	return(t1 != t2);
}

int32
reltimelt(t1, t2)
	RelativeTime	t1,t2;
{
        if (t1 == INVALID_RELTIME || t2 == INVALID_RELTIME)
	        return 0;
	return(t1 < t2);
}

int32
reltimegt(t1, t2)
	RelativeTime	t1,t2;
{
        if (t1 == INVALID_RELTIME || t2 == INVALID_RELTIME)
	        return 0;
	return(t1 > t2);
}

int32
reltimele(t1, t2)
	RelativeTime	t1,t2;
{
        if (t1 == INVALID_RELTIME || t2 == INVALID_RELTIME)
	        return 0;
	return(t1 <= t2);
}

int32
reltimege(t1, t2)
	RelativeTime	t1,t2;
{
        if (t1 == INVALID_RELTIME || t2 == INVALID_RELTIME)
	        return 0;
	return(t1 >= t2);
}


/*
 *	intervaleq	- returns 1, iff interval i1 is equal to interval i2
 */
int32
intervaleq(i1, i2)
	TimeInterval	i1, i2;
{
	if (i1->status == T_INTERVAL_INVAL || i2->status == T_INTERVAL_INVAL)
		return(0);	/* invalid interval */
	return(abstimeeq(i1->data[0], i2->data[0]) &&
	       abstimeeq(i1->data[1], i2->data[1]));
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
        RelativeTime rt;

	if ((i->status == T_INTERVAL_INVAL) || (t == INVALID_RELTIME))
		return(0);
	rt = intervalrel(i);
	return (rt != INVALID_RELTIME && rt == t);
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
        RelativeTime rt;

	if ((i->status == T_INTERVAL_INVAL) || (t == INVALID_RELTIME))
		return(0);
	rt = intervalrel(i);
	return (rt != INVALID_RELTIME && rt != t);
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
        RelativeTime rt;

	if ((i->status == T_INTERVAL_INVAL) || (t == INVALID_RELTIME))
		return(0);
	rt = intervalrel(i);
	return (rt != INVALID_RELTIME && rt < t);
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
        RelativeTime rt;

	if ((i->status == T_INTERVAL_INVAL) || (t == INVALID_RELTIME))
		return(0);
	rt = intervalrel(i);
	return (rt != INVALID_RELTIME && rt > t);
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
        RelativeTime rt;

	if ((i->status == T_INTERVAL_INVAL) || (t == INVALID_RELTIME))
		return(0);
	rt = intervalrel(i);
	return (rt != INVALID_RELTIME && rt <= t);
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
        RelativeTime rt;

	if ((i->status == T_INTERVAL_INVAL) || (t == INVALID_RELTIME))
		return(0);
	rt = intervalrel(i);
	return (rt != INVALID_RELTIME && rt >= t);
}

/*
 *	intervalct	- returns 1, iff interval i1 contains interval i2
 */
int32
intervalct(i1,i2)
	TimeInterval	i1, i2;
{
	if (i1->status == T_INTERVAL_INVAL || i2->status == T_INTERVAL_INVAL)
		return(0);
	return(abstimele(i1->data[0], i2->data[0]) &&
	       abstimege(i1->data[1], i2->data[1]));
}

/*
 *	intervalov	- returns 1, iff interval i1 (partially) overlaps i2
 */
int32
intervalov(i1, i2)
	TimeInterval	i1, i2;
{
	if (i1->status == T_INTERVAL_INVAL || i2->status == T_INTERVAL_INVAL)
		return(0);
	return(! (abstimelt(i1->data[1], i2->data[0]) ||
		  abstimegt(i1->data[0], i2->data[1])));
}

/*
 *	intervalstart	- returns  the start of interval i
 */
AbsoluteTime
intervalstart(i)
	TimeInterval	i;
{
        if (i->status == T_INTERVAL_INVAL)
	        return INVALID_ABSTIME;
	return(i->data[0]);
}

/*
 *	intervalend	- returns  the end of interval i
 */
AbsoluteTime
intervalend(i)
	TimeInterval	i;
{
        if (i->status == T_INTERVAL_INVAL)
	        return INVALID_ABSTIME;
	return(i->data[1]);
}


	     /* ========== PRIVATE ROUTINES ========== */

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
			if ((c == ' ' || c == '\0') && i != 0)
			{
			    direction[i] = '\0';
			    correct_dir(direction, sign);
			    break;	  /* correct direction found */
			}
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

	while (j < NUNITS) {
		if (strncmp(unit, unit_tab[j], strlen(unit_tab[j])) == 0) {
			*unptr = j;
			return(1);
		}
		j++;
	}
	return (0); /* invalid unit descriptor */
}

/*
 *	correct_dir	- returns 1, iff direction is a correct identifier
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
	if (strncmp(RELTIME_PAST, direction, strlen(RELTIME_PAST)+1) == 0)
	{
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
