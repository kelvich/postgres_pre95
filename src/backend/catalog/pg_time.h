/* ----------------------------------------------------------------
 *   FILE
 *	pg_time.h
 *
 *   DESCRIPTION
 *	the system commit-time relation "pg_time" is not a "heap" relation.
 *	it is automatically created by the transam/ code and the
 *	information here is all bogus and is just here to make the
 *	relcache code happy.
 *   NOTES
 *	The structures and macros used by the transam/ code
 *	to access pg_time should some day go here -cim 6/18/90
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */
#ifndef PgTimeIncluded
#define PgTimeIncluded 1	/* include this only once */

/* ----------------
 *	postgres.h contains the system type definintions and the
 *	CATALOG(), BOOTSTRAP and DATA() sugar words so this file
 *	can be read by both genbki.sh and the C compiler.
 * ----------------
 */
#include "tmp/postgres.h"

CATALOG(pg_time) BOOTSTRAP {
    oid  	timefoo;
} FormData_pg_time;

typedef FormData_pg_time	*Form_pg_time;

#define Name_pg_time		"pg_time"
#define Natts_pg_time		1
#define Anum_pg_time_timefoo	1


#endif PgTimeIncluded
