/* $Header$ */

#ifndef _NABSTIME_INCL_
#define _NABSTIME_INCL_

#include <sys/types.h>
#include <time.h>

/* ----------------------------------------------------------------
 *		time types + support macros
 *
 *
 * ----------------------------------------------------------------
 */
typedef uint32	AbsoluteTime;

typedef struct { 
	int32	status;
	Time	data[2];
} TimeIntervalData;
typedef TimeIntervalData *TimeInterval;

#define EPOCH_ABSTIME 4294967294
#define INVALID_ABSTIME 4294967295
#define InvalidAbsoluteTime	INVALID_ABSTIME

#define InvalidTime	INVALID_ABSTIME	/* XXX this will disappear */

/* ----------------
 *	time support macros (from tim.h)
 * ----------------
 */

#define AbsoluteTimeIsValid(time) \
    ((bool) ((time) != InvalidAbsoluteTime))

#define AbsoluteTimeIsReal(time) \
    ((bool) (((unsigned)(time)) < (unsigned)EPOCH_ABSTIME))

#define RelativeTimeIsValid(time) \
    ((bool) ((time) != InvalidRelativeTime))

#define TimeIsValid(time) AbsoluteTimeIsValid(time)

#define GetCurrentAbsoluteTime() \
    ((Time) GetSystemTime())

/* XXX remove this */
#define GetCurrentTime() \
    ((AbsoluteTime) GetCurrentAbsoluteTime())

/*
 * GetSystemTime --
 *	Returns system time.
 */
#define GetSystemTime() \
    ((SystemTime) (time(0l)))

/*
 *  Meridian:  am, pm, or 24-hour style.
 */
#define AM 0
#define PM 1
#define HR24 2

/* can't have more of these than there are bits in an unsigned long */
#define MONTH	1
#define YEAR	2
#define DAY	3
#define TIME	4
#define TZ	5
#define DTZ	6
#define IGNORE	7
#define AMPM	8
/* below here are unused so far */
#define SECONDS	9
#define MONTHS	10
#define YEARS	11
#define NUMBER	12
/* these are only for relative dates */
#define ABS_BEFORE	13
#define ABS_AFTER	14
#define AGO	15


#define SECS(n)		((time_t)(n))
#define MINS(n)		((time_t)(n) * SECS(60))
#define HOURS(n)	((time_t)(n) * MINS(60))	/* 3600 secs */
#define DAYS(n)		((time_t)(n) * HOURS(24))	/* 86400 secs */
/* months and years are not constant length, must be specially dealt with */

#define TOKMAXLEN 6	/* only this many chars are stored in datetktbl */

/* keep this struct small; it gets used a lot */
typedef struct {
	char token[TOKMAXLEN];
	char type;
	char value;		/* this may be unsigned, alas */
} datetkn;


/* nabstime.c prototypes */

AbsoluteTime nabstimein ARGS((char *timestr ));
char *nabstimeout ARGS((AbsoluteTime time ));

int prsabsdate ARGS((char *timestr,struct timeb *now, struct tm *tm, int *tzp));
int parsetime ARGS((char *time , struct tm *tm ));
int split ARGS((char *string , char *fields [], int nfields , char *sep ));
int MonthNumToStr ARGS((int mnum , char *mstr ));
int WeekdayToStr ARGS((int wday , char *wstr ));
int IsNowStr ARGS((char *tstr ));
int IsEpochStr ARGS((char *tstr ));

int tryabsdate ARGS((char *fields[],int nf,struct timeb *now,struct tm *tm,int *tzp));

datetkn *datebsearch ARGS((char *key , datetkn *base , unsigned int nel ));
datetkn *datetoktype ARGS((char *s , int *bigvalp ));

AbsoluteTime dateconv ARGS((struct tm *tm , int zone ));
time_t qmktime ARGS((struct tm *tp ));

bool AbsoluteTimeIsBefore ARGS((AbsoluteTime time1 , AbsoluteTime time2 ));
bool AbsoluteTimeIsAfter ARGS((AbsoluteTime time1 , AbsoluteTime time2 ));

#endif _NABSTIME_INCL_
