/* ----------------------------------------------------------------
 *   FILE
 *	pg_log.h
 *
 *   DESCRIPTION
 *	the system log relation "pg_log" is not a "heap" relation.
 *	it is automatically created by the transam/ code and does
 *	not have CATALOG() or DATA() information.
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


#endif PgLogIncluded
