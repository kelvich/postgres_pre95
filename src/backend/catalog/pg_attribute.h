/* ----------------------------------------------------------------
 *   FILE
 *	pg_attribute.h
 *
 *   DESCRIPTION
 *	definition of the system "attribute" relation (pg_attribute)
 *	along with the relation's initial contents.
 *
 *   NOTES
 *	the genbki.sh script reads this file and generates .bki
 *	information from the DATA() statements.
 *
 *	utils/cache/relcache.c requires some hard-coded tuple descriptors
 *	for some of the system catalogs so if the schema for any of
 *	these changes, be sure and change the appropriate Schema_xxx
 *	macros!  -cim 2/5/91
 *
 *	fastgetattr() now uses attcacheoff to cache byte offsets of
 *	attributes in heap tuples.  The data actually stored in 
 *	pg_attribute (-1) indicates no cached value.  But when we copy
 *	these tuples into a tuple descriptor, we may then update attcacheoff
 *	in the copies.  This speeds up the attribute walking process.
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */
#ifndef PgAttributeIncluded
#define PgAttributeIncluded 1	/* include this only once */
   
/* ----------------
 *	postgres.h contains the system type definintions and the
 *	CATALOG(), BOOTSTRAP and DATA() sugar words so this file
 *	can be read by both genbki.sh and the C compiler.
 * ----------------
 */
#include "tmp/postgres.h"

/* ----------------
 *	pg_attribute definition.  cpp turns this into
 *	typedef struct FormData_pg_attribute
 * ----------------
 */
CATALOG(pg_attribute) BOOTSTRAP {
    oid  	attrelid;
    char16  	attname;
    oid  	atttypid;
    oid  	attdefrel;
    int4  	attnvals;
    oid  	atttyparg;
    int2 	attlen;
    int2  	attnum;
    int2 	attbound;
    bool  	attbyval;
    bool 	attcanindex;
    oid 	attproc;
    int4	attnelems;
    int4	attcacheoff;
} FormData_pg_attribute;

/* ----------------
 *	Form_pg_attribute corresponds to a pointer to a tuple with
 *	the format of pg_attribute relation.
 * ----------------
 */
typedef FormData_pg_attribute	*Form_pg_attribute;

/* ----------------
 *	compiler constants for pg_attribute
 * ----------------
 */
#define Name_pg_attribute		"pg_attribute"
#define Natts_pg_attribute		14
#define Anum_pg_attribute_attrelid	1
#define Anum_pg_attribute_attname	2
#define Anum_pg_attribute_atttypid	3
#define Anum_pg_attribute_attdefrel	4
#define Anum_pg_attribute_attnvals	5
#define Anum_pg_attribute_atttyparg	6
#define Anum_pg_attribute_attlen	7
#define Anum_pg_attribute_attnum	8
#define Anum_pg_attribute_attbound	9
#define Anum_pg_attribute_attbyval	10
#define Anum_pg_attribute_attcanindex	11
#define Anum_pg_attribute_attproc	12
#define Anum_pg_attribute_attnelems	13
#define Anum_pg_attribute_attcacheoff	14


/* ----------------
 *	SCHEMA_ macros for declaring hardcoded tuple descriptors.
 *	these are used in utils/cache/relcache.c
 * ----------------
 */
#define SCHEMA_NAME(x) CppConcat(Name_,x)
#define SCHEMA_DESC(x) CppConcat(Desc_,x)
#define SCHEMA_NATTS(x) CppConcat(Natts_,x)
#define SCHEMA_DEF(x) \
    AttributeTupleFormData \
    SCHEMA_DESC(x) [ SCHEMA_NATTS(x) ] = \
    { \
	CppConcat(Schema_,x) \
    }

/* ----------------
 *	bki base attribute definitions
 * ----------------
 */
#if 0
BKI_BEGIN
#define SYSTEMATTS(OID) \
insert OID = 0 (  OID ctid             27 0 0 0  6  -1 0 f t 0 0 -1) \
insert OID = 0 (  OID lock             31 0 0 0 -1  -2 0 f t 0 0 -1) \
insert OID = 0 (  OID oid              26 0 0 0  4  -3 0 t t 0 0 -1) \
insert OID = 0 (  OID xmin             28 0 0 0  5  -4 0 f t 0 0 -1) \
insert OID = 0 (  OID cmin             29 0 0 0  1  -5 0 t t 0 0 -1) \
insert OID = 0 (  OID xmax             28 0 0 0  5  -6 0 f t 0 0 -1) \
insert OID = 0 (  OID cmax             29 0 0 0  1  -7 0 t t 0 0 -1) \
insert OID = 0 (  OID chain            27 0 0 0  6  -8 0 f t 0 0 -1) \
insert OID = 0 (  OID anchor           27 0 0 0  6  -9 0 f t 0 0 -1) \
insert OID = 0 (  OID tmax            702 0 0 0  4 -10 0 t t 0 0 -1) \
insert OID = 0 (  OID tmin            702 0 0 0  4 -11 0 t t 0 0 -1) \
insert OID = 0 (  OID vtype            18 0 0 0  1 -12 0 t t 0 0 -1)
BKI_END
#endif    
    
