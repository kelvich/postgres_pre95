/* ----------------------------------------------------------------
 *   FILE
 *	pg_aggregate.h
 *
 *   DESCRIPTION
 *	definition of the system "aggregate" relation (pg_aggregate)
 *	along with the relation's initial contents.
 *
 *   NOTES
 *	the genbki.sh script reads this file and generates .bki
 *	information from the DATA() statements.
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */
#ifndef PgAggregateIncluded
#define PgAggregateIncluded 1	/* include this only once */

/* ----------------
 *	postgres.h contains the system type definintions and the
 *	CATALOG(), BOOTSTRAP and DATA() sugar words so this file
 *	can be read by both genbki.sh and the C compiler.
 * ----------------
 */
#include "tmp/postgres.h"

/* ----------------------------------------------------------------
 *	pg_aggregate definition.
 *
 *	cpp turns this into typedef struct FormData_pg_aggregate
 * ----------------------------------------------------------------
 */ 
CATALOG(pg_aggregate) {
    char16 	aggname;	/* aggregate name */
    oid 	aggowner;	/* aggregate owner id */
    regproc 	aggfun1;	/* aggregate step function */
    regproc 	aggfun2;	/* aggregate total function */
} FormData_pg_aggregate;

/* ----------------
 *	Form_pg_aggregate corresponds to a pointer to a tuple with
 *	the format of pg_aggregate relation.
 * ----------------
 */
typedef FormData_pg_aggregate	*Form_pg_aggregate;

/* ----------------
 *	compiler constants for pg_aggregate
 * ----------------
 */
#define Name_pg_aggregate		"pg_aggregate"
#define Natts_pg_aggregate		4
#define Anum_pg_aggregate_aggname	1
#define Anum_pg_aggregate_aggowner	2
#define Anum_pg_aggregate_aggfun1	3
#define Anum_pg_aggregate_aggfun2	4

/* ----------------
 *	old definition of struct aggregate
 * ----------------
 */
#ifndef struct_aggregate_Defined
#define struct_aggregate_Defined 1

struct	aggregate {
	char	aggname[16];
	OID	aggowner;
	OID	aggfun1;
	OID	aggfun2;
};
#endif struct_aggregate_Defined


#endif PgAggregateIncluded
