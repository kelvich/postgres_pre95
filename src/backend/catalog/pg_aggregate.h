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
 *  xitionfunc1		transition function 1
 *  xitionfunc2		transition function 2
 *  finalfunc		final function - produces the aggregate value
 *  inttype		output type for xition funcs
 *  fintype		output type for final func
 *  initaggval		initial aggregate value
 *  initsecval		initial value for transition state 2
 * ----------------------------------------------------------------
 */ 
CATALOG(pg_aggregate) {
    char16 		aggname;
    regproc	 	xitionfunc1;
    regproc		xitionfunc2;
    regproc 		finalfunc;
    oid			inttype;
    oid			fintype;
    text		initaggval;	/* VARIABLE LENGTH FIELD */
    text		initsecval;	/* VARIABLE LENGTH FIELD */
} FormData_pg_aggregate;

/* ----------------
 *	Form_pg_aggregate corresponds to a pointer to a tuple with
 *	the format of pg_aggregate relation.
 * ----------------
 */
typedef FormData_pg_aggregate	*Form_pg_aggregate;

#define AggregateTupleFormData FormData_pg_aggregate;

/* ----------------
 *	compiler constants for pg_aggregate
 * ----------------
 */
#define Name_pg_aggregate		"pg_aggregate"
#define Natts_pg_aggregate		8
#define Anum_pg_aggregate_aggname	1
#define Anum_pg_aggregate_xitionfunc1	2
#define Anum_pg_aggregate_xitionfunc2	3
#define Anum_pg_aggregate_finalfunc	4
#define Anum_pg_aggregate_inttype	5
#define Anum_pg_aggregate_fintype	6
#define Anum_pg_aggregate_initaggval	7
#define Anum_pg_aggregate_initsecval	8


/* ----------------
 * initial contents of pg_aggregate
 * ---------------
 */

DATA(insert OID = 0 ( int4ave   int4pl  int4inc  int4div  23  23 0  0 ));
DATA(insert OID = 0 ( int2ave   int2pl  int2inc  int2div  21  21 0  0 ));
DATA(insert OID = 0 ( float4ave float4pl float4inc float4div 700  700 0.0 0.0 ));
DATA(insert OID = 0 ( float8ave float8pl float8inc float8div 701  701 0.0 0.0 ));

DATA(insert OID = 0 ( int4sum   int4pl   - -  23  23  0   _null_ ));
DATA(insert OID = 0 ( int2sum   int2pl   - -  21  21  0   _null_ ));
DATA(insert OID = 0 ( float4sum float4pl - - 700  700 0.0 _null_ ));
DATA(insert OID = 0 ( float8sum float8pl - - 701  701 0.0 _null_ ));

DATA(insert OID = 0 ( int4max   int4larger   - -  23  23  _null_ _null_ ));
DATA(insert OID = 0 ( int2max   int2larger   - -  21  21  _null_ _null_ ));
DATA(insert OID = 0 ( float4max float4larger - - 700  700 _null_ _null_ ));
DATA(insert OID = 0 ( float8max float8larger - - 701  701 _null_ _null_ ));

DATA(insert OID = 0 ( int4min   int4smaller   - -  23  23  _null_ _null_ ));
DATA(insert OID = 0 ( int2min   int2smaller   - -  21  21  _null_ _null_ ));
DATA(insert OID = 0 ( float4min float4smaller - - 700  700 _null_ _null_ ));
DATA(insert OID = 0 ( float8min float8smaller - - 701  701 _null_ _null_ ));

DATA(insert OID = 0 ( count     int4inc  - -  23  23  0   _null_ ));

/* ----------------
 *	old definition of struct aggregate
 * ----------------
 */
#ifndef struct_aggregate_Defined
#define struct_aggregate_Defined 1

struct	aggregate {
	char	aggname[16];
	OID	xitionfunc1;
	OID	xitionfunc2;
	OID	finalfunc;
	OID 	inttype;
	OID 	fintype;
	Datum   initaggval;
	Datum   initsecval;
}
;
#endif struct_aggregate_Defined

#define AggregateNameAttributeNumber \
   Anum_pg_aggregate_aggname
#define AggregateIntFunc1AttributeNumber \
   Anum_pg_aggregate_xitionfunc1
#define AggregateIntFunc2AttributeNumber \
   Anum_pg_aggregate_xitionfunc2
#define AggregateFinFuncAttributeNumber \
   Anum_pg_aggregate_finalfunc
#define AggregateRelationNumberOfAttributes \
   Natts_pg_aggregate
#define AggregateInternalTypeAttributeNumber \
   Anum_pg_aggregate_inttype
#define AggregateFinalTypeAttributeNumber \
   Anum_pg_aggregate_fintype


#endif PgAggregateIncluded