/* ----------------
 *	initial contents of pg_attribute
 * ----------------
 */

/* ----------------
 *	pg_type schema
 * ----------------
 */
#define Schema_pg_type \
{ 71l, "typname",      19l, 71l, 0l, 0l, 16,  1, 0,   '\0', '\001', 0l, 0l, -1l }, \
{ 71l, "typowner",     26l, 71l, 0l, 0l,  4,  2, 0, '\001', '\001', 0l, 0l, -1l }, \
{ 71l, "typlen",       21l, 71l, 0l, 0l,  2,  3, 0, '\001', '\001', 0l, 0l, -1l }, \
{ 71l, "typprtlen",    21l, 71l, 0l, 0l,  2,  4, 0, '\001', '\001', 0l, 0l, -1l }, \
{ 71l, "typbyval",     16l, 71l, 0l, 0l,  1,  5, 0, '\001', '\001', 0l, 0l, -1l }, \
{ 71l, "typtype",      16l, 71l, 0l, 0l,  1,  6, 0, '\001', '\001', 0l, 0l, -1l }, \
{ 71l, "typisdefined", 16l, 71l, 0l, 0l,  1,  7, 0, '\001', '\001', 0l, 0l, -1l }, \
{ 71l, "typdelim",     16l, 71l, 0l, 0l,  1,  8, 0, '\001', '\001', 0l, 0l, -1l }, \
{ 71l, "typrelid",     26l, 71l, 0l, 0l,  4,  9, 0, '\001', '\001', 0l, 0l, -1l }, \
{ 71l, "typelem",      26l, 71l, 0l, 0l,  4, 10, 0, '\001', '\001', 0l, 0l, -1l }, \
{ 71l, "typinput",     24l, 71l, 0l, 0l,  4, 11, 0, '\001', '\001', 0l, 0l, -1l }, \
{ 71l, "typoutput",    24l, 71l, 0l, 0l,  4, 12, 0, '\001', '\001', 0l, 0l, -1l }, \
{ 71l, "typreceive",   24l, 71l, 0l, 0l,  4, 13, 0, '\001', '\001', 0l, 0l, -1l }, \
{ 71l, "typsend",      24l, 71l, 0l, 0l,  4, 14, 0, '\001', '\001', 0l, 0l, -1l }, \
{ 71l, "typdefault",   25l, 71l, 0l, 0l, -1, 15, 0,   '\0', '\001', 0l, 0l, -1l }

DATA(insert OID = 0 (  71 typname          19 0 0 0 16   1 0 f t 0 0 -1));
DATA(insert OID = 0 (  71 typowner         26 0 0 0  4   2 0 t t 0 0 -1));
DATA(insert OID = 0 (  71 typlen           21 0 0 0  2   3 0 t t 0 0 -1));
DATA(insert OID = 0 (  71 typprtlen        21 0 0 0  2   4 0 t t 0 0 -1));
DATA(insert OID = 0 (  71 typbyval         16 0 0 0  1   5 0 t t 0 0 -1));
DATA(insert OID = 0 (  71 typtype          18 0 0 0  1   6 0 t t 0 0 -1));
DATA(insert OID = 0 (  71 typisdefined     16 0 0 0  1   7 0 t t 0 0 -1));
DATA(insert OID = 0 (  71 typdelim         18 0 0 0  1   8 0 t t 0 0 -1));
DATA(insert OID = 0 (  71 typrelid         26 0 0 0  4   9 0 t t 0 0 -1));
DATA(insert OID = 0 (  71 typelem          26 0 0 0  4  10 0 t t 0 0 -1));
DATA(insert OID = 0 (  71 typinput         26 0 0 0  4  11 0 t t 0 0 -1));
DATA(insert OID = 0 (  71 typoutput        26 0 0 0  4  12 0 t t 0 0 -1));
DATA(insert OID = 0 (  71 typrecieve       26 0 0 0  4  13 0 t t 0 0 -1));
DATA(insert OID = 0 (  71 typsend          26 0 0 0  4  14 0 t t 0 0 -1));
DATA(insert OID = 0 (  71 typdefault       25 0 0 0 -1  15 0 f t 0 0 -1));
DATA(insert OID = 0 (  71 ctid             27 0 0 0  6  -1 0 f t 0 0 -1));
DATA(insert OID = 0 (  71 lock             31 0 0 0 -1  -2 0 f t 0 0 -1));
DATA(insert OID = 0 (  71 oid              26 0 0 0  4  -3 0 t t 0 0 -1));
DATA(insert OID = 0 (  71 xmin             28 0 0 0  5  -4 0 f t 0 0 -1));
DATA(insert OID = 0 (  71 cmin             29 0 0 0  1  -5 0 t t 0 0 -1));
DATA(insert OID = 0 (  71 xmax             28 0 0 0  5  -6 0 f t 0 0 -1));
DATA(insert OID = 0 (  71 cmax             29 0 0 0  1  -7 0 t t 0 0 -1));
DATA(insert OID = 0 (  71 chain            27 0 0 0  6  -8 0 f t 0 0 -1));
DATA(insert OID = 0 (  71 anchor           27 0 0 0  6  -9 0 f t 0 0 -1));
DATA(insert OID = 0 (  71 tmax            702 0 0 0  4 -10 0 t t 0 0 -1));
DATA(insert OID = 0 (  71 tmin            702 0 0 0  4 -11 0 t t 0 0 -1));
DATA(insert OID = 0 (  71 vtype            18 0 0 0  1 -12 0 t t 0 0 -1));

