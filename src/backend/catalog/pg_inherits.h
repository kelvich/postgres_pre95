/* ----------------------------------------------------------------
 *   FILE
 *	pg_inherits.h
 *
 *   DESCRIPTION
 *	definition of the system "inherits" relation (pg_inherits)
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
#ifndef PgInheritsIncluded
#define PgInheritsIncluded 1	/* include this only once */

/* ----------------
 *	catmacros.h defines the CATALOG(), BOOTSTRAP and
 *	DATA() sugar words so this file can be read by both
 *	genbki.sh and the C compiler.
 * ----------------
 */
#include "catalog/catmacros.h"

/* ----------------
 *	pg_inherits definition.  cpp turns this into
 *	typedef struct FormData_pg_inherits
 * ----------------
 */ 
CATALOG(pg_inherits) {
    oid 	inhrel;
    oid 	inhparent;
    int4 	inhseqno;
} FormData_pg_inherits;

/* ----------------
 *	Form_pg_inherits corresponds to a pointer to a tuple with
 *	the format of pg_inherits relation.
 * ----------------
 */
typedef FormData_pg_inherits	*Form_pg_inherits;

/* ----------------
 *	compiler constants for pg_inherits
 * ----------------
 */
#define Name_pg_inherits		"pg_inherits"
#define Natts_pg_inherits		3
#define Anum_pg_inherits_inhrel		1
#define Anum_pg_inherits_inhparent	2
#define Anum_pg_inherits_inhseqno	3

/* ----------------
 *	old definition of InheritsTupleForm
 * ----------------
 */
#ifndef InheritsTupleForm_Defined
#define InheritsTupleForm_Defined 1

typedef struct InheritsTupleFormD {
	ObjectId	inhrel;
	ObjectId	inhparent;
	int32		inhseqnum;	/* XXX uint32 better? */
} InheritsTupleFormD;

typedef InheritsTupleFormD	*InheritsTupleForm;

#endif InheritsTupleForm_Defined

/* ----------------
 *	old style compiler constants.  these are obsolete and
 *	should not be used -cim 6/17/90
 * ----------------
 */
#define	InheritsRelationIdAttributeNumber \
    Anum_pg_inherits_inhrel
#define	InheritsParentAttributeNumber \
    Anum_pg_inherits_inhparent
#define	InheritsSequenceNumberAttributeNumber \
    Anum_pg_inherits_inhseqno

#define InheritsRelationNumberOfAttributes \
    Natts_pg_inherits
    
#endif PgInheritsIncluded
