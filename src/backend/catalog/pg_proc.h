/* ----------------------------------------------------------------
 *   FILE
 *	pg_proc.h
 *
 *   DESCRIPTION
 *	definition of the system "procedure" relation (pg_proc)
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
#ifndef PgProcIncluded
#define PgProcIncluded 1	/* include this only once */

/* ----------------
 *	postgres.h contains the system type definintions and the
 *	CATALOG(), BOOTSTRAP and DATA() sugar words so this file
 *	can be read by both genbki.sh and the C compiler.
 * ----------------
 */
#include "tmp/postgres.h"
#include "nodes/pg_lisp.h"
#include "tcop/dest.h"

/* ----------------
 *	pg_proc definition.  cpp turns this into
 *	typedef struct FormData_pg_proc
 * ----------------
 */
CATALOG(pg_proc) BOOTSTRAP {
    char16 	proname;
    oid 	proowner;
    oid 	prolang;
    bool 	proisinh;
    bool 	proistrusted;
    bool 	proiscachable;
    int2 	pronargs;
    bool	proretset;
    oid 	prorettype;
    oid8        proargtypes;
    int4        probyte_pct;
    int4        properbyte_cpu;
    int4        propercall_cpu;
    int4        prooutin_ratio;
    text 	prosrc;		/* VARIABLE LENGTH FIELD */
    bytea 	probin;		/* VARIABLE LENGTH FIELD */
} FormData_pg_proc;

/* ----------------
 *	Form_pg_proc corresponds to a pointer to a tuple with
 *	the format of pg_proc relation.
 * ----------------
 */
typedef FormData_pg_proc	*Form_pg_proc;

/* ----------------
 *	compiler constants for pg_proc
 * ----------------
 */
#define Name_pg_proc			"pg_proc"
#define Natts_pg_proc			16
#define Anum_pg_proc_proname		1
#define Anum_pg_proc_proowner		2
#define Anum_pg_proc_prolang		3
#define Anum_pg_proc_proisinh		4
#define Anum_pg_proc_proistrusted	5
#define Anum_pg_proc_proiscachable	6
#define Anum_pg_proc_pronargs		7
#define Anum_pg_proc_proretset		8
#define Anum_pg_proc_prorettype		9
#define Anum_pg_proc_proargtypes        10
#define Anum_pg_proc_probyte_pct        11
#define Anum_pg_proc_properbyte_cpu     12
#define Anum_pg_proc_propercall_cpu     13
#define Anum_pg_proc_prooutin_ratio     14 
#define Anum_pg_proc_prosrc		15
#define Anum_pg_proc_probin		16

/* ----------------
 *	initial contents of pg_proc
 * ----------------
 */