/* ----------------
 *	pg_database
 * ----------------
 */
DATA(insert OID = 0 (  88 datname          19 0 0 0 16   1 0 f t 0 0 -1));
DATA(insert OID = 0 (  88 datdba           26 0 0 0  4   2 0 t t 0 0 -1));
DATA(insert OID = 0 (  88 datpath          25 0 0 0 -1   3 0 f t 0 0 -1));
DATA(insert OID = 0 (  88 ctid             27 0 0 0  6  -1 0 f t 0 0 -1));
DATA(insert OID = 0 (  88 lock             31 0 0 0 -1  -2 0 f t 0 0 -1));
DATA(insert OID = 0 (  88 oid              26 0 0 0  4  -3 0 t t 0 0 -1));
DATA(insert OID = 0 (  88 xmin             28 0 0 0  5  -4 0 f t 0 0 -1));
DATA(insert OID = 0 (  88 cmin             29 0 0 0  1  -5 0 t t 0 0 -1));
DATA(insert OID = 0 (  88 xmax             28 0 0 0  5  -6 0 f t 0 0 -1));
DATA(insert OID = 0 (  88 cmax             29 0 0 0  1  -7 0 t t 0 0 -1));
DATA(insert OID = 0 (  88 chain            27 0 0 0  6  -8 0 f t 0 0 -1));
DATA(insert OID = 0 (  88 anchor           27 0 0 0  6  -9 0 f t 0 0 -1));
DATA(insert OID = 0 (  88 tmax            702 0 0 0  4 -10 0 t t 0 0 -1));
DATA(insert OID = 0 (  88 tmin            702 0 0 0  4 -11 0 t t 0 0 -1));
DATA(insert OID = 0 (  88 vtype            18 0 0 0  1 -12 0 t t 0 0 -1));
    
/* ----------------
 *	pg_demon
 * ----------------
 */
DATA(insert OID = 0 (  76 demserid         26 0 0 0  4   1 0 t t 0 0 -1));
DATA(insert OID = 0 (  76 demname          19 0 0 0 16   2 0 f t 0 0 -1));
DATA(insert OID = 0 (  76 demowner         26 0 0 0  4   3 0 t t 0 0 -1));
DATA(insert OID = 0 (  76 demcode          24 0 0 0  4   4 0 t t 0 0 -1));
DATA(insert OID = 0 (  76 ctid             27 0 0 0  6  -1 0 f t 0 0 -1));
DATA(insert OID = 0 (  76 lock             31 0 0 0 -1  -2 0 f t 0 0 -1));
DATA(insert OID = 0 (  76 oid              26 0 0 0  4  -3 0 t t 0 0 -1));
DATA(insert OID = 0 (  76 xmin             28 0 0 0  5  -4 0 f t 0 0 -1));
DATA(insert OID = 0 (  76 cmin             29 0 0 0  1  -5 0 t t 0 0 -1));
DATA(insert OID = 0 (  76 xmax             28 0 0 0  5  -6 0 f t 0 0 -1));
DATA(insert OID = 0 (  76 cmax             29 0 0 0  1  -7 0 t t 0 0 -1));
DATA(insert OID = 0 (  76 chain            27 0 0 0  6  -8 0 f t 0 0 -1));
DATA(insert OID = 0 (  76 anchor           27 0 0 0  6  -9 0 f t 0 0 -1));
DATA(insert OID = 0 (  76 tmax            702 0 0 0  4 -10 0 t t 0 0 -1));
DATA(insert OID = 0 (  76 tmin            702 0 0 0  4 -11 0 t t 0 0 -1));
DATA(insert OID = 0 (  76 vtype            18 0 0 0  1 -12 0 t t 0 0 -1));
    
/* ----------------
 *	pg_proc
 * ----------------
 */
