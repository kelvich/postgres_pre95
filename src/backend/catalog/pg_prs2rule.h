/* ----------------------------------------------------------------
 *   FILE
 *	pg_prs2rule.h
 *
 *   DESCRIPTION
 *	definition of the system "prs2rule" relation (pg_prs2rule)
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
#ifndef PgPrs2ruleIncluded
#define PgPrs2ruleIncluded 1	/* include this only once */

/* ----------------
 *	catmacros.h defines the CATALOG(), BOOTSTRAP and
 *	DATA() sugar words so this file can be read by both
 *	genbki.sh and the C compiler.
 * ----------------
 */
#include "catalog/catmacros.h"

/* ----------------
 *	pg_prs2rule definition.  cpp turns this into
 *	typedef struct FormData_pg_prs2rule
 * ----------------
 */ 
CATALOG(pg_prs2rule) {
    char16 	prs2name;
    char 	prs2eventtype;
    oid 	prs2eventrel;
    int2 	prs2eventattr;
    float4	necessary;
    float4	sufficient;
    text 	prs2text;	/* VARIABLE LENGTH FIELD */
} FormData_pg_prs2rule;

/* ----------------
 *	Form_pg_prs2rule corresponds to a pointer to a tuple with
 *	the format of pg_prs2rule relation.
 * ----------------
 */
typedef FormData_pg_prs2rule	*Form_pg_prs2rule;

/* ----------------
 *	compiler constants for pg_prs2rule
 * ----------------
 */
#define Name_pg_prs2rule		"pg_prs2rule"
#define Natts_pg_prs2rule		5
#define Anum_pg_prs2rule_prs2name	1
#define Anum_pg_prs2rule_prs2eventtype	2
#define Anum_pg_prs2rule_prs2eventrel	3
#define Anum_pg_prs2rule_prs2eventattr	4
#define Anum_pg_prs2rule_prs2text	5

/* ----------------
 *	old definition of struct prs2rule
 * ----------------
 */
#ifndef struct_prs2rule_Defined
#define struct_prs2rule_Defined 1

struct	prs2rule {
	char	prs2name[16];
	uint8	prs2eventtype;
	OID	prs2eventrel;
	uint16	prs2eventattr;
	struct	varlena prs2text;
}; /* VARIABLE LENGTH STRUCTURE */

#endif struct_prs2rule_Defined


/* ----------------
 *	old style compiler constants.  these are obsolete and
 *	should not be used -cim 6/17/90
 * ----------------
 */    
#define Prs2RuleNameAttributeNumber \
    Anum_pg_prs2rule_prs2name
#define Prs2RuleEventTypeAttributeNumber \
    Anum_pg_prs2rule_prs2eventtype
#define Prs2RuleEventTargetRelationAttributeNumber \
    Anum_pg_prs2rule_prs2eventrel
#define Prs2RuleEventTargetAttributeAttributeNumber \
    Anum_pg_prs2rule_prs2eventattr
#define Prs2RuleTextAttributeNumber \
    Anum_pg_prs2rule_prs2text

#define Prs2RuleRelationNumberOfAttributes \
    Natts_pg_prs2rule
    
#endif PgPrs2ruleIncluded
