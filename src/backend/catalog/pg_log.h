/* ----------------------------------------------------------------
 *   FILE
 *	pg_log.h
 *
 *   DESCRIPTION
 *	the system log relation "pg_log" is not a "heap" relation.
 *	it is automatically created by the transam/ code and the
 *	information here is all bogus and is just here to make the
 *	relcache code happy.
 *
 *   NOTES
 *	The structures and macros used by the transam/ code
 *	to access pg_log should some day go here -cim 6/18/90
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */
#ifndef PgLogIncluded
#define PgLogIncluded 1	/* include this only once */

/* ----------------
 *	postgres.h contains the system type definintions and the
 *	CATALOG(), BOOTSTRAP and DATA() sugar words so this file
 *	can be read by both genbki.sh and the C compiler.
 * ----------------
 */
#include "tmp/postgres.h"

CATALOG(pg_log) BOOTSTRAP {
    oid  	logfoo;
} FormData_pg_log;

typedef FormData_pg_log	*Form_pg_log;

#define Name_pg_log		"pg_log"
#define Natts_pg_log		1
#define Anum_pg_log_logfoo	1

#endif PgLogIncluded