#define Schema_pg_proc \
{ 81l, "proname",       19l, 81l, 0l, 0l, 16,  1, 0,   '\0', '\001', 0l, 0l, -1l }, \
{ 81l, "proowner",      26l, 81l, 0l, 0l,  4,  2, 0, '\001', '\001', 0l, 0l, -1l }, \
{ 81l, "prolang",       26l, 81l, 0l, 0l,  4,  3, 0, '\001', '\001', 0l, 0l, -1l }, \
{ 81l, "proisinh",      16l, 81l, 0l, 0l,  1,  4, 0, '\001', '\001', 0l, 0l, -1l }, \
{ 81l, "proistrusted",  16l, 81l, 0l, 0l,  1,  5, 0, '\001', '\001', 0l, 0l, -1l }, \
{ 81l, "proiscachable", 16l, 81l, 0l, 0l,  1,  6, 0, '\001', '\001', 0l, 0l, -1l }, \
{ 81l, "pronargs",      21l, 81l, 0l, 0l,  2,  7, 0, '\001', '\001', 0l, 0l, -1l }, \
{ 81l, "prorettype",    26l, 81l, 0l, 0l,  4,  8, 0, '\001', '\001', 0l, 0l, -1l }, \
{ 81l, "proargtypes",   30l, 81l, 0l, 0l,  32, 9, 0,   '\0', '\001', 0l, 0l, \
      -1l }, \
{ 81l, "prosrc",        25l, 81l, 0l, 0l, -1,  10, 0,   '\0', '\001', 0l, 0l, -1l }, \
{ 81l, "probin",        17l, 81l, 0l, 0l, -1,  11, 0,   '\0', '\001', 0l, 0l, -1l }

DATA(insert OID = 0 (  81 proname          19 0 0 0 16   1 0 f t 0 0 -1));
DATA(insert OID = 0 (  81 proowner         26 0 0 0  4   2 0 t t 0 0 -1));
DATA(insert OID = 0 (  81 prolang          26 0 0 0  4   3 0 t t 0 0 -1));
DATA(insert OID = 0 (  81 proisinh         16 0 0 0  1   4 0 t t 0 0 -1));
DATA(insert OID = 0 (  81 proistrusted     16 0 0 0  1   5 0 t t 0 0 -1));
DATA(insert OID = 0 (  81 proiscachable    16 0 0 0  1   6 0 t t 0 0 -1));
DATA(insert OID = 0 (  81 pronargs         21 0 0 0  2   7 0 t t 0 0 -1));
DATA(insert OID = 0 (  81 prorettype       26 0 0 0  4   8 0 t t 0 0 -1));
DATA(insert OID = 0 (  81 proargtypes      30 0 0 0 32   9 0 f t 0 0 -1));
DATA(insert OID = 0 (  81 prosrc           25 0 0 0 -1  10 0 f t 0 0 -1));
DATA(insert OID = 0 (  81 probin           17 0 0 0 -1  11 0 f t 0 0 -1));
DATA(insert OID = 0 (  81 ctid             27 0 0 0  6  -1 0 f t 0 0 -1));
DATA(insert OID = 0 (  81 lock             31 0 0 0 -1  -2 0 f t 0 0 -1));
DATA(insert OID = 0 (  81 oid              26 0 0 0  4  -3 0 t t 0 0 -1));
DATA(insert OID = 0 (  81 xmin             28 0 0 0  5  -4 0 f t 0 0 -1));
DATA(insert OID = 0 (  81 cmin             29 0 0 0  1  -5 0 t t 0 0 -1));
DATA(insert OID = 0 (  81 xmax             28 0 0 0  5  -6 0 f t 0 0 -1));
DATA(insert OID = 0 (  81 cmax             29 0 0 0  1  -7 0 t t 0 0 -1));
DATA(insert OID = 0 (  81 chain            27 0 0 0  6  -8 0 f t 0 0 -1));
DATA(insert OID = 0 (  81 anchor           27 0 0 0  6  -9 0 f t 0 0 -1));
DATA(insert OID = 0 (  81 tmax            702 0 0 0  4 -10 0 t t 0 0 -1));
DATA(insert OID = 0 (  81 tmin            702 0 0 0  4 -11 0 t t 0 0 -1));
DATA(insert OID = 0 (  81 vtype            18 0 0 0  1 -12 0 t t 0 0 -1));
    
/* ----------------
 *	pg_server
 * ----------------
 */
DATA(insert OID = 0 (  82 sername          19 0 0 0 16   1 0 f t 0 0 -1));
DATA(insert OID = 0 (  82 serpid           21 0 0 0  2   2 0 t t 0 0 -1));
DATA(insert OID = 0 (  82 serport          21 0 0 0  2   3 0 t t 0 0 -1));
DATA(insert OID = 0 (  82 ctid             27 0 0 0  6  -1 0 f t 0 0 -1));
DATA(insert OID = 0 (  82 lock             31 0 0 0 -1  -2 0 f t 0 0 -1));
DATA(insert OID = 0 (  82 oid              26 0 0 0  4  -3 0 t t 0 0 -1));
DATA(insert OID = 0 (  82 xmin             28 0 0 0  5  -4 0 f t 0 0 -1));
DATA(insert OID = 0 (  82 cmin             29 0 0 0  1  -5 0 t t 0 0 -1));
DATA(insert OID = 0 (  82 xmax             28 0 0 0  5  -6 0 f t 0 0 -1));
DATA(insert OID = 0 (  82 cmax             29 0 0 0  1  -7 0 t t 0 0 -1));
DATA(insert OID = 0 (  82 chain            27 0 0 0  6  -8 0 f t 0 0 -1));
DATA(insert OID = 0 (  82 anchor           27 0 0 0  6  -9 0 f t 0 0 -1));
DATA(insert OID = 0 (  82 tmax            702 0 0 0  4 -10 0 t t 0 0 -1));
DATA(insert OID = 0 (  82 tmin            702 0 0 0  4 -11 0 t t 0 0 -1));
DATA(insert OID = 0 (  82 vtype            18 0 0 0  1 -12 0 t t 0 0 -1));
    
