/* ----------------------------------------------------------------
 *   FILE
 *	pg_type.h
 *
 *   DESCRIPTION
 *	definition of the system "type" relation (pg_type)
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
#ifndef PgTypeIncluded
#define PgTypeIncluded 1	/* include this only once */

/* ----------------
 *	postgres.h contains the system type definintions and the
 *	CATALOG(), BOOTSTRAP and DATA() sugar words so this file
 *	can be read by both genbki.sh and the C compiler.
 * ----------------
 */
#include "tmp/postgres.h"

/* ----------------
 *	pg_type definition.  cpp turns this into
 *	typedef struct FormData_pg_type
 * ----------------
 */
CATALOG(pg_type) BOOTSTRAP {
    char16 	typname;
    oid  	typowner;
    int2  	typlen;
    int2  	typprtlen;
    bool  	typbyval;
    char  	typtype;
    bool  	typisdefined;
    char	typdelim;
    oid 	typrelid;
    oid  	typelem;
    regproc  	typinput;
    regproc  	typoutput;
    regproc  	typreceive;
    regproc  	typsend;
    text     	typdefault;	/* VARIABLE LENGTH FIELD */
} FormData_pg_type;

/* ----------------
 *	Form_pg_type corresponds to a pointer to a tuple with
 *	the format of pg_type relation.
 * ----------------
 */
typedef FormData_pg_type	*Form_pg_type;

/* ----------------
 *	compiler constants for pg_type
 * ----------------
 */
#define Name_pg_type			"pg_type"
#define Natts_pg_type			15
#define Anum_pg_type_typname		1
#define Anum_pg_type_typowner		2
#define Anum_pg_type_typlen		3
#define Anum_pg_type_typprtlen		4
#define Anum_pg_type_typbyval		5
#define Anum_pg_type_typtype		6
#define Anum_pg_type_typisdefined	7
#define Anum_pg_type_typdelim     	8
#define Anum_pg_type_typrelid		9
#define Anum_pg_type_typelem		10
#define Anum_pg_type_typinput		11
#define Anum_pg_type_typoutput		12
#define Anum_pg_type_typreceive		13
#define Anum_pg_type_typsend		14
#define Anum_pg_type_typdefault		15

/*
 * XXX -- the implementation of int28's in postgres is a hack, and will
 *	  go away someday.  until that happens, there is a case (in the
 *	  catalog cache management code) where we need to step gingerly
 *	  over piles of int28's on the sidewalk.  in order to do so, we
 *	  need the OID of the int28 tuple from pg_type.
 */

#define INT28OID	22

/* ----------------
 *	initial contents of pg_type
 * ----------------
 */