DATA(insert OID =  28 (  boolin            6 11 f t f 1 f 16 "0" 100 0 0  100  foo bar ));
DATA(insert OID =  29 (  boolout           6 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID =  30 (  byteain           6 11 f t f 1 f 17 "0" 100 0 0 100  foo bar ));
DATA(insert OID =  31 (  byteaout          6 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID =  32 (  charin            6 11 f t f 1 f 18 "0" 100 0 0 100  foo bar ));
DATA(insert OID =  33 (  charout           6 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID =  34 (  char16in          6 11 f t f 1 f 19 "0" 100 0 0 100  foo bar ));
DATA(insert OID =  35 (  char16out         6 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID =  36 (  dtin              6 11 f t f 1 f 20 "0" 100 0 0 100  foo bar ));
DATA(insert OID =  37 (  dtout             6 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID =  38 (  int2in            6 11 f t f 1 f 21 "0" 100 0 0 100  foo bar ));
DATA(insert OID =  39 (  int2out           6 11 f t f 1 f 23 "21" 100 0 0 100  foo bar ));
DATA(insert OID =  40 (  int28in           6 11 f t f 1 f 22 "0" 100 0 0 100  foo bar ));
DATA(insert OID =  41 (  int28out          6 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID =  42 (  int4in            6 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID =  43 (  int4out           6 11 f t f 1 f 19 "23" 100 0 0 100  foo bar ));
DATA(insert OID =  44 (  regprocin         6 11 f t f 1 f 24 "0" 100 0 0 100  foo bar ));
DATA(insert OID =  45 (  regprocout        6 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID =  46 (  textin            6 11 f t f 1 f 25 "0" 100 0 0 100  foo bar ));
#define TextInRegProcedure 46

DATA(insert OID =  47 (  textout           6 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID =  48 (  tidin             6 11 f t f 1 f 27 "0" 100 0 0 100  foo bar ));
DATA(insert OID =  49 (  tidout            6 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID =  50 (  xidin             6 11 f t f 1 f 28 "0" 100 0 0 100  foo bar ));
DATA(insert OID =  51 (  xidout            6 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID =  52 (  cidin             6 11 f t f 1 f 29 "0" 100 0 0 100  foo bar ));
DATA(insert OID =  53 (  cidout            6 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID =  54 (  oid8in            6 11 f t f 1 f 30 "0" 100 0 0 100  foo bar ));
DATA(insert OID =  55 (  oid8out           6 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID =  56 (  lockin            6 11 f t f 1 f 31 "0" 100 0 0 100  foo bar ));
DATA(insert OID =  57 (  lockout           6 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID =  58 (  stubin            6 11 f t f 1 f 33 "0" 100 0 0 100  foo bar ));
DATA(insert OID =  59 (  stubout           6 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID =  60 (  booleq            6 11 f t f 2 f 16 "16 16" 100 0 0 100  foo bar ));
DATA(insert OID =  61 (  chareq            6 11 f t f 2 f 16 "18 18" 100 0 0 100  foo bar ));
#define       CharacterEqualRegProcedure      61

DATA(insert OID =  62 (  char16eq          6 11 f t f 2 f 16 "19 19" 100 0 0 100  foo bar ));
#define NameEqualRegProcedure		62
#define Character16EqualRegProcedure	62
    
DATA(insert OID =  63 (  int2eq            6 11 f t f 2 f 16 "21 21" 100 0 0 100  foo bar ));
#define Integer16EqualRegProcedure	63
    
DATA(insert OID =  64 (  int2lt            6 11 f t f 2 f 16 "21 21" 100 0 0 100  foo bar ));
DATA(insert OID =  65 (  int4eq            6 11 f t f 2 f 16 "23 23" 100 0 0 100  foo bar ));
#define Integer32EqualRegProcedure	65
    
DATA(insert OID =  66 (  int4lt            6 11 f t f 2 f 16 "23 23" 100 0 0 100  foo bar ));
DATA(insert OID =  67 (  texteq            6 11 f t f 2 f 16 "25 25" 100 0 0 0  foo bar ));
DATA(insert OID =  68 (  xideq             6 11 f t f 2 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID =  69 (  cideq             6 11 f t f 2 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID =  70 (  charne            6 11 f t f 2 f 16 "18 18" 100 0 0 100  foo bar ));
DATA(insert OID =  71 (  charlt            6 11 f t f 2 f 16 "18 18" 100 0 0 100  foo bar ));
DATA(insert OID =  72 (  charle            6 11 f t f 2 f 16 "18 18" 100 0 0 100  foo bar ));
DATA(insert OID =  73 (  chargt            6 11 f t f 2 f 16 "18 18" 100 0 0 100  foo bar ));
DATA(insert OID =  74 (  charge            6 11 f t f 2 f 16 "18 18" 100 0 0 100  foo bar ));
DATA(insert OID =  75 (  charpl            6 11 f t f 2 f 16 "18 18" 100 0 0 100  foo bar ));
DATA(insert OID =  76 (  charmi            6 11 f t f 2 f 16 "18 18" 100 0 0 100  foo bar ));
DATA(insert OID =  77 (  charmul           6 11 f t f 2 f 16 "18 18" 100 0 0 100  foo bar ));
DATA(insert OID =  78 (  chardiv           6 11 f t f 2 f 16 "18 18" 100 0 0 100  foo bar ));

DATA(insert OID =  79 (  char16regexeq     6 11 f t f 2 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID =  80 (  char16regexne     6 11 f t f 2 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID =  81 (  textregexeq       6 11 f t f 2 f 16 "0" 100 0 1 0  foo bar ));
DATA(insert OID =  82 (  textregexne       6 11 f t f 2 f 16 "0" 100 0 1 0  foo bar ));

DATA(insert OID =  97 (  rtsel             6 11 f t f 7 f 109 "0" 100 0 0 100  foo bar ));
DATA(insert OID =  98 (  rtnpage           6 11 f t f 7 f 109 "0" 100 0 0 100  foo bar ));
DATA(insert OID =  99 (  btreesel          6 11 f t f 7 f 109 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 100 (  btreenpage        6 11 f t f 7 f 109 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 101 (  eqsel             6 11 f t f 5 f 109 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 102 (  neqsel            6 11 f t f 5 f 109 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 103 (  intltsel          6 11 f t f 5 f 109 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 104 (  intgtsel          6 11 f t f 5 f 109 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 105 (  eqjoinsel         6 11 f t f 5 f 109 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 106 (  neqjoinsel        6 11 f t f 5 f 109 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 107 (  intltjoinsel      6 11 f t f 5 f 109 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 108 (  intgtjoinsel      6 11 f t f 5 f 109 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 109 (  btreeendscan      6 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 110 (  btreemarkpos      6 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 111 (  btreerestrpos     6 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 112 (  btreeinsert       6 11 f t f 3 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 113 (  btreedelete       6 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 114 (  btreegettuple     6 11 f t f 6 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 115 (  btreebuild        6 11 f t f 7 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 116 (  btreerescan       6 11 f t f 3 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 117 (  point_in          6 11 f t f 1 f 600 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 118 (  point_out         6 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 119 (  lseg_in           6 11 f t f 1 f 601 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 120 (  lseg_out          6 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 121 (  path_in           6 11 f t f 1 f 602 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 122 (  path_out          6 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 123 (  box_in            6 11 f t f 1 f 603 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 124 (  box_out           6 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 125 (  box_overlap       6 11 f t f 2 f 16 "0" 100 1 0 100  foo bar ));
DATA(insert OID = 126 (  box_ge            6 11 f t f 2 f 16 "0" 100 1 0 100  foo bar ));
DATA(insert OID = 127 (  box_gt            6 11 f t f 2 f 16 "0" 100 1 0 100  foo bar ));
DATA(insert OID = 128 (  box_eq            6 11 f t f 2 f 16 "0" 100 1 0 100  foo bar ));
DATA(insert OID = 129 (  box_lt            6 11 f t f 2 f 16 "0" 100 1 0 100  foo bar ));
DATA(insert OID = 130 (  box_le            6 11 f t f 2 f 16 "0" 100 1 0 100  foo bar ));
DATA(insert OID = 131 (  point_above       6 11 f t f 2 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 132 (  point_left        6 11 f t f 2 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 133 (  point_right       6 11 f t f 2 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 134 (  point_below       6 11 f t f 2 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 135 (  point_eq          6 11 f t f 2 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 136 (  on_pb             6 11 f t f 2 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 137 (  on_ppath          6 11 f t f 2 f 16 "0" 100 0 1 0  foo bar ));
DATA(insert OID = 138 (  box_center        6 11 f t f 1 f 600 "0" 100 1 0 100  foo bar ));
DATA(insert OID = 139 (  areasel           6 11 f t f 5 f 109 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 140 (  areajoinsel       6 11 f t f 5 f 109 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 141 (  int4mul           6 11 f t f 2 f 23 "23 23" 100 0 0 100  foo bar ));
DATA(insert OID = 142 (  int4fac           6 11 f t f 1 f 23 "23" 100 0 0 100  foo bar ));
DATA(insert OID = 143 (  pointdist         6 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 144 (  int4ne            6 11 f t f 2 f 16 "23 23" 100 0 0 100  foo bar ));
DATA(insert OID = 145 (  int2ne            6 11 f t f 2 f 16 "21 21" 100 0 0 100  foo bar ));
DATA(insert OID = 146 (  int2gt            6 11 f t f 2 f 16 "21 21" 100 0 0 100  foo bar ));
DATA(insert OID = 147 (  int4gt            6 11 f t f 2 f 16 "23 23" 100 0 0 100  foo bar ));
DATA(insert OID = 148 (  int2le            6 11 f t f 2 f 16 "21 21" 100 0 0 100  foo bar ));
DATA(insert OID = 149 (  int4le            6 11 f t f 2 f 16 "23 23" 100 0 0 100  foo bar ));
DATA(insert OID = 150 (  int4ge            6 11 f t f 2 f 16 "23 23" 100 0 0 100  foo bar ));
DATA(insert OID = 151 (  int2ge            6 11 f t f 2 f 16 "21 21" 100 0 0 100  foo bar ));
DATA(insert OID = 152 (  int2mul           6 11 f t f 2 f 21 "21 21" 100 0 0 100  foo bar ));
DATA(insert OID = 153 (  int2div           6 11 f t f 2 f 21 "21 21" 100 0 0 100  foo bar ));
DATA(insert OID = 154 (  int4div           6 11 f t f 2 f 23 "23 23" 100 0 0 100  foo bar ));
DATA(insert OID = 155 (  int2mod           6 11 f t f 2 f 21 "21 21" 100 0 0 100  foo bar ));
DATA(insert OID = 156 (  int4mod           6 11 f t f 2 f 23 "23 23" 100 0 0 100  foo bar ));
DATA(insert OID = 157 (  textne            6 11 f t f 2 f 16 "25 25" 100 0 0 0  foo bar ));
DATA(insert OID = 158 (  int24eq           6 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 159 (  int42eq           6 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 160 (  int24lt           6 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 161 (  int42lt           6 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 162 (  int24gt           6 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 163 (  int42gt           6 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 164 (  int24ne           6 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 165 (  int42ne           6 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 166 (  int24le           6 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 167 (  int42le           6 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 168 (  int24ge           6 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 169 (  int42ge           6 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 170 (  int24mul          6 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 171 (  int42mul          6 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 172 (  int24div          6 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 173 (  int42div          6 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 174 (  int24mod          6 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 175 (  int42mod          6 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 176 (  int2pl            6 11 f t f 2 f 21 "21 21" 100 0 0 100  foo bar ));
DATA(insert OID = 177 (  int4pl            6 11 f t f 2 f 23 "23 23" 100 0 0 100  foo bar ));
DATA(insert OID = 178 (  int24pl           6 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 179 (  int42pl           6 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 180 (  int2mi            6 11 f t f 2 f 21 "21 21" 100 0 0 100  foo bar ));
DATA(insert OID = 181 (  int4mi            6 11 f t f 2 f 23 "23 23" 100 0 0 100  foo bar ));
DATA(insert OID = 182 (  int24mi           6 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 183 (  int42mi           6 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 184 (  oideq             6 11 f t f 2 f 16 "26 26" 100 0 0 100  foo bar ));
#define ObjectIdEqualRegProcedure	184
    
DATA(insert OID = 185 (  oidneq            6 11 f t f 2 f 16 "26 26" 100 0 0 100  foo bar ));
DATA(insert OID = 186 (  box_same          6 11 f t f 2 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 187 (  box_contain       6 11 f t f 2 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 188 (  box_left          6 11 f t f 2 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 189 (  box_overleft      6 11 f t f 2 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 190 (  box_overright     6 11 f t f 2 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 191 (  box_right         6 11 f t f 2 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 192 (  box_contained     6 11 f t f 2 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 193 (  rt_box_union      6 11 f t f 2 f 603 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 194 (  rt_box_inter      6 11 f t f 2 f 603 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 195 (  rt_box_size       6 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 196 (  rt_bigbox_size    6 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 197 (  rt_poly_union     6 11 f t f 2 f 603 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 198 (  rt_poly_inter     6 11 f t f 2 f 603 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 199 (  rt_poly_size      6 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 200 (  float4in          6 11 f t f 1 f 700 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 201 (  float4out         6 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 202 (  float4mul         6 11 f t f 2 f 700 "700 700" 100 0 0 100  foo bar ));
DATA(insert OID = 203 (  float4div         6 11 f t f 2 f 700 "700 700" 100 0 0 100  foo bar ));
DATA(insert OID = 204 (  float4pl          6 11 f t f 2 f 700 "700 700" 100 0 0 100  foo bar ));
DATA(insert OID = 205 (  float4mi          6 11 f t f 2 f 700 "700 700" 100 0 0 100  foo bar ));
DATA(insert OID = 206 (  float4um          6 11 f t f 1 f 700 "700" 100 0 0 100  foo bar ));
DATA(insert OID = 207 (  float4abs         6 11 f t f 1 f 700 "700 700" 100 0 0 100  foo bar ));
DATA(insert OID = 208 (  float4inc         6 11 f t f 1 f 700 "700 700" 100 0 0 100  foo bar ));
DATA(insert OID = 209 (  float4larger      6 11 f t f 2 f 700 "700 700" 100 0 0 100  foo bar ));
DATA(insert OID = 211 (  float4smaller     6 11 f t f 2 f 700 "700 700" 100 0 0 100  foo bar ));

DATA(insert OID = 212 (  int4um            6 11 f t f 1 f 23 "23" 100 0 0 100  foo bar ));
DATA(insert OID = 213 (  int2um            6 11 f t f 1 f 21 "21" 100 0 0 100  foo bar ));
    
DATA(insert OID = 214 (  float8in          6 11 f t f 1 f 701 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 215 (  float8out         6 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 216 (  float8mul         6 11 f t f 2 f 701 "701 701" 100 0 0 100  foo bar ));
DATA(insert OID = 217 (  float8div         6 11 f t f 2 f 701 "701 701" 100 0 0 100  foo bar ));
DATA(insert OID = 218 (  float8pl          6 11 f t f 2 f 701 "701 701" 100 0 0 100  foo bar ));
DATA(insert OID = 219 (  float8mi          6 11 f t f 2 f 701 "701 701" 100 0 0 100  foo bar ));
DATA(insert OID = 220 (  float8um          6 11 f t f 1 f 701 "701" 100 0 0 100  foo bar ));
DATA(insert OID = 221 (  float8abs         6 11 f t f 1 f 701 "701" 100 0 0 100  foo bar ));
DATA(insert OID = 222 (  float8inc         6 11 f t f 1 f 701 "701" 100 0 0 100  foo bar ));
DATA(insert OID = 223 (  float8larger      6 11 f t f 2 f 701 "701 701" 100 0 0 100  foo bar ));
DATA(insert OID = 224 (  float8smaller     6 11 f t f 2 f 701 "701 701" 100 0 0 100  foo bar ));
DATA(insert OID = 228 (  dround            6 11 f t f 1 f 701 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 229 (  dtrunc            6 11 f t f 1 f 701 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 230 (  dsqrt             6 11 f t f 1 f 701 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 231 (  dcbrt             6 11 f t f 1 f 701 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 232 (  dpow              6 11 f t f 2 f 701 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 233 (  dexp              6 11 f t f 1 f 701 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 234 (  dlog1             6 11 f t f 1 f 701 "0" 100 0 0 100  foo bar ));
    
DATA(insert OID = 240 (  nabstimein         6 11 f t f 1 f 702 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 241 (  nabstimeout        6 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 242 (  reltimein         6 11 f t f 1 f 703 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 243 (  reltimeout        6 11 f t f 1 f 23 "703" 100 0 0 100  foo bar ));
DATA(insert OID = 244 (  timepl            6 11 f t f 2 f 702 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 245 (  timemi            6 11 f t f 2 f 702 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 246 (  tintervalin       6 11 f t f 1 f 704 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 247 (  tintervalout      6 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 248 (  ininterval        6 11 f t f 2 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 249 (  intervalrel       6 11 f t f 1 f 703 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 250 (  timenow           6 11 f t f 0 f 702 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 251 (  abstimeeq         6 11 f t f 2 f 16 "702 702" 100 0 0 100  foo bar ));
DATA(insert OID = 252 (  abstimene         6 11 f t f 2 f 16 "702 702" 100 0 0 100  foo bar ));
DATA(insert OID = 253 (  abstimelt         6 11 f t f 2 f 16 "702 702" 100 0 0 100  foo bar ));
DATA(insert OID = 254 (  abstimegt         6 11 f t f 2 f 16 "702 702" 100 0 0 100  foo bar ));
DATA(insert OID = 255 (  abstimele         6 11 f t f 2 f 16 "702 702" 100 0 0 100  foo bar ));
DATA(insert OID = 256 (  abstimege         6 11 f t f 2 f 16 "702 702" 100 0 0 100  foo bar ));
DATA(insert OID = 257 (  reltimeeq         6 11 f t f 2 f 16 "703 703" 100 0 0 100  foo bar ));
DATA(insert OID = 258 (  reltimene         6 11 f t f 2 f 16 "703 703" 100 0 0 100  foo bar ));
DATA(insert OID = 259 (  reltimelt         6 11 f t f 2 f 16 "703 703" 100 0 0 100  foo bar ));
DATA(insert OID = 260 (  reltimegt         6 11 f t f 2 f 16 "703 703" 100 0 0 100  foo bar ));
DATA(insert OID = 261 (  reltimele         6 11 f t f 2 f 16 "703 703" 100 0 0 100  foo bar ));
DATA(insert OID = 262 (  reltimege         6 11 f t f 2 f 16 "703 703" 100 0 0 100  foo bar ));
DATA(insert OID = 263 (  intervaleq        6 11 f t f 2 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 264 (  intervalct        6 11 f t f 2 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 265 (  intervalov        6 11 f t f 2 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 266 (  intervalleneq     6 11 f t f 2 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 267 (  intervallenne     6 11 f t f 2 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 268 (  intervallenlt     6 11 f t f 2 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 269 (  intervallengt     6 11 f t f 2 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 270 (  intervallenle     6 11 f t f 2 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 271 (  intervallenge     6 11 f t f 2 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 272 (  intervalstart     6 11 f t f 1 f 702 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 273 (  intervalend       6 11 f t f 1 f 702 "0" 100 0 0 100  foo bar ));
    
DATA(insert OID = 287 (  float4eq          6 11 f t f 2 f 16 "700 700" 100 0 0 100  foo bar ));
DATA(insert OID = 288 (  float4ne          6 11 f t f 2 f 16 "700 700" 100 0 0 100  foo bar ));
DATA(insert OID = 289 (  float4lt          6 11 f t f 2 f 16 "700 700" 100 0 0 100  foo bar ));
DATA(insert OID = 290 (  float4le          6 11 f t f 2 f 16 "700 700" 100 0 0 100  foo bar ));
DATA(insert OID = 291 (  float4gt          6 11 f t f 2 f 16 "700 700" 100 0 0 100  foo bar ));
DATA(insert OID = 292 (  float4ge          6 11 f t f 2 f 16 "700 700" 100 0 0 100  foo bar ));

DATA(insert OID = 293 (  float8eq          6 11 f t f 2 f 16 "701 701" 100 0 0 100  foo bar ));
DATA(insert OID = 294 (  float8ne          6 11 f t f 2 f 16 "701 701" 100 0 0 100  foo bar ));
DATA(insert OID = 295 (  float8lt          6 11 f t f 2 f 16 "701 701" 100 0 0 100  foo bar ));
DATA(insert OID = 296 (  float8le          6 11 f t f 2 f 16 "701 701" 100 0 0 100  foo bar ));
DATA(insert OID = 297 (  float8gt          6 11 f t f 2 f 16 "701 701" 100 0 0 100  foo bar ));
DATA(insert OID = 298 (  float8ge          6 11 f t f 2 f 16 "701 701" 100 0 0 100  foo bar ));

DATA(insert OID = 319 (  btreebeginscan    6 11 f t f 3 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 320 (  rtinsert          6 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 321 (  rtdelete          6 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 322 (  rtgettuple        6 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 323 (  rtbuild           6 11 f t f 9 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 324 (  rtbeginscan       6 11 f t f 4 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 325 (  rtendscan         6 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 326 (  rtmarkpos         6 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 327 (  rtrestrpos        6 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 328 (  rtrescan          6 11 f t f 3 f 23 "0" 100 0 0 100  foo bar ));

DATA(insert OID = 330 (  btgettuple        6 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 331 (  btinsert          6 11 f t f 3 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 332 (  btdelete          6 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 333 (  btbeginscan       6 11 f t f 4 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 334 (  btrescan          6 11 f t f 3 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 335 (  btendscan         6 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 336 (  btmarkpos         6 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 337 (  btrestrpos        6 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 338 (  btbuild           6 11 f t f 7 f 23 "0" 100 0 0 100  foo bar ));

DATA(insert OID = 339 (  poly_same         6 11 f t f 2 f 16 "0" 100 0 1 0  foo bar ));
DATA(insert OID = 340 (  poly_contain      6 11 f t f 2 f 16 "0" 100 0 1 0  foo bar ));
DATA(insert OID = 341 (  poly_left         6 11 f t f 2 f 16 "0" 100 0 1 0  foo bar ));
DATA(insert OID = 342 (  poly_overleft     6 11 f t f 2 f 16 "0" 100 0 1 0  foo bar ));
DATA(insert OID = 343 (  poly_overright    6 11 f t f 2 f 16 "0" 100 0 1 0  foo bar ));
DATA(insert OID = 344 (  poly_right        6 11 f t f 2 f 16 "0" 100 0 1 0  foo bar ));
DATA(insert OID = 345 (  poly_contained    6 11 f t f 2 f 16 "0" 100 0 1 0  foo bar ));
DATA(insert OID = 346 (  poly_overlap      6 11 f t f 2 f 16 "0" 100 0 1 0  foo bar ));
DATA(insert OID = 347 (  poly_in           6 11 f t f 1 f 604 "0" 100 0 1 0  foo bar ));
DATA(insert OID = 348 (  poly_out          6 11 f t f 1 f 23 "0" 100 0 1 0  foo bar ));

DATA(insert OID = 350 (  btint2cmp         6 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 351 (  btint4cmp         6 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 352 (  btint42cmp        6 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 353 (  btint24cmp        6 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 354 (  btfloat4cmp       6 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 355 (  btfloat8cmp       6 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 356 (  btoidcmp          6 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 357 (  btabstimecmp      6 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 358 (  btcharcmp         6 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 359 (  btchar16cmp       6 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 360 (  bttextcmp         6 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 361 (  lseg_distance     6 11 f t t 2 f 701 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 362 (  lseg_interpt      6 11 f t t 2 f 600 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 363 (  dist_ps           6 11 f t t 2 f 701 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 364 (  dist_pb           6 11 f t t 2 f 701 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 365 (  dist_sb           6 11 f t t 2 f 701 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 366 (  close_ps          6 11 f t t 2 f 600 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 367 (  close_pb          6 11 f t t 2 f 600 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 368 (  close_sb          6 11 f t t 2 f 600 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 369 (  on_ps             6 11 f t t 2 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 370 (  path_distance     6 11 f t t 2 f 701 "602 602" 100 0 1 0 foo bar ));
DATA(insert OID = 371 (  dist_ppth         6 11 f t t 2 f 701 "600 602" 100 0 1 0 foo bar ));
DATA(insert OID = 372 (  on_sb             6 11 f t t 2 f 701 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 373 (  inter_sb          6 11 f t t 2 f 16 "0" 100 0 0 100  foo bar ));

DATA(insert OID = 612 (  fbtreeinsert      6 11 f t f 4 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 613 (  fbtreedelete      6 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 614 (  fbtreegettuple    6 11 f t f 7 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 615 (  fbtreebuild       6 11 f t f 7 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 632 ( Negation           6 11 f t f 1 f 701 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 633 ( Conjunction        6 11 f t f 2 f 701 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 634 ( Disjunction        6 11 f t f 2 f 701 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 635 ( LInference         6 11 f t f 4 f 701 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 636 ( UInference         6 11 f t f 4 f 701 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 637 ( UIntersection      6 11 f t f 4 f 701 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 638 ( LIntersection      6 11 f t f 4 f 701 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 639 ( UDempster          6 11 f t f 4 f 701 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 640 ( LDempster          6 11 f t f 4 f 701 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 649 ( GetAttributeByName 6 11 f t f 3 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 650 (  int4notin         6 11 f t f 2 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 651 (  oidnotin          6 11 f t f 2 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 652 (  int44in           6 11 f t f 1 f 22 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 653 (  int44out          6 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 654 (  GetAttributeByNum 6 11 f t f 3 f 23 "0" 100 0 0 100  foo bar ));

DATA(insert OID = 655 (  char16lt          6 11 f t f 2 f 16 "19 19" 100 0 0 100  foo bar ));
DATA(insert OID = 656 (  char16le          6 11 f t f 2 f 16 "19 19" 100 0 0 100  foo bar ));
DATA(insert OID = 657 (  char16gt          6 11 f t f 2 f 16 "19 19" 100 0 0 100  foo bar ));
DATA(insert OID = 658 (  char16ge          6 11 f t f 2 f 16 "19 19" 100 0 0 100  foo bar ));
DATA(insert OID = 659 (  char16ne          6 11 f t f 2 f 16 "19 19" 100 0 0 100  foo bar ));

DATA(insert OID = 700 (  lockadd           6 11 f t f 2 f 31 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 701 (  lockrm            6 11 f t f 2 f 31 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 710 (  pg_username       6 11 f t t 0 f 19 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 711 (  userfntest        6 11 f t t 1 f 23 "0" 100 0 0 100  foo bar ));

DATA(insert OID = 720 (  byteasize	   6 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 721 (  byteagetbyte	   6 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 722 (  byteasetbyte	   6 11 f t f 3 f 17 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 723 (  byteagetbit	   6 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 724 (  byteasetbit	   6 11 f t f 3 f 17 "0" 100 0 0 100  foo bar ));

DATA(insert OID = 730 (  pqtest            6 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));

DATA(insert OID = 740 (  text_lt           6 11 f t f 2 f 16 "25 25" 100 0 0 0  foo bar ));
DATA(insert OID = 741 (  text_le           6 11 f t f 2 f 16 "25 25" 100 0 0 0  foo bar ));
DATA(insert OID = 742 (  text_gt           6 11 f t f 2 f 16 "25 25" 100 0 0 0  foo bar ));
DATA(insert OID = 743 (  text_ge           6 11 f t f 2 f 16 "25 25" 100 0 0 0  foo bar ));

DATA(insert OID = 750 (  array_in          6 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 751 (  array_out         6 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 752 (  filename_in       6 11 f t f 2 f 605 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 753 (  filename_out      6 11 f t f 2 f 19 "0" 100 0 0 100  foo bar ));

DATA(insert OID = 760 (  smgrin		   6 11 f t f 1 f 210 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 761 (  smgrout	   6 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 762 (  smgreq		   6 11 f t f 2 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 763 (  smgrne		   6 11 f t f 2 f 16 "0" 100 0 0 100  foo bar ));

DATA(insert OID = 764 (  lo_filein         6 11 f t f 1 f 605 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 765 (  lo_fileout        6 11 f t f 1 f 19 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 766 (  int4inc           6 11 f t f 1 f 23 "23" 100 0 0 100  foo bar ));
DATA(insert OID = 767 (  int2inc           6 11 f t f 1 f 21 "21" 100 0 0 100  foo bar ));
DATA(insert OID = 768 (  int4larger        6 11 f t f 2 f 23 "23 23" 100 0 0 100  foo bar ));
DATA(insert OID = 769 (  int4smaller       6 11 f t f 2 f 23 "23 23" 100 0 0 100  foo bar ));
DATA(insert OID = 770 (  int2larger        6 11 f t f 2 f 23 "21 21" 100 0 0 100  foo bar ));
DATA(insert OID = 771 (  int2smaller       6 11 f t f 2 f 23 "21 21" 100 0 0 100  foo bar ));

BKI_BEGIN
#ifdef NOBTREE
BKI_END
DATA(insert OID = 800 (  nobtgettuple      6 11 f t f 6 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 801 (  nobtinsert        6 11 f t f 3 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 802 (  nobtdelete        6 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 803 (  nobtbeginscan     6 11 f t f 4 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 804 (  nobtrescan        6 11 f t f 3 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 805 (  nobtendscan       6 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 806 (  nobtmarkpos       6 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 807 (  nobtrestrpos      6 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 808 (  nobtbuild         6 11 f t f 7 f 23 "0" 100 0 0 100  foo bar ));
BKI_BEGIN
#endif /* NOBTREE */
BKI_END

DATA(insert OID = 900 (  fimport           6 11 f t f 1 f 26 "25" 100 0 0 100  foo bar ));
DATA(insert OID = 901 (  fexport           6 11 f t f 2 f 23 "25 26" 100 0 0 100  foo bar ));
DATA(insert OID = 902 (  fabstract         6 11 f t f 5 f 23 "25 26 23 23 23" 100 0 0 100  foo bar ));

DATA(insert OID = 920 (  oidint4in	   6 11 f t f 1 f 910 "23" 100 0 0 100  foo bar));
DATA(insert OID = 921 (  oidint4out	   6 11 f t f 1 f 19 "910" 100 0 0 100  foo bar));
DATA(insert OID = 922 (  oidint4lt	   6 11 f t f 2 f 16 "0" 100 0 0 100  foo bar));
DATA(insert OID = 923 (  oidint4le	   6 11 f t f 2 f 16 "0" 100 0 0 100  foo bar));
DATA(insert OID = 924 (  oidint4eq	   6 11 f t f 2 f 16 "0" 100 0 0 100  foo bar));

#define OidInt4EqRegProcedure 924

DATA(insert OID = 925 (  oidint4ge	   6 11 f t f 2 f 16 "0" 100 0 0 100  foo bar));
DATA(insert OID = 926 (  oidint4gt	   6 11 f t f 2 f 16 "0" 100 0 0 100  foo bar));
DATA(insert OID = 927 (  oidint4ne	   6 11 f t f 2 f 16 "0" 100 0 0 100  foo bar));
DATA(insert OID = 928 (  oidint4cmp	   6 11 f t f 2 f 23 "0" 100 0 0 100  foo bar));
DATA(insert OID = 929 (  mkoidint4	   6 11 f t f 2 f 910 "26 23" 100 0 0 100  foo bar));

DATA(insert OID = 940 (  oidchar16in	   6 11 f t f 1 f 911 "23" 100 0 0 100  foo bar));
DATA(insert OID = 941 (  oidchar16out	   6 11 f t f 1 f 19 "910" 100 0 0 100  foo bar));
DATA(insert OID = 942 (  oidchar16lt	   6 11 f t f 2 f 16 "0" 100 0 0 100  foo bar));
DATA(insert OID = 943 (  oidchar16le	   6 11 f t f 2 f 16 "0" 100 0 0 100  foo bar));
DATA(insert OID = 944 (  oidchar16eq	   6 11 f t f 2 f 16 "0" 100 0 0 100  foo bar));

#define OidChar16EqRegProcedure 944

DATA(insert OID = 945 (  oidchar16ge	   6 11 f t f 2 f 16 "0" 100 0 0 100  foo bar));
DATA(insert OID = 946 (  oidchar16gt	   6 11 f t f 2 f 16 "0" 100 0 0 100  foo bar));
DATA(insert OID = 947 (  oidchar16ne	   6 11 f t f 2 f 16 "0" 100 0 0 100  foo bar));
DATA(insert OID = 948 (  oidchar16cmp	   6 11 f t f 2 f 23 "0" 100 0 0 100  foo bar));
DATA(insert OID = 949 (  mkoidchar16	   6 11 f t f 2 f 911 "26 23" 100 0 0 100  foo bar));

DATA(insert OID = 950 (  filetooid         6 11 f t f 1 f 26 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 951 (  locreatoid        6 11 f t f 1 f 26 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 952 (  loopen            6 11 f t f 2 f 23 "25 23" 100 0 0 100  foo bar ));
DATA(insert OID = 953 (  loclose           6 11 f t f 1 f 23 "23" 100 0 0 100  foo bar ));
DATA(insert OID = 954 (  loread            6 11 f t f 2 f 17 "0 0" 100 0 0 100  foo bar ));
DATA(insert OID = 955 (  lowrite           6 11 f t f 2 f 23 "0 0" 100 0 0 100  foo bar ));
DATA(insert OID = 956 (  lolseek           6 11 f t f 3 f 23 "0 0 0" 100 0 0 100  foo bar ));
DATA(insert OID = 957 (  locreat           6 11 f t f 3 f 23 "0 0 0" 100 0 0 100  foo bar ));
DATA(insert OID = 958 (  lotell            6 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 959 (  loftruncate       6 11 f t f 2 f 23 "0 0" 100 0 0 100  foo bar ));
DATA(insert OID = 960 (  lostat            6 11 f t f 1 f 17 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 961 (  lorename          6 11 f t f 2 f 23 "0 0" 100 0 0 100  foo bar ));
DATA(insert OID = 962 (  lomkdir           6 11 f t f 2 f 23 "0 0" 100 0 0 100  foo bar ));
DATA(insert OID = 963 (  lormdir           6 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 964 (  lounlink          6 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));

DATA(insert OID = 970 (  pftp_read         6 11 f t f 3 f 23 "25 23 26" 100 0 1 0  foo bar ));
DATA(insert OID = 971 (  pftp_write        6 11 f t f 2 f 26 "25 23" 100 0 1 0  foo bar ));

DATA(insert OID = 972 (  RegprocToOid      6 11 f t t 1 f 26 "24" 100 0 0 100  foo bar ));

DATA(insert OID = 973 (  path_inter        6 11 f t t 2 f 16 "0" 100 0 10 100  foo bar ));
DATA(insert OID = 974 (  box_copy          6 11 f t t 1 f 603 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 975 (  box_area          6 11 f t t 1 f 701 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 976 (  box_length        6 11 f t t 1 f 701 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 977 (  box_height        6 11 f t t 1 f 701 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 978 (  box_distance      6 11 f t t 2 f 701 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 980 (  box_intersect     6 11 f t t 2 f 603 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 981 (  box_diagonal      6 11 f t t 1 f 601 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 982 (  path_n_lt         6 11 f t t 2 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 983 (  path_n_gt         6 11 f t t 2 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 984 (  path_n_eq         6 11 f t t 2 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 985 (  path_n_le         6 11 f t t 2 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 986 (  path_n_ge         6 11 f t t 2 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 987 (  path_length       6 11 f t t 1 f 701 "0" 100 0 1 0  foo bar ));
DATA(insert OID = 988 (  point_copy        6 11 f t t 1 f 600 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 989 (  point_vert        6 11 f t t 2 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 990 (  point_horiz       6 11 f t t 2 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 991 (  point_distance    6 11 f t t 2 f 701 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 992 (  point_slope       6 11 f t t 2 f 701 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 993 (  lseg_construct    6 11 f t t 2 f 601 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 994 (  lseg_intersect    6 11 f t t 2 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 995 (  lseg_parallel     6 11 f t t 2 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 996 (  lseg_perp         6 11 f t t 2 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 997 (  lseg_vertical     6 11 f t t 1 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 998 (  lseg_horizontal   6 11 f t t 1 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 999 (  lseg_eq           6 11 f t t 2 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 1029 (  NullValue        6 11 f t f 1 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 1030 (  NonNullValue     6 11 f t f 1 f 16 "0" 100 0 0 100  foo bar ));

/* ----------------
 *	old definition of struct proc
 * ----------------
 */
#ifndef struct_proc_Defined
#define struct_proc_Defined 1

struct	proc {
	char	proname[16];
	OID	proowner;
	OID	prolang;
	Boolean	proisinh;
	Boolean	proistrusted;
	Boolean	proiscachable;
	uint16	pronargs;
	Boolean	proretset;
	OID	prorettype;
	oid8    proargtypes;
	uint32  probyte_pct;
	uint32  properbyte_cpu;
	uint32  propercall_cpu;
	uint32  prooutin_ratio;
}; /* VARIABLE LENGTH STRUCTURE */

#endif struct_proc_Defined

    
/* ----------------
 *	old style compiler constants.  these are obsolete and
 *	should not be used -cim 6/17/90
 * ----------------
 */
#define	ProcedureNameAttributeNumber \
    Anum_pg_proc_proname
#define	ProcedureReturnTypeAttributeNumber \
    Anum_pg_proc_prorettype
#define	ProcedureBinaryAttributeNumber \
    Anum_pg_proc_probin
    
#define ProcedureRelationNumberOfAttributes \
    Natts_pg_proc

#include "nodes/pg_lisp.h"
/* pg_proc.c */
void ProcedureDefine ARGS((Name procedureName , bool returnsSet , Name returnTypeName , Name languageName , char *prosrc , char *probin , Boolean canCache , int32 byte_pct , int32 perbyte_cpu , int32 percall_cpu , int32 outin_ratio , List argList , CommandDest dest));
#endif PgProcIncluded