/* ----------------
 *	pg_user
 * ----------------
 */
DATA(insert OID = 0 (  86 usename          19 0 0 0 16   1 0 f t 0 0 -1));
DATA(insert OID = 0 (  86 usesysid         21 0 0 0  2   2 0 t t 0 0 -1));
DATA(insert OID = 0 (  86 usecreatedb      16 0 0 0  1   3 0 t t 0 0 -1));
DATA(insert OID = 0 (  86 usetrace         16 0 0 0  1   4 0 t t 0 0 -1));
DATA(insert OID = 0 (  86 usesuper         16 0 0 0  1   5 0 t t 0 0 -1));
DATA(insert OID = 0 (  86 usecatupd        16 0 0 0  1   6 0 t t 0 0 -1));
DATA(insert OID = 0 (  86 ctid             27 0 0 0  6  -1 0 f t 0 0 -1));
DATA(insert OID = 0 (  86 lock             31 0 0 0 -1  -2 0 f t 0 0 -1));
DATA(insert OID = 0 (  86 oid              26 0 0 0  4  -3 0 t t 0 0 -1));
DATA(insert OID = 0 (  86 xmin             28 0 0 0  5  -4 0 f t 0 0 -1));
DATA(insert OID = 0 (  86 cmin             29 0 0 0  1  -5 0 t t 0 0 -1));
DATA(insert OID = 0 (  86 xmax             28 0 0 0  5  -6 0 f t 0 0 -1));
DATA(insert OID = 0 (  86 cmax             29 0 0 0  1  -7 0 t t 0 0 -1));
DATA(insert OID = 0 (  86 chain            27 0 0 0  6  -8 0 f t 0 0 -1));
DATA(insert OID = 0 (  86 anchor           27 0 0 0  6  -9 0 f t 0 0 -1));
DATA(insert OID = 0 (  86 tmax            702 0 0 0  4 -10 0 t t 0 0 -1));
DATA(insert OID = 0 (  86 tmin            702 0 0 0  4 -11 0 t t 0 0 -1));
DATA(insert OID = 0 (  86 vtype            18 0 0 0  1 -12 0 t t 0 0 -1));
    
/* ----------------
 *	pg_attribute
 * ----------------
 */
#define Schema_pg_attribute \
{ 75l, "attrelid",    26l, 75l, 0l, 0l,  4,  1, 0, '\001', '\001', 0l, 0l, -1l }, \
{ 75l, "attname",     19l, 75l, 0l, 0l, 16,  2, 0,   '\0', '\001', 0l, 0l, -1l }, \
{ 75l, "atttypid",    26l, 75l, 0l, 0l,  4,  3, 0, '\001', '\001', 0l, 0l, -1l }, \
{ 75l, "attdefrel",   26l, 75l, 0l, 0l,  4,  4, 0, '\001', '\001', 0l, 0l, -1l }, \
{ 75l, "attnvals",    23l, 75l, 0l, 0l,  4,  5, 0, '\001', '\001', 0l, 0l, -1l }, \
{ 75l, "atttyparg",   26l, 75l, 0l, 0l,  4,  6, 0, '\001', '\001', 0l, 0l, -1l }, \
{ 75l, "attlen",      21l, 75l, 0l, 0l,  2,  7, 0, '\001', '\001', 0l, 0l, -1l }, \
{ 75l, "attnum",      21l, 75l, 0l, 0l,  2,  8, 0, '\001', '\001', 0l, 0l, -1l }, \
{ 75l, "attbound",    21l, 75l, 0l, 0l,  2,  9, 0, '\001', '\001', 0l, 0l, -1l }, \
{ 75l, "attbyval",    16l, 75l, 0l, 0l,  1, 10, 0, '\001', '\001', 0l, 0l, -1l }, \
{ 75l, "attcanindex", 16l, 75l, 0l, 0l,  1, 11, 0, '\001', '\001', 0l, 0l, -1l }, \
{ 75l, "attproc",     26l, 75l, 0l, 0l,  4, 12, 0, '\001', '\001', 0l, 0l, -1l }, \
{ 75l, "attnelems",   23l, 75l, 0l, 0l,  4, 13, 0, '\001', '\001', 0l, 0l, -1l }, \
{ 75l, "attcacheoff", 23l, 75l, 0l, 0l,  4, 14, 0, '\001', '\001', 0l, 0l, -1l }

