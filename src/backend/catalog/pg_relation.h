/* ----------------------------------------------------------------
 *   FILE
 *	pg_relation.h
 *
 *   DESCRIPTION
 *	definition of the system "relation" relation (pg_relation)
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
#ifndef PgRelationIncluded
#define PgRelationIncluded 1	/* include this only once */

/* ----------------
 *	catmacros.h defines the CATALOG(), BOOTSTRAP and
 *	DATA() sugar words so this file can be read by both
 *	genbki.sh and the C compiler.
 * ----------------
 */
#include "catalog/catmacros.h"

/* ----------------
 *	pg_relation definition.  cpp turns this into
 *	typedef struct FormData_pg_relation
 * ----------------
 */
CATALOG(pg_relation) BOOTSTRAP {
     char16 	relname;
     oid 	relowner;
     oid 	relam;
     int4 	relpages;
     int4 	reltuples;
     dt 	relexpires;
     dt 	relpreserved;
     bool 	relhasindex;
     bool 	relisshared;
     char 	relkind;
     char 	relarch;
     int2 	relnatts;
     int28 	relkey;
     oid8	relkeyop;
     stub	relstub;
} FormData_pg_relation;

/* ----------------
 *	Form_pg_relation corresponds to a pointer to a tuple with
 *	the format of pg_relation relation.
 * ----------------
 */
typedef FormData_pg_relation	*Form_pg_relation;

/* ----------------
 *	compiler constants for pg_relation
 * ----------------
 */
#define Name_pg_relation		"pg_relation"
#define Natts_pg_relation		15
#define Anum_pg_relation_relname	1
#define Anum_pg_relation_relowner	2
#define Anum_pg_relation_relam		3
#define Anum_pg_relation_relpages	4
#define Anum_pg_relation_reltuples	5
#define Anum_pg_relation_relexpires	6
#define Anum_pg_relation_relpreserved	7
#define Anum_pg_relation_relhasindex	8
#define Anum_pg_relation_relisshared	9
#define Anum_pg_relation_relkind	10
#define Anum_pg_relation_relarch	11
#define Anum_pg_relation_relnatts	12
#define Anum_pg_relation_relkey		13
#define Anum_pg_relation_relkeyop	14
#define Anum_pg_relation_relstub	15

/* ----------------
 *	initial contents of pg_relation
 * ----------------
 */

DATA(insert OID =  71 (  pg_type           6 0 0 0 0 0 f f r n 14 - - - ));
DATA(insert OID =  88 (  pg_database       6 0 0 0 0 0 f t r n 3 - - - ));
DATA(insert OID =  76 (  pg_demon          6 0 0 0 0 0 f t r n 4 - - - ));
DATA(insert OID =  81 (  pg_proc           6 0 0 0 0 0 f f r n 10 - - - ));
DATA(insert OID =  82 (  pg_server         6 0 0 0 0 0 f t r n 3 - - - ));
DATA(insert OID =  86 (  pg_user           6 0 0 0 0 0 f t r n 6 - - - ));
DATA(insert OID =  75 (  pg_attribute      6 0 0 0 0 0 f f r n 12 - - - ));
DATA(insert OID =  83 (  pg_relation       6 0 0 0 0 0 f f r n 15 - - - ));
DATA(insert OID =  80 (  pg_magic          6 0 0 0 0 0 f t r n 2 - - - ));
DATA(insert OID =  89 (  pg_defaults       6 0 0 0 0 0 f t r n 2 - - - ));
DATA(insert OID =  90 (  pg_variable       6 0 0 0 0 0 f t s n 2 - - - ));
DATA(insert OID =  99 (  pg_log            6 0 0 0 0 0 f t s n 1 - - - ));
DATA(insert OID = 100 (  pg_time           6 0 0 0 0 0 f t s n 1 - - - ));

/* ----------------
 *	XXX well known relation identifiers put here for now.
 *	these are obsolete, but they better match the above definitions.
 *	-cim 6/17/90
 * ----------------
 */
#define	AttributeRelationId	75
#define VariableRelationId	90
    
/* ----------------
 *	old definition of RelationTupleForm
 * ----------------
 */
#ifndef RelationTupleForm_Defined
#define RelationTupleForm_Defined 1

typedef struct RelationTupleFormD {
	NameData	relname;
	ObjectId	relowner;
	ObjectId	relam;
	uint32		relpages;
	uint32		reltuples;
	ABSTIME		relexpires;
	RELTIME		relpreserved;
	Boolean		relhasindex;
	Boolean		relisshared;
	char		relkind;
	char		relarch;
	AttributeNumber	relnatts;
	AttributeNumber	relkey[8];
	ObjectId	relkeyop[8];
	struct varlena	relstub;
/*	LOCK	rellock; */
/*	SPQUEL	reldesc; */
} RelationTupleFormD;

typedef RelationTupleFormD	*RelationTupleForm;

#endif RelationTupleForm_Defined

/* ----------------
 *	old definition of struct relation
 * ----------------
 */
#ifndef struct_relation_Defined
#define struct_relation_Defined 1

struct	relation {
	char	relname[16];
	OID	relowner;
	OID	relam;
	uint32	relpages;
	uint32	reltuples;
	ABSTIME	relexpires;
	RELTIME	relpreserved;
	Boolean	relhasindex;
	Boolean	relisshared;
	char	relkind;
	char	relarch;
	uint16	relnatts;
	int16	relkey[8];
	OID	relkeyop[8];
	struct varlena	relstub;
/*	LOCK	rellock; */
/*	SPQUEL	reldesc; */
};

#endif struct_relation_Defined

    
/* ----------------
 *	old style compiler constants.  these are obsolete and
 *	should not be used -cim 6/17/90
 * ----------------
 */
#define RelationNameAttributeNumber \
    Anum_pg_relation_relname
#define RelationPagesAttributeNumber \
    Anum_pg_relation_relpages
#define RelationTuplesAttributeNumber \
    Anum_pg_relation_reltuples
#define	RelationExpiresAttributeNumber \
    Anum_pg_relation_relexpires
#define	RelationPreservedAttributeNumber \
    Anum_pg_relation_relpreserved
#define RelationHasIndexAttributeNumber \
    Anum_pg_relation_relhasindex
#define e RelationStubAttributeNumber \
    Anum_pg_relation_relstub

#define RelationRelationNumberOfAttributes \
    Natts_pg_relation
    
#endif PgRelationIncluded
