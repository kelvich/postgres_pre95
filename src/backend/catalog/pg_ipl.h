/* ----------------------------------------------------------------
 *   FILE
 *	pg_ipl.h
 *
 *   DESCRIPTION
 *	definition of the system "ipl" relation (pg_ipl)
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
#ifndef PgIplIncluded
#define PgIplIncluded 1	/* include this only once */

/* ----------------
 *	catmacros.h defines the CATALOG(), BOOTSTRAP and
 *	DATA() sugar words so this file can be read by both
 *	genbki.sh and the C compiler.
 * ----------------
 */
#include "catalog/catmacros.h"

/* ----------------
 *	pg_ipl definition.  cpp turns this into
 *	typedef struct FormData_pg_ipl
 * ----------------
 */ 
CATALOG(pg_ipl) {
     oid 	iplrel;
     oid 	iplipl;
     int4 	iplseqno;
} FormData_pg_ipl;

/* ----------------
 *	Form_pg_ipl corresponds to a pointer to a tuple with
 *	the format of pg_ipl relation.
 * ----------------
 */
typedef FormData_pg_ipl	*Form_pg_ipl;

/* ----------------
 *	compiler constants for pg_ipl
 * ----------------
 */
#define Name_pg_ipl		"pg_ipl"
#define Natts_pg_ipl		3
#define Anum_pg_ipl_iplrel	1
#define Anum_pg_ipl_iplipl	2
#define Anum_pg_ipl_iplseqno	3

/* ----------------
 *	old definition of struct ipl
 * ----------------
 */
#ifndef struct_ipl_Defined
#define struct_ipl_Defined 1

struct	ipl {
	OID	ipdrel;
	OID	ipdinherits;
	int32	ipdseqnum;	/* XXX uint16 better? */
};
#endif struct_ipl_Defined

/* ----------------
 *	old style compiler constants.  these are obsolete and
 *	should not be used -cim 6/17/90
 * ----------------
 */
#define InheritancePrecidenceListRelationIdAttributeNumber \
    Anum_pg_ipl_iplrel
    
#define InheritancePrecidenceListRelationNumberOfAttributes \
    Natts_pg_ipl
    
#endif PgIplIncluded