DATA(insert OID = 0 (  75 attrelid         26 0 0 0  4   1 0 t t 0 0 -1));
DATA(insert OID = 0 (  75 attname          19 0 0 0 16   2 0 f t 0 0 -1));
DATA(insert OID = 0 (  75 atttypid         26 0 0 0  4   3 0 t t 0 0 -1));
DATA(insert OID = 0 (  75 attdefrel        26 0 0 0  4   4 0 t t 0 0 -1));
DATA(insert OID = 0 (  75 attnvals         23 0 0 0  4   5 0 t t 0 0 -1));
DATA(insert OID = 0 (  75 atttyparg        26 0 0 0  4   6 0 t t 0 0 -1));
DATA(insert OID = 0 (  75 attlen           21 0 0 0  2   7 0 t t 0 0 -1));
DATA(insert OID = 0 (  75 attnum           21 0 0 0  2   8 0 t t 0 0 -1));
DATA(insert OID = 0 (  75 attbound         21 0 0 0  2   9 0 t t 0 0 -1));
DATA(insert OID = 0 (  75 attbyval         16 0 0 0  1  10 0 t t 0 0 -1));
DATA(insert OID = 0 (  75 attcanindex      16 0 0 0  1  11 0 t t 0 0 -1));
DATA(insert OID = 0 (  75 attproc          26 0 0 0  4  12 0 t t 0 0 -1));
DATA(insert OID = 0 (  75 attnelems        23 0 0 0  4  13 0 t t 0 0 -1));
DATA(insert OID = 0 (  75 attcacheoff      23 0 0 0  4  14 0 t t 0 0 -1));
DATA(insert OID = 0 (  75 ctid             27 0 0 0  6  -1 0 f t 0 0 -1));
DATA(insert OID = 0 (  75 lock             31 0 0 0 -1  -2 0 f t 0 0 -1));
DATA(insert OID = 0 (  75 oid              26 0 0 0  4  -3 0 t t 0 0 -1));
DATA(insert OID = 0 (  75 xmin             28 0 0 0  5  -4 0 f t 0 0 -1));
DATA(insert OID = 0 (  75 cmin             29 0 0 0  1  -5 0 t t 0 0 -1));
DATA(insert OID = 0 (  75 xmax             28 0 0 0  5  -6 0 f t 0 0 -1));
DATA(insert OID = 0 (  75 cmax             29 0 0 0  1  -7 0 t t 0 0 -1));
DATA(insert OID = 0 (  75 chain            27 0 0 0  6  -8 0 f t 0 0 -1));
DATA(insert OID = 0 (  75 anchor           27 0 0 0  6  -9 0 f t 0 0 -1));
DATA(insert OID = 0 (  75 tmax            702 0 0 0  4 -10 0 t t 0 0 -1));
DATA(insert OID = 0 (  75 tmin            702 0 0 0  4 -11 0 t t 0 0 -1));
DATA(insert OID = 0 (  75 vtype            18 0 0 0  1 -12 0 t t 0 0 -1));
    
/* ----------------
 *	pg_relation
 * ----------------
 */
#define Schema_pg_relation \
{ 83l, "relname",      19l, 83l, 0l, 0l, 16,  1, 0,   '\0', '\001', 0l, 0l, -1l }, \
{ 83l, "relowner",     26l, 83l, 0l, 0l,  4,  2, 0, '\001', '\001', 0l, 0l, -1l }, \
{ 83l, "relam",        26l, 83l, 0l, 0l,  4,  3, 0, '\001', '\001', 0l, 0l, -1l }, \
{ 83l, "relpages",     23,  83l, 0l, 0l,  4,  4, 0, '\001', '\001', 0l, 0l, -1l }, \
{ 83l, "reltuples",    23,  83l, 0l, 0l,  4,  5, 0, '\001', '\001', 0l, 0l, -1l }, \
{ 83l, "relexpires",   20,  83l, 0l, 0l,  4,  6, 0, '\001', '\001', 0l, 0l, -1l }, \
{ 83l, "relpreserved", 20,  83l, 0l, 0l,  4,  7, 0, '\001', '\001', 0l, 0l, -1l }, \
{ 83l, "relhasindex",  16,  83l, 0l, 0l,  1,  8, 0, '\001', '\001', 0l, 0l, -1l }, \
{ 83l, "relisshared",  16,  83l, 0l, 0l,  1,  9, 0, '\001', '\001', 0l, 0l, -1l }, \
{ 83l, "relkind",      18,  83l, 0l, 0l,  1, 10, 0, '\001', '\001', 0l, 0l, -1l }, \
{ 83l, "relarch",      18,  83l, 0l, 0l,  1, 11, 0, '\001', '\001', 0l, 0l, -1l }, \
{ 83l, "relnatts",     21,  83l, 0l, 0l,  2, 12, 0, '\001', '\001', 0l, 0l, -1l }, \
{ 83l, "relsmgr",      210l,  83l, 0l, 0l,  2, 13, 0, '\001', '\001', 0l, 0l, -1l }, \
{ 83l, "relkey",       22,  83l, 0l, 0l, 16, 14, 0,   '\0', '\001', 0l, 0l, -1l }, \
{ 83l, "relkeyop",     30,  83l, 0l, 0l, 32, 15, 0,   '\0', '\001', 0l, 0l, -1l }

