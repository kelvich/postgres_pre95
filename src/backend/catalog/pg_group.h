/* ----------------------------------------------------------------
 *   FILE
 *	pg_group.h
 *
 *   DESCRIPTION
 *
 *   NOTES
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */
#ifndef PgGroupIncluded
#define PgGroupIncluded 1	/* include this only once */

/* ----------------
 *	postgres.h contains the system type definintions and the
 *	CATALOG(), BOOTSTRAP and DATA() sugar words so this file
 *	can be read by both genbki.sh and the C compiler.
 * ----------------
 */
#include "tmp/postgres.h"

CATALOG(pg_group) {
	char16	groname;
	int2	grosysid;
	bytea	grolist;
} FormData_pg_group;
/* VARIABLE LENGTH STRUCTURE */

typedef FormData_pg_group	*Form_pg_group;

#define Name_pg_group		"pg_group"
#define Natts_pg_group		1
#define Anum_pg_group_groname	1
#define Anum_pg_group_grosysid	2
#define Anum_pg_group_grolist	3

#endif PgGroupIncluded
