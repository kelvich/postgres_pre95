/* ----------------------------------------------------------------
 *   FILE
 *	pg_index.h
 *
 *   DESCRIPTION
 *	definition of the system "index" relation (pg_index)
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
#ifndef PgIndexIncluded
#define PgIndexIncluded 1	/* include this only once */

/* ----------------
 *	postgres.h contains the system type definintions and the
 *	CATALOG(), BOOTSTRAP and DATA() sugar words so this file
 *	can be read by both genbki.sh and the C compiler.
 * ----------------
 */
#include "tmp/postgres.h"

/* ----------------
 *	pg_index definition.  cpp turns this into
 *	typedef struct FormData_pg_index.  The oid of the index relation
 *	is stored in indexrelid; the oid of the indexed relation is stored
 *	in indrelid.
 * ----------------
 */ 
CATALOG(pg_index) {
    oid 	indexrelid;
    oid 	indrelid;
    int28 	indkey;
    oid8 	indclass;
    bool 	indisclustered;
    bool 	indisarchived;
} FormData_pg_index;

/* ----------------
 *	Form_pg_index corresponds to a pointer to a tuple with
 *	the format of pg_index relation.
 * ----------------
 */
typedef FormData_pg_index	*Form_pg_index;

/* ----------------
 *	compiler constants for pg_index
 * ----------------
 */
#define Name_pg_index			"pg_index"
#define Natts_pg_index			6
#define Anum_pg_index_indexrelid	1
#define Anum_pg_index_indrelid		2
#define Anum_pg_index_indkey		3
#define Anum_pg_index_indclass		4
#define Anum_pg_index_indisclustered	5
#define Anum_pg_index_indisarchived	6

/* ----------------
 *	old definition of IndexTupleForm
 * ----------------
 */
#ifndef IndexTupleForm_Defined
#define IndexTupleForm_Defined 1

typedef struct IndexTupleFormD {
	ObjectId	indexrelid;
	ObjectId	indrelid;
	AttributeNumber	indkey[8];
	ObjectId	indclass[8];
	Boolean		indisclustered;
	Boolean		indisarchived;
/*	SPQUEL	inddesc; */
} IndexTupleFormD;

typedef IndexTupleFormD		*IndexTupleForm;
#define IndexTupleFormData	IndexTupleFormD

#endif IndexTupleForm_Defined

/* ----------------
 *	old definition of struct index
 * ----------------
 */
#ifndef struct_index_Defined
#define struct_index_Defined 1

struct	index {
	OID	indexrelid;
	OID	indrelid;
	int16	indkey[8];
	OID	indclass[8];
	Boolean	indisclustered;
	Boolean	indisarchived;
/*	SPQUEL	inddesc; */
};
#endif struct_index_Defined


/* ----------------
 *	old style compiler constants.  these are obsolete and
 *	should not be used -cim 6/17/90
 * ----------------
 */
#define IndexRelationIdAttributeNumber \
    Anum_pg_index_indexrelid
#define IndexHeapRelationIdAttributeNumber \
    Anum_pg_index_indrelid
#define IndexKeyAttributeNumber \
    Anum_pg_index_indkey
#define IndexIsArchivedAttributeNumber \
    Anum_pg_index_indisarchived

#define IndexRelationNumberOfAttributes	\
    Natts_pg_index
    
#endif PgIndexIncluded