DATA(insert OID = 16 (  bool       6  1   1 t b t \054 0   0 boolin boolout boolin boolout - ));
DATA(insert OID = 17 (  bytea      6 -1  -1 f b t \054 0  18 byteain byteaout byteain byteaout - ));
DATA(insert OID = 18 (  char       6  1   1 t b t \054 0   0 charin charout charin charout - ));
DATA(insert OID = 19 (  char16     6 16  16 f b t \054 0  18 char16in char16out char16in char16out - ));
DATA(insert OID = 20 (  dt         6  4  10 t b t \054 0   0 dtin dtout dtin dtout - ));
DATA(insert OID = 21 (  int2       6  2   5 t b t \054 0   0 int2in int2out int2in int2out - ));
DATA(insert OID = 22 (  int28      6 16  50 f b t \054 0  21 int28in int28out int28in int28out - ));
DATA(insert OID = 23 (  int4       6  4  10 t b t \054 0   0 int4in int4out int4in int4out - ));
DATA(insert OID = 24 (  regproc    6  4  16 t b t \054 0   0 regprocin regprocout regprocin regprocout - ));
DATA(insert OID = 25 (  text       6 -1  -1 f b t \054 0  18 textin textout textin textout - ));
DATA(insert OID = 26 (  oid        6  4  10 t b t \054 0   0 int4in int4out int4in int4out - ));
DATA(insert OID = 27 (  tid        6  6  19 f b t \054 0   0 tidin tidout tidin tidout - ));
DATA(insert OID = 28 (  xid        6  5  12 f b t \054 0   0 xidin xidout xidin xidout - ));
DATA(insert OID = 29 (  cid        6  1   3 t b t \054 0   0 cidin cidout cidin cidout - ));
DATA(insert OID = 30 (  oid8       6 32  89 f b t \054 0  26 oid8in oid8out oid8in oid8out - ));
DATA(insert OID = 31 (  lock       6 -1  -1 f b t \054 0  -1 lockin lockout lockin lockout - ));
DATA(insert OID = 32 (  SET        6 -1  -1 f r t \054 0  -1 textin textout textin textout - ));
DATA(insert OID = 33 (  stub       6 -1  -1 f b t \054 0  -1 stubin stubout stubin stubout - ));
DATA(insert OID = 210 (  smgr       6 2  12 t b t \054 0  -1 smgrin smgrout smgrin smgrout - ));
DATA(insert OID = 591 (  ref 	   6  8   1 f b t \054 0   0 refn refout refin refout - ));
DATA(insert OID = 600 (  point     6 16  24 f b t \054 0 701 point_in point_out point_in point_out - ));
DATA(insert OID = 601 (  lseg      6 32  48 f b t \054 0 600 lseg_in lseg_out lseg_in lseg_out - ));
DATA(insert OID = 602 (  path      6 -1  -1 f b t \054 0 600 path_in path_out path_in path_out - ));
DATA(insert OID = 603 (  box       6 32 100 f b t \073 0 600 box_in box_out box_in box_out - ));
DATA(insert OID = 604 (  polygon   6 -1  -1 f b t \054 0  -1 poly_in poly_out poly_in poly_out - ));
DATA(insert OID = 605 (  filename  6 256 -1 f b t \054 0 18 filename_in filename_out filename_in filename_out - ));
DATA(insert OID = 700 (  float4    6  4  12 f b t \054 0   0 float4in float4out float4in float4out - ));
DATA(insert OID = 701 (  float8    6  8  24 f b t \054 0   0 float8in float8out float8in float8out - ));
DATA(insert OID = 702 (  abstime   6  4  20 t b t \054 0   0 abstimein abstimeout abstimein abstimeout - ));
DATA(insert OID = 703 (  reltime   6  4  20 t b t \054 0   0 reltimein reltimeout reltimein reltimeout - ));
DATA(insert OID = 704 (  tinterval 6 12  47 f b t \054 0   0 tintervalin tintervalout tintervalin tintervalout - ));
DATA(insert OID = 705 (  unknown 6 -1  -1 f b t \054 0   18 textin textout textin textout - ));
DATA(insert OID = 910 (  oidseq    6  8  20 f b t \054 0   0 oidseqin oidseqout oidseqin oidseqout - ));
DATA(insert OID = 1000 (  _bool      6 -1  -1 f b t \054 0  16 array_in array_out array_in array_out - ));
DATA(insert OID = 1001 (  _bytea     6 -1  -1 f b t \054 0  17 array_in array_out array_in array_out - ));
DATA(insert OID = 1002 (  _char      6 -1  -1 f b t \054 0  18 textin textout textin textout - ));
DATA(insert OID = 1003 (  _char16    6 -1  -1 f b t \054 0  19 array_in array_out array_in array_out - ));
DATA(insert OID = 1004 (  _dt        6 -1  -1 f b t \054 0  20 array_in array_out array_in array_out - ));
DATA(insert OID = 1005 (  _int2      6 -1  -1 f b t \054 0  21 array_in array_out array_in array_out - ));
DATA(insert OID = 1006 (  _int28     6 -1  -1 f b t \054 0  22 array_in array_out array_in array_out - )); 
DATA(insert OID = 1007 (  _int4      6 -1  -1 f b t \054 0  23 array_in array_out array_in array_out - ));  
DATA(insert OID = 1008 (  _regproc   6 -1  -1 f b t \054 0  24 array_in array_out array_in array_out - ));  
DATA(insert OID = 1009 (  _text      6 -1  -1 f b t \054 0  25 array_in array_out array_in array_out - ));
DATA(insert OID = 1009 (  _oid       6 -1  -1 f b t \054 0  26 array_in array_out array_in array_out - ));
DATA(insert OID = 1010 (  _tid       6 -1  -1 f b t \054 0  27 array_in array_out array_in array_out - ));
DATA(insert OID = 1011 (  _xid       6 -1  -1 f b t \054 0  28 array_in array_out array_in array_out - ));
DATA(insert OID = 1012 (  _cid       6 -1  -1 f b t \054 0  29 array_in array_out array_in array_out - ));
DATA(insert OID = 1013 (  _oid8      6 -1  -1 f b t \054 0  30 array_in array_out array_in array_out - ));
DATA(insert OID = 1014 (  _lock      6 -1  -1 f b t \054 0  31 array_in array_out array_in array_out - ));
DATA(insert OID = 1015 (  _stub      6 -1  -1 f b t \054 0  33 array_in array_out array_in array_out - ));
DATA(insert OID = 1016 (  _ref 	     6 -1  -1 f b t \054 0 591 array_in array_out array_in array_out - ));
DATA(insert OID = 1017 (  _point     6 -1  -1 f b t \054 0 600 array_in array_out array_in array_out - ));
DATA(insert OID = 1018 (  _lseg      6 -1  -1 f b t \054 0 601 array_in array_out array_in array_out - ));
DATA(insert OID = 1019 (  _path      6 -1  -1 f b t \054 0 602 array_in array_out array_in array_out - ));
DATA(insert OID = 1020 (  _box       6 -1  -1 f b t \073 0 603 array_in array_out array_in array_out - ));
DATA(insert OID = 1021 (  _float4    6 -1  -1 f b t \054 0 700 array_in array_out array_in array_out - ));
DATA(insert OID = 1022 (  _float8    6 -1  -1 f b t \054 0 701 array_in array_out array_in array_out - ));
DATA(insert OID = 1023 (  _abstime   6 -1  -1 f b t \054 0 702 array_in array_out array_in array_out - ));
DATA(insert OID = 1024 (  _reltime   6 -1  -1 f b t \054 0 703 array_in array_out array_in array_out - ));
DATA(insert OID = 1025 (  _tinterval 6 -1  -1 f b t \054 0 704 array_in array_out array_in array_out - ));
DATA(insert OID = 1026 (  _filename 6 -1  -1 f b t \054 0 605 array_in array_out array_in array_out - ));
DATA(insert OID = 1027 (  _polygon 6 -1  -1 f b t \054 0 604 array_in array_out array_in array_out - ));