DATA(insert OID = 0 (  83 relname          19 0 0 0 16   1 0 f t 0 0 -1));
DATA(insert OID = 0 (  83 relowner         26 0 0 0  4   2 0 t t 0 0 -1));
DATA(insert OID = 0 (  83 relam            26 0 0 0  4   3 0 t t 0 0 -1));
DATA(insert OID = 0 (  83 relpages         23 0 0 0  4   4 0 t t 0 0 -1));
DATA(insert OID = 0 (  83 reltuples        23 0 0 0  4   5 0 t t 0 0 -1));
DATA(insert OID = 0 (  83 relexpires      702 0 0 0  4   6 0 t t 0 0 -1));
DATA(insert OID = 0 (  83 relpreserved    702 0 0 0  4   7 0 t t 0 0 -1));
DATA(insert OID = 0 (  83 relhasindex      16 0 0 0  1   8 0 t t 0 0 -1));
DATA(insert OID = 0 (  83 relisshared      16 0 0 0  1   9 0 t t 0 0 -1));
DATA(insert OID = 0 (  83 relkind          18 0 0 0  1  10 0 t t 0 0 -1));
DATA(insert OID = 0 (  83 relarch          18 0 0 0  1  11 0 t t 0 0 -1));
DATA(insert OID = 0 (  83 relnatts         21 0 0 0  2  12 0 t t 0 0 -1));
DATA(insert OID = 0 (  83 relsmgr         210 0 0 0  2  13 0 t t 0 0 -1));
DATA(insert OID = 0 (  83 relkey           22 0 0 0 16  14 0 f t 0 0 -1));
DATA(insert OID = 0 (  83 relkeyop         30 0 0 0 32  15 0 f t 0 0 -1));
DATA(insert OID = 0 (  83 ctid             27 0 0 0  6  -1 0 f t 0 0 -1));
DATA(insert OID = 0 (  83 lock             31 0 0 0 -1  -2 0 f t 0 0 -1));
DATA(insert OID = 0 (  83 oid              26 0 0 0  4  -3 0 t t 0 0 -1));
DATA(insert OID = 0 (  83 xmin             28 0 0 0  5  -4 0 f t 0 0 -1));
DATA(insert OID = 0 (  83 cmin             29 0 0 0  1  -5 0 t t 0 0 -1));
DATA(insert OID = 0 (  83 xmax             28 0 0 0  5  -6 0 f t 0 0 -1));
DATA(insert OID = 0 (  83 cmax             29 0 0 0  1  -7 0 t t 0 0 -1));
DATA(insert OID = 0 (  83 chain            27 0 0 0  6  -8 0 f t 0 0 -1));
DATA(insert OID = 0 (  83 anchor           27 0 0 0  6  -9 0 f t 0 0 -1));
DATA(insert OID = 0 (  83 tmax            702 0 0 0  4 -10 0 t t 0 0 -1));
DATA(insert OID = 0 (  83 tmin            702 0 0 0  4 -11 0 t t 0 0 -1));
DATA(insert OID = 0 (  83 vtype            18 0 0 0  1 -12 0 t t 0 0 -1));
    
/* ----------------
 *	pg_magic
 * ----------------
 */
DATA(insert OID = 0 (  80 magname          19 0 0 0 16   1 0 f t 0 0 -1));
DATA(insert OID = 0 (  80 magvalue         19 0 0 0 16   2 0 f t 0 0 -1));
DATA(insert OID = 0 (  80 ctid             27 0 0 0  6  -1 0 f t 0 0 -1));
DATA(insert OID = 0 (  80 lock             31 0 0 0 -1  -2 0 f t 0 0 -1));
DATA(insert OID = 0 (  80 oid              26 0 0 0  4  -3 0 t t 0 0 -1));
DATA(insert OID = 0 (  80 xmin             28 0 0 0  5  -4 0 f t 0 0 -1));
DATA(insert OID = 0 (  80 cmin             29 0 0 0  1  -5 0 t t 0 0 -1));
DATA(insert OID = 0 (  80 xmax             28 0 0 0  5  -6 0 f t 0 0 -1));
DATA(insert OID = 0 (  80 cmax             29 0 0 0  1  -7 0 t t 0 0 -1));
DATA(insert OID = 0 (  80 chain            27 0 0 0  6  -8 0 f t 0 0 -1));
DATA(insert OID = 0 (  80 anchor           27 0 0 0  6  -9 0 f t 0 0 -1));
DATA(insert OID = 0 (  80 tmax            702 0 0 0  4 -10 0 t t 0 0 -1));
DATA(insert OID = 0 (  80 tmin            702 0 0 0  4 -11 0 t t 0 0 -1));
DATA(insert OID = 0 (  80 vtype            18 0 0 0  1 -12 0 t t 0 0 -1));
    
/* ----------------
 *	pg_defaults
 * ----------------
 */
