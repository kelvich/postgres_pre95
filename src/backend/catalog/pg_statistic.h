/* ----------------------------------------------------------------
 *   FILE
 *	pg_statistic.h
 *
 *   DESCRIPTION
 *	definition of the system "statistic" relation (pg_statistic)
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
#ifndef PgStatisticIncluded
#define PgStatisticIncluded 1	/* include this only once */

/* ----------------
 *	postgres.h contains the system type definintions and the
 *	CATALOG(), BOOTSTRAP and DATA() sugar words so this file
 *	can be read by both genbki.sh and the C compiler.
 * ----------------
 */
#include "tmp/postgres.h"

/* ----------------
 *	pg_statistic definition.  cpp turns this into
 *	typedef struct FormData_pg_statistic
 * ----------------
 */ 
CATALOG(pg_statistic) {
    oid 	starelid;
    int2 	staattnum;
    oid 	staop;
    text 	stalokey;	/* VARIABLE LENGTH FIELD */
    text 	stahikey;	/* VARIABLE LENGTH FIELD */
} FormData_pg_statistic;

/* ----------------
 *	Form_pg_statistic corresponds to a pointer to a tuple with
 *	the format of pg_statistic relation.
 * ----------------
 */
typedef FormData_pg_statistic	*Form_pg_statistic;

/* ----------------
 *	compiler constants for pg_statistic
 * ----------------
 */
#define Name_pg_statistic		"pg_statistic"
#define Natts_pg_statistic		5
#define Anum_pg_statistic_starelid	1
#define Anum_pg_statistic_staattnum	2
#define Anum_pg_statistic_staop		3
#define Anum_pg_statistic_stalokey	4
#define Anum_pg_statistic_stahikey	5

/* ----------------
 *	old definition of struct statistic
 * ----------------
 */
#ifndef struct_statistic_Defined
#define struct_statistic_Defined 1

struct	statistic {
	OID	starelid;
	uint16	staattnum;
	OID	staop;
	struct	varlena	stalokey;
/*	struct	varlena	stahikey; */
}; /* VARIABLE LENGTH STRUCTURE */

#endif struct_statistic_Defined

/* ----------------
 *	old style compiler constants.  these are obsolete and
 *	should not be used -cim 6/17/90
 * ----------------
 */
#define	StatisticRelationIdAttributeNumber \
    Anum_pg_statistic_starelid
#define	StatisticAttributeNumberAttributeNumber \
    Anum_pg_statistic_staattnum
#define	StatisticOperatorAttributeNumber \
    Anum_pg_statistic_staop
#define	StatisticLowKeyAttributeNumber \
    Anum_pg_statistic_stalokey
#define	StatisticHighKeyAttributeNumber \
    Anum_pg_statistic_stahikey

#endif PgStatisticIncluded
