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
    char16 		aggname;
    oid 		aggowner;
    regproc	 	xitionfunc1;	
    regproc		xitionfunc2;
    regproc 		finalfunc;	
    oid			inttype;
    oid			fintype;
    int4		initaggval;
    int4		initsecval;
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
#define Natts_pg_aggregate		9
#define Anum_pg_aggregate_aggname	1
#define Anum_pg_aggregate_aggowner	2
#define Anum_pg_aggregate_xitionfunc1	3
#define Anum_pg_aggregate_xitionfunc2	4
#define Anum_pg_aggregate_finalfunc	5
#define Anum_pg_aggregate_inttype	6
#define Anum_pg_aggregate_fintype	7
#define Anum_pg_aggregate_initaggval	8
#define Anum_pg_aggregate_initsecval	9


/* ----------------
 * initial contents of pg_aggregate
 * ---------------
 */

DATA(insert OID = 1028 ( int4ave   6  int4pl  int4inc  int4div  23  23 0  0));
DATA(insert OID = 1029 ( int2ave   6  int2pl  int2inc  int2div  21  21 0  0));

/* ----------------
 *	old definition of struct aggregate
 * ----------------
 */
#ifndef struct_aggregate_Defined
#define struct_aggregate_Defined 1

struct	aggregate {
	char	aggname[16];
	OID	aggowner;
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

#define AggNameGetInitAggVal(x) CInteger(SearchSysCacheGetAttribute(AGGNAME, \
	Anum_pg_aggregate_initaggval, x, 0, 0, 0))

#define AggNameGetInitSecVal(x) CInteger(SearchSysCacheGetAttribute(AGGNAME, \
	Anum_pg_aggregate_initsecval, x, 0, 0, 0))

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