DATA(insert OID = 0 (  89 defname          19 0 0 0 16   1 0 f t 0 0 -1));
DATA(insert OID = 0 (  89 defvalue         19 0 0 0 16   2 0 f t 0 0 -1));
DATA(insert OID = 0 (  89 ctid             27 0 0 0  6  -1 0 f t 0 0 -1));
DATA(insert OID = 0 (  89 lock             31 0 0 0 -1  -2 0 f t 0 0 -1));
DATA(insert OID = 0 (  89 oid              26 0 0 0  4  -3 0 t t 0 0 -1));
DATA(insert OID = 0 (  89 xmin             28 0 0 0  5  -4 0 f t 0 0 -1));
DATA(insert OID = 0 (  89 cmin             29 0 0 0  1  -5 0 t t 0 0 -1));
DATA(insert OID = 0 (  89 xmax             28 0 0 0  5  -6 0 f t 0 0 -1));
DATA(insert OID = 0 (  89 cmax             29 0 0 0  1  -7 0 t t 0 0 -1));
DATA(insert OID = 0 (  89 chain            27 0 0 0  6  -8 0 f t 0 0 -1));
DATA(insert OID = 0 (  89 anchor           27 0 0 0  6  -9 0 f t 0 0 -1));
DATA(insert OID = 0 (  89 tmax            702 0 0 0  4 -10 0 t t 0 0 -1));
DATA(insert OID = 0 (  89 tmin            702 0 0 0  4 -11 0 t t 0 0 -1));
DATA(insert OID = 0 (  89 vtype            18 0 0 0  1 -12 0 t t 0 0 -1));
    
/* ----------------
 *	pg_variable - this relation is modified by special purpose access
 *	          method code.  The following is garbage but is needed
 *		  so that the reldesc code works properly.
 * ----------------
 */
#define Schema_pg_variable \
{ 90l, "varfoo",  26l, 90l, 0l, 0l, 4, 1, 0, '\001', '\001', 0l, 0l, -1l }
    
DATA(insert OID = 0 (  90 varfoo           26 0 0 0  4   1 0 t t 0 0 -1));
    
/* ----------------
 *	pg_log - this relation is modified by special purpose access
 *	          method code.  The following is garbage but is needed
 *		  so that the reldesc code works properly.
 * ----------------
 */
#define Schema_pg_log \
{ 99l, "logfoo",  26l, 99l, 0l, 0l, 4, 1, 0, '\001', '\001', 0l, 0l, -1l }

DATA(insert OID = 0 (  99 logfoo           26 0 0 0  4   1 0 t t 0 0 -1));
    
/* ----------------
 *	pg_time - this relation is modified by special purpose access
 *	          method code.  The following is garbage but is needed
 *		  so that the reldesc code works properly.
 * ----------------
 */
#define Schema_pg_time \
{ 100l, "timefoo",  26l, 100l, 0l, 0l, 4, 1, 0, '\001', '\001', 0l, 0l, -1l }

DATA(insert OID = 0 (  100 timefoo         26 0 0 0  4   1 0 t t 0 0 -1));
    
/* ----------------
 *	dummy attribute definition.  
 * ----------------
 */
#define DummyAttributeTupleForm \
{ 0l, "dummy", 28, 0l, 0l, 0l, 5, 1, 0, '\000', '\001', 0, 0, 0}

/* ----------------------------------------------------------------
 *			old stuff
 * ----------------------------------------------------------------
 */
/* ----------------
 *	old definition of AttributeTupleForm
 * ----------------
 */
#ifndef AttributeTupleForm_Defined
#define AttributeTupleForm_Defined 1

typedef struct AttributeTupleFormD {
	ObjectId	attrelid;
	NameData	attname;
	ObjectId	atttypid;
	ObjectId	attdefrel;
	uint32		attnvals;
	ObjectId	atttyparg;	/* type arg for arrays/spquel/procs */
	int16		attlen;
	AttributeNumber	attnum;
	uint16		attbound;
	Boolean		attbyval;
	Boolean		attcanindex;
	OID		attproc;	/* spquel? */
	uint32		attnelems;
	int32		attcacheoff;
/*	char	*attlock; */
} AttributeTupleFormD;

typedef AttributeTupleFormD	*AttributeTupleForm;
#define AttributeTupleFormData	AttributeTupleFormD

#endif AttributeTupleForm_Defined

/* ----------------
 *	old definition of struct attribute
 * ----------------
 */
#ifndef struct_attribute_Defined
#define struct_attribute_Defined 1

struct	attribute {
	OID	attrelid;
	char	attname[16];
	OID	atttypid;
	OID	attdefrel;
	uint32	attnvals;
	OID	atttyparg;		/* type arg for arrays/spquel/procs */
	int16	attlen;
	int16	attnum;
	uint16	attbound;
	Boolean	attbyval;
	Boolean	attcanindex;
	OID	attproc;		/* spquel? */
	uint32	attnelems;
	int32	attcacheoff;
/*	char	*attlock; */
};

#endif struct_attribute_Defined

/* ----------------
 *	old style compiler constants.  these are obsolete and
 *	should not be used -cim 6/17/90
 * ----------------
 */
#define AttributeRelationIdAttributeNumber \
    Anum_pg_attribute_attrelid
#define AttributeNameAttributeNumber \
    Anum_pg_attribute_attname
#define AttributeNumberAttributeNumber \
    Anum_pg_attribute_attnum
#define AttributeProcAttributeNumber \
    Anum_pg_attribute_attproc
    
#define AttributeRelationNumberOfAttributes \
    Natts_pg_attribute
    
#endif PgAttributeIncluded
