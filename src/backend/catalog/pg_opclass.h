/* ----------------------------------------------------------------
 *   FILE
 *	pg_opclass.h
 *
 *   DESCRIPTION
 *	definition of the system "opclass" relation (pg_opclass)
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
#ifndef PgOpclassIncluded
#define PgOpclassIncluded 1	/* include this only once */

/* ----------------
 *	postgres.h contains the system type definintions and the
 *	CATALOG(), BOOTSTRAP and DATA() sugar words so this file
 *	can be read by both genbki.sh and the C compiler.
 * ----------------
 */
#include "tmp/postgres.h"

/* ----------------
 *	pg_opclass definition.  cpp turns this into
 *	typedef struct FormData_pg_opclass
 * ----------------
 */ 

CATALOG(pg_opclass) {
    char16 opcname;
} FormData_pg_opclass;

/* ----------------
 *	Form_pg_opclass corresponds to a pointer to a tuple with
 *	the format of pg_opclass relation.
 * ----------------
 */
typedef FormData_pg_opclass	*Form_pg_opclass;

/* ----------------
 *	compiler constants for pg_opclass
 * ----------------
 */
#define Name_pg_opclass			"pg_opclass"
#define Natts_pg_opclass		1
#define Anum_pg_opclass_opcname		1

/* ----------------
 *	initial contents of pg_opclass
 * ----------------
 */

DATA(insert OID = 421 (    int2_ops ));
DATA(insert OID = 422 (    box_ops ));
DATA(insert OID = 423 (    float8_ops ));
DATA(insert OID = 424 (    int24_ops ));
DATA(insert OID = 425 (    int42_ops ));
DATA(insert OID = 426 (    int4_ops ));
DATA(insert OID = 427 (    oid_ops ));
DATA(insert OID = 428 (    float4_ops ));
DATA(insert OID = 429 (    char_ops ));
DATA(insert OID = 430 (    char16_ops ));
DATA(insert OID = 431 (    text_ops ));
DATA(insert OID = 432 (    abstime_ops ));
DATA(insert OID = 433 (    bigbox_ops));
DATA(insert OID = 434 (    poly_ops));

/* ----------------
 *	old definition of struct opclass
 * ----------------
 */
#ifndef struct_opclass_Defined
#define struct_opclass_Defined 1

struct	opclass {
	char	opcname[16];
};
#endif struct_opclass_Defined

    
/* ----------------
 *	old style compiler constants.  these are obsolete and
 *	should not be used -cim 6/17/90
 * ----------------
 */    
#define OperatorClassNameAttributeNumber \
    Anum_pg_opclass_opcname
    
#endif PgOpclassIncluded
