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
 *
 *  aggname		Name of the aggregate
 *  aggtransfn1		transition function 1
 *  aggtransfn2		transition function 2
 *  aggfinalfn		final function
 *  aggtranstype	output type for xition funcs
 *  aggfinaltype	output type for final func
 *  agginitval1		initial aggregate value
 *  agginitval2		initial value for transition state 2
 * ----------------------------------------------------------------
 */ 
CATALOG(pg_aggregate) {
    char16 		aggname;
    oid			aggowner;
    regproc	 	aggtransfn1;
    regproc		aggtransfn2;
    regproc 		aggfinalfn;
    oid			aggtranstype;
    oid			aggfinaltype;
    text		agginitval1;	/* VARIABLE LENGTH FIELD */
    text		agginitval2;	/* VARIABLE LENGTH FIELD */
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
#define Natts_pg_aggregate		9
#define Anum_pg_aggregate_aggname	1
#define Anum_pg_aggregate_aggowner	2
#define Anum_pg_aggregate_aggtransfn1	3
#define Anum_pg_aggregate_aggtransfn2	4
#define Anum_pg_aggregate_aggfinalfn	5
#define Anum_pg_aggregate_aggtranstype	6
#define Anum_pg_aggregate_aggfinaltype	7
#define Anum_pg_aggregate_agginitval1	8
#define Anum_pg_aggregate_agginitval2	9


/* ----------------
 * initial contents of pg_aggregate
 * ---------------
 */

DATA(insert OID = 0 ( int4ave   PGUID int4pl  int4inc  int4div  23  23 0  0 ));
DATA(insert OID = 0 ( int2ave   PGUID int2pl  int2inc  int2div  21  21 0  0 ));
DATA(insert OID = 0 ( float4ave PGUID float4pl float4inc float4div 700  700 0.0 0.0 ));
DATA(insert OID = 0 ( float8ave PGUID float8pl float8inc float8div 701  701 0.0 0.0 ));

DATA(insert OID = 0 ( int4sum   PGUID int4pl   - -  23  23  0   _null_ ));
DATA(insert OID = 0 ( int2sum   PGUID int2pl   - -  21  21  0   _null_ ));
DATA(insert OID = 0 ( float4sum PGUID float4pl - - 700  700 0.0 _null_ ));
DATA(insert OID = 0 ( float8sum PGUID float8pl - - 701  701 0.0 _null_ ));

DATA(insert OID = 0 ( int4max   PGUID int4larger   - -  23  23  _null_ _null_ ));
DATA(insert OID = 0 ( int2max   PGUID int2larger   - -  21  21  _null_ _null_ ));
DATA(insert OID = 0 ( float4max PGUID float4larger - - 700  700 _null_ _null_ ));
DATA(insert OID = 0 ( float8max PGUID float8larger - - 701  701 _null_ _null_ ));

DATA(insert OID = 0 ( int4min   PGUID int4smaller   - -  23  23  _null_ _null_ ));
DATA(insert OID = 0 ( int2min   PGUID int2smaller   - -  21  21  _null_ _null_ ));
DATA(insert OID = 0 ( float4min PGUID float4smaller - - 700  700 _null_ _null_ ));
DATA(insert OID = 0 ( float8min PGUID float8smaller - - 701  701 _null_ _null_ ));

DATA(insert OID = 0 ( count     PGUID - int4inc - 23 23 _null_ 0 ));

#endif PgAggregateIncluded
