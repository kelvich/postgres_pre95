/* ----------------------------------------------------------------
 *   FILE
 *	pg_prs2plans.h
 *
 *   DESCRIPTION
 *	definition of the system "prs2plans" relation (pg_prs2plans)
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
#ifndef PgPrs2plansIncluded
#define PgPrs2plansIncluded 1	/* include this only once */

/* ----------------
 *	postgres.h contains the system type definintions and the
 *	CATALOG(), BOOTSTRAP and DATA() sugar words so this file
 *	can be read by both genbki.sh and the C compiler.
 * ----------------
 */
#include "postgres.h"

/* ----------------
 *	pg_prs2plans definition.  cpp turns this into
 *	typedef struct FormData_pg_prs2plans
 * ----------------
 */ 
CATALOG(pg_prs2plans) {
     oid 	prs2ruleid;
     int2 	prs2planno;
     text 	prs2code;	/* VARIABLE LENGTH FIELD */
} FormData_pg_prs2plans;

/* ----------------
 *	Form_pg_prs2plans corresponds to a pointer to a tuple with
 *	the format of pg_prs2plans relation.
 * ----------------
 */
typedef FormData_pg_prs2plans	*Form_pg_prs2plans;

/* ----------------
 *	compiler constants for pg_prs2plans
 * ----------------
 */
#define Name_pg_prs2plans		"pg_prs2plans"
#define Natts_pg_prs2plans		3
#define Anum_pg_prs2plans_prs2ruleid	1
#define Anum_pg_prs2plans_prs2planno	2
#define Anum_pg_prs2plans_prs2code	3

/* ----------------
 *	old definition of struct prs2plans
 * ----------------
 */
#ifndef struct_prs2plans_Defined
#define struct_prs2plans_Defined 1

struct	prs2plans {
	OID	prs2ruleid;
	uint16	prs2planno;
	struct	varlena prs2code;
}; /* VARIABLE LENGTH STRUCTURE */

#endif struct_prs2plans_Defined


/* ----------------
 *	old style compiler constants.  these are obsolete and
 *	should not be used -cim 6/17/90
 * ----------------
 */    
#define Prs2PlansRuleIdAttributeNumber \
    Anum_pg_prs2plans_prs2ruleid
#define Prs2PlansPlanNumberAttributeNumber \
    Anum_pg_prs2plans_prs2planno
#define Prs2PlansCodeAttributeNumber \
    Anum_pg_prs2plans_prs2code

#define Prs2PlansRelationNumberOfAttributes \
    Natts_pg_prs2plans
    
#endif PgPrs2plansIncluded
