/* ----------------------------------------------------------------
 *   FILE
 *	pg_relation.h
 *
 *   DESCRIPTION
 *	definition of the system "relation" relation (pg_relation)
 *	along with the relation's initial contents.
 *
 *   NOTES
 *	``pg_relation'' is being replaced by ``pg_class''.  currently
 *	we are only changing the name in the catalogs but someday the
 *	code will be changed too. -cim 2/26/90
 *
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
 *	postgres.h contains the system type definintions and the
 *	CATALOG(), BOOTSTRAP and DATA() sugar words so this file
 *	can be read by both genbki.sh and the C compiler.
 * ----------------
 */
#include "tmp/postgres.h"

/* ----------------
 *	pg_relation definition.  cpp turns this into
 *	typedef struct FormData_pg_relation
 *
 *	Note: the #if 0, #endif around the BKI_BEGIN.. END block
 *	      below keeps cpp from seeing what is meant for the
 *	      genbki script: pg_relation is now called pg_class, but
 *	      only in the catalogs -cim 2/26/90
 * ----------------
 */
#if 0
BKI_BEGIN
#define pg_relation pg_class    
BKI_END    
#endif    
    
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
     int2	relsmgr;
     int28 	relkey;
     oid8	relkeyop;
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
/* pg_relation is now called pg_class -cim 2/26/90 */
#define Name_pg_relation		"pg_class"

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
#define Anum_pg_relation_relsmgr	13
#define Anum_pg_relation_relkey		14
#define Anum_pg_relation_relkeyop	15

/* ----------------
 *	initial contents of pg_relation
 * ----------------
 */

DATA(insert OID =  71 (  pg_type           PGUID 0 0 0 0 0 f f r n 15 "magnetic disk" - - ));
DATA(insert OID =  88 (  pg_database       PGUID 0 0 0 0 0 f t r n 3 "magnetic disk" - - ));
DATA(insert OID =  76 (  pg_demon          PGUID 0 0 0 0 0 f t r n 4 "magnetic disk" - - ));
DATA(insert OID =  81 (  pg_proc           PGUID 0 0 0 0 0 f f r n 16 "magnetic disk" - - ));
DATA(insert OID =  82 (  pg_server         PGUID 0 0 0 0 0 f t r n 3 "magnetic disk" - - ));
DATA(insert OID =  86 (  pg_user           PGUID 0 0 0 0 0 f t r n 6 "magnetic disk" - - ));
DATA(insert OID =  75 (  pg_attribute      PGUID 0 0 0 0 0 f f r n 13 "magnetic disk" - - ));
/* pg_relation is now called pg_class -cim 2/26/90 */
DATA(insert OID =  83 (  pg_class          PGUID 0 0 0 0 0 f f r n 15 "magnetic disk" - - ));
    
DATA(insert OID =  80 (  pg_magic          PGUID 0 0 0 0 0 f t r n 2 "magnetic disk" - - ));
DATA(insert OID =  89 (  pg_defaults       PGUID 0 0 0 0 0 f t r n 2 "magnetic disk" - - ));
DATA(insert OID =  90 (  pg_variable       PGUID 0 0 0 0 0 f t s n 2 "magnetic disk" - - ));
DATA(insert OID =  99 (  pg_log            PGUID 0 0 0 0 0 f t s n 1 "magnetic disk" - - ));
DATA(insert OID = 100 (  pg_time           PGUID 0 0 0 0 0 f t s n 1 "magnetic disk" - - ));

#define RelOid_pg_type		71
#define RelOid_pg_database    	88   
#define RelOid_pg_demon       	76   
#define RelOid_pg_proc       	81   
#define RelOid_pg_server     	82   
#define RelOid_pg_user       	86   
#define RelOid_pg_attribute  	75   
#define RelOid_pg_relation   	83   
#define RelOid_pg_magic   	80      
#define RelOid_pg_defaults  	89    
#define RelOid_pg_variable   	90   
#define RelOid_pg_log   	99       
#define RelOid_pg_time   	100      
    
/* ----------------
 *	XXX well known relation identifiers put here for now.
 *	these are obsolete, but they better match the above definitions.
 *	-cim 6/17/90
 * ----------------
 */
#define	AttributeRelationId	RelOid_pg_attribute
#define VariableRelationId	RelOid_pg_variable
    
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
	int2		relsmgr;
	AttributeNumber	relkey[8];
	ObjectId	relkeyop[8];
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
	int2	relsmgr;
	int16	relkey[8];
	OID	relkeyop[8];
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
#define RelationStubAttributeNumber \
    Anum_pg_relation_relstub

#define RelationRelationNumberOfAttributes \
    Natts_pg_relation
    
#endif PgRelationIncluded