/* ----------------
 *	old definition of TypeTupleForm
 * ----------------
 */
#ifndef TypeTupleForm_Defined
#define TypeTupleForm_Defined 1

typedef struct	TypeTupleFormD {
        /* VARIABLE LENGTH STRUCTURE */
	NameData	typname;
	ObjectId	typowner;
	int16		typlen;
	int16		typprtlen;
	Boolean		typbyval;
	char		typtype;
	Boolean 	typisdefined;
	char		typdelim;
	ObjectId	typrelid;
	ObjectId	typelem;
	RegProcedure	typinput;
	RegProcedure	typoutput;
	RegProcedure	typreceive;
	RegProcedure	typsend;
	struct	varlena	typdefault;
} TypeTupleFormD;

typedef TypeTupleFormD		*TypeTupleForm;
#define TypeTupleFormData	TypeTupleFormD

#endif TypeTupleForm_Defined

/* ----------------
 *	old definition of struct type
 * ----------------
 */
#ifndef struct_type_Defined
#define struct_type_Defined 1

struct	type {
	char	typname[16];
	OID	typowner;
	int16	typlen;
	int16	typprtlen;
	Boolean	typbyval;
	char	typtype;
	Boolean typisdefined;
	char	typdelim;
	OID	typrelid;
	OID	typelem;
	REGPROC	typinput;
	REGPROC	typoutput;
	REGPROC	typreceive;
	REGPROC	typsend;
	struct	varlena	typdefault;
}; /* VARIABLE LENGTH STRUCTURE */

#endif struct_type_Defined


/* ----------------
 *	old style compiler constants.  these are obsolete and
 *	should not be used -cim 6/17/90
 * ----------------
 */    
#define	TypeNameAttributeNumber \
    Anum_pg_type_typname
#define TypeLengthAttributeNumber \
    Anum_pg_type_typlen
#define	TypeIsDefinedAttributeNumber \
    Anum_pg_type_typisdefined
#define	TypeDefaultAttributeNumber \
    Anum_pg_type_typdefault

#define TypeRelationNumberOfAttributes \
    Natts_pg_type
    
#endif PgTypeIncluded
