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

DATA(insert OID =  28 (  boolin            PGUID 11 f t f 1 f 16 "0" 100 0 0  100  foo bar ));
DATA(insert OID =  29 (  boolout           PGUID 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID =  30 (  byteain           PGUID 11 f t f 1 f 17 "0" 100 0 0 100  foo bar ));
DATA(insert OID =  31 (  byteaout          PGUID 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID =  32 (  charin            PGUID 11 f t f 1 f 18 "0" 100 0 0 100  foo bar ));
DATA(insert OID =  33 (  charout           PGUID 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID =  34 (  char16in          PGUID 11 f t f 1 f 19 "0" 100 0 0 100  foo bar ));
DATA(insert OID =  35 (  char16out         PGUID 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID =  36 (  dtin              PGUID 11 f t f 1 f 20 "0" 100 0 0 100  foo bar ));
DATA(insert OID =  37 (  dtout             PGUID 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID =  38 (  int2in            PGUID 11 f t f 1 f 21 "0" 100 0 0 100  foo bar ));
DATA(insert OID =  39 (  int2out           PGUID 11 f t f 1 f 23 "21" 100 0 0 100  foo bar ));
DATA(insert OID =  40 (  int28in           PGUID 11 f t f 1 f 22 "0" 100 0 0 100  foo bar ));
DATA(insert OID =  41 (  int28out          PGUID 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID =  42 (  int4in            PGUID 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID =  43 (  int4out           PGUID 11 f t f 1 f 19 "23" 100 0 0 100  foo bar ));
DATA(insert OID =  44 (  regprocin         PGUID 11 f t f 1 f 24 "0" 100 0 0 100  foo bar ));
DATA(insert OID =  45 (  regprocout        PGUID 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID =  46 (  textin            PGUID 11 f t f 1 f 25 "0" 100 0 0 100  foo bar ));
#define TextInRegProcedure 46

DATA(insert OID =  47 (  textout           PGUID 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID =  48 (  tidin             PGUID 11 f t f 1 f 27 "0" 100 0 0 100  foo bar ));
DATA(insert OID =  49 (  tidout            PGUID 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID =  50 (  xidin             PGUID 11 f t f 1 f 28 "0" 100 0 0 100  foo bar ));
DATA(insert OID =  51 (  xidout            PGUID 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID =  52 (  cidin             PGUID 11 f t f 1 f 29 "0" 100 0 0 100  foo bar ));
DATA(insert OID =  53 (  cidout            PGUID 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID =  54 (  oid8in            PGUID 11 f t f 1 f 30 "0" 100 0 0 100  foo bar ));
DATA(insert OID =  55 (  oid8out           PGUID 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID =  56 (  lockin            PGUID 11 f t f 1 f 31 "0" 100 0 0 100  foo bar ));
DATA(insert OID =  57 (  lockout           PGUID 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID =  58 (  stubin            PGUID 11 f t f 1 f 33 "0" 100 0 0 100  foo bar ));
DATA(insert OID =  59 (  stubout           PGUID 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID =  60 (  booleq            PGUID 11 f t f 2 f 16 "16 16" 100 0 0 100  foo bar ));
DATA(insert OID =  61 (  chareq            PGUID 11 f t f 2 f 16 "18 18" 100 0 0 100  foo bar ));
#define       CharacterEqualRegProcedure      61

DATA(insert OID =  62 (  char16eq          PGUID 11 f t f 2 f 16 "19 19" 100 0 0 100  foo bar ));
#define NameEqualRegProcedure		62
#define Character16EqualRegProcedure	62
    
DATA(insert OID =  63 (  int2eq            PGUID 11 f t f 2 f 16 "21 21" 100 0 0 100  foo bar ));
#define Integer16EqualRegProcedure	63
    
DATA(insert OID =  64 (  int2lt            PGUID 11 f t f 2 f 16 "21 21" 100 0 0 100  foo bar ));
DATA(insert OID =  65 (  int4eq            PGUID 11 f t f 2 f 16 "23 23" 100 0 0 100  foo bar ));
#define Integer32EqualRegProcedure	65
    
DATA(insert OID =  66 (  int4lt            PGUID 11 f t f 2 f 16 "23 23" 100 0 0 100  foo bar ));
DATA(insert OID =  67 (  texteq            PGUID 11 f t f 2 f 16 "25 25" 100 0 0 0  foo bar ));
DATA(insert OID =  68 (  xideq             PGUID 11 f t f 2 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID =  69 (  cideq             PGUID 11 f t f 2 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID =  70 (  charne            PGUID 11 f t f 2 f 16 "18 18" 100 0 0 100  foo bar ));
DATA(insert OID =  71 (  charlt            PGUID 11 f t f 2 f 16 "18 18" 100 0 0 100  foo bar ));
DATA(insert OID =  72 (  charle            PGUID 11 f t f 2 f 16 "18 18" 100 0 0 100  foo bar ));
DATA(insert OID =  73 (  chargt            PGUID 11 f t f 2 f 16 "18 18" 100 0 0 100  foo bar ));
DATA(insert OID =  74 (  charge            PGUID 11 f t f 2 f 16 "18 18" 100 0 0 100  foo bar ));
DATA(insert OID =  75 (  charpl            PGUID 11 f t f 2 f 16 "18 18" 100 0 0 100  foo bar ));
DATA(insert OID =  76 (  charmi            PGUID 11 f t f 2 f 16 "18 18" 100 0 0 100  foo bar ));
DATA(insert OID =  77 (  charmul           PGUID 11 f t f 2 f 16 "18 18" 100 0 0 100  foo bar ));
DATA(insert OID =  78 (  chardiv           PGUID 11 f t f 2 f 16 "18 18" 100 0 0 100  foo bar ));

DATA(insert OID =  79 (  char16regexeq     PGUID 11 f t f 2 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID =  80 (  char16regexne     PGUID 11 f t f 2 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID =  81 (  textregexeq       PGUID 11 f t f 2 f 16 "0" 100 0 1 0  foo bar ));
DATA(insert OID =  82 (  textregexne       PGUID 11 f t f 2 f 16 "0" 100 0 1 0  foo bar ));
DATA(insert OID =  84 (  boolne            PGUID 11 f t f 2 f 16 "16 16" 100 0 0 100  foo bar ));

DATA(insert OID =  97 (  rtsel             PGUID 11 f t f 7 f 109 "0" 100 0 0 100  foo bar ));
DATA(insert OID =  98 (  rtnpage           PGUID 11 f t f 7 f 109 "0" 100 0 0 100  foo bar ));
DATA(insert OID =  99 (  btreesel          PGUID 11 f t f 7 f 109 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 100 (  btreenpage        PGUID 11 f t f 7 f 109 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 101 (  eqsel             PGUID 11 f t f 5 f 109 "0" 100 0 0 100  foo bar ));
#define EqualSelectivityProcedure 101

DATA(insert OID = 102 (  neqsel            PGUID 11 f t f 5 f 109 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 103 (  intltsel          PGUID 11 f t f 5 f 109 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 104 (  intgtsel          PGUID 11 f t f 5 f 109 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 105 (  eqjoinsel         PGUID 11 f t f 5 f 109 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 106 (  neqjoinsel        PGUID 11 f t f 5 f 109 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 107 (  intltjoinsel      PGUID 11 f t f 5 f 109 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 108 (  intgtjoinsel      PGUID 11 f t f 5 f 109 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 109 (  btreeendscan      PGUID 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 110 (  btreemarkpos      PGUID 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 111 (  btreerestrpos     PGUID 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 112 (  btreeinsert       PGUID 11 f t f 3 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 113 (  btreedelete       PGUID 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 114 (  btreegettuple     PGUID 11 f t f 6 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 115 (  btreebuild        PGUID 11 f t f 7 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 116 (  btreerescan       PGUID 11 f t f 3 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 117 (  point_in          PGUID 11 f t f 1 f 600 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 118 (  point_out         PGUID 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 119 (  lseg_in           PGUID 11 f t f 1 f 601 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 120 (  lseg_out          PGUID 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 121 (  path_in           PGUID 11 f t f 1 f 602 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 122 (  path_out          PGUID 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 123 (  box_in            PGUID 11 f t f 1 f 603 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 124 (  box_out           PGUID 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 125 (  box_overlap       PGUID 11 f t f 2 f 16 "0" 100 1 0 100  foo bar ));
DATA(insert OID = 126 (  box_ge            PGUID 11 f t f 2 f 16 "0" 100 1 0 100  foo bar ));
DATA(insert OID = 127 (  box_gt            PGUID 11 f t f 2 f 16 "0" 100 1 0 100  foo bar ));
DATA(insert OID = 128 (  box_eq            PGUID 11 f t f 2 f 16 "0" 100 1 0 100  foo bar ));
DATA(insert OID = 129 (  box_lt            PGUID 11 f t f 2 f 16 "0" 100 1 0 100  foo bar ));
DATA(insert OID = 130 (  box_le            PGUID 11 f t f 2 f 16 "0" 100 1 0 100  foo bar ));
DATA(insert OID = 131 (  point_above       PGUID 11 f t f 2 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 132 (  point_left        PGUID 11 f t f 2 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 133 (  point_right       PGUID 11 f t f 2 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 134 (  point_below       PGUID 11 f t f 2 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 135 (  point_eq          PGUID 11 f t f 2 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 136 (  on_pb             PGUID 11 f t f 2 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 137 (  on_ppath          PGUID 11 f t f 2 f 16 "0" 100 0 1 0  foo bar ));
DATA(insert OID = 138 (  box_center        PGUID 11 f t f 1 f 600 "0" 100 1 0 100  foo bar ));
DATA(insert OID = 139 (  areasel           PGUID 11 f t f 5 f 109 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 140 (  areajoinsel       PGUID 11 f t f 5 f 109 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 141 (  int4mul           PGUID 11 f t f 2 f 23 "23 23" 100 0 0 100  foo bar ));
DATA(insert OID = 142 (  int4fac           PGUID 11 f t f 1 f 23 "23" 100 0 0 100  foo bar ));
DATA(insert OID = 143 (  pointdist         PGUID 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 144 (  int4ne            PGUID 11 f t f 2 f 16 "23 23" 100 0 0 100  foo bar ));
DATA(insert OID = 145 (  int2ne            PGUID 11 f t f 2 f 16 "21 21" 100 0 0 100  foo bar ));
DATA(insert OID = 146 (  int2gt            PGUID 11 f t f 2 f 16 "21 21" 100 0 0 100  foo bar ));
DATA(insert OID = 147 (  int4gt            PGUID 11 f t f 2 f 16 "23 23" 100 0 0 100  foo bar ));
DATA(insert OID = 148 (  int2le            PGUID 11 f t f 2 f 16 "21 21" 100 0 0 100  foo bar ));
DATA(insert OID = 149 (  int4le            PGUID 11 f t f 2 f 16 "23 23" 100 0 0 100  foo bar ));
DATA(insert OID = 150 (  int4ge            PGUID 11 f t f 2 f 16 "23 23" 100 0 0 100  foo bar ));
DATA(insert OID = 151 (  int2ge            PGUID 11 f t f 2 f 16 "21 21" 100 0 0 100  foo bar ));
DATA(insert OID = 152 (  int2mul           PGUID 11 f t f 2 f 21 "21 21" 100 0 0 100  foo bar ));
DATA(insert OID = 153 (  int2div           PGUID 11 f t f 2 f 21 "21 21" 100 0 0 100  foo bar ));
DATA(insert OID = 154 (  int4div           PGUID 11 f t f 2 f 23 "23 23" 100 0 0 100  foo bar ));
DATA(insert OID = 155 (  int2mod           PGUID 11 f t f 2 f 21 "21 21" 100 0 0 100  foo bar ));
DATA(insert OID = 156 (  int4mod           PGUID 11 f t f 2 f 23 "23 23" 100 0 0 100  foo bar ));
DATA(insert OID = 157 (  textne            PGUID 11 f t f 2 f 16 "25 25" 100 0 0 0  foo bar ));
DATA(insert OID = 158 (  int24eq           PGUID 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 159 (  int42eq           PGUID 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 160 (  int24lt           PGUID 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 161 (  int42lt           PGUID 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 162 (  int24gt           PGUID 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 163 (  int42gt           PGUID 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 164 (  int24ne           PGUID 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 165 (  int42ne           PGUID 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 166 (  int24le           PGUID 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 167 (  int42le           PGUID 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 168 (  int24ge           PGUID 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 169 (  int42ge           PGUID 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 170 (  int24mul          PGUID 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 171 (  int42mul          PGUID 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 172 (  int24div          PGUID 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 173 (  int42div          PGUID 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 174 (  int24mod          PGUID 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 175 (  int42mod          PGUID 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 176 (  int2pl            PGUID 11 f t f 2 f 21 "21 21" 100 0 0 100  foo bar ));
DATA(insert OID = 177 (  int4pl            PGUID 11 f t f 2 f 23 "23 23" 100 0 0 100  foo bar ));
DATA(insert OID = 178 (  int24pl           PGUID 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 179 (  int42pl           PGUID 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 180 (  int2mi            PGUID 11 f t f 2 f 21 "21 21" 100 0 0 100  foo bar ));
DATA(insert OID = 181 (  int4mi            PGUID 11 f t f 2 f 23 "23 23" 100 0 0 100  foo bar ));
DATA(insert OID = 182 (  int24mi           PGUID 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 183 (  int42mi           PGUID 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 184 (  oideq             PGUID 11 f t f 2 f 16 "26 26" 100 0 0 100  foo bar ));
#define ObjectIdEqualRegProcedure	184
    
DATA(insert OID = 185 (  oidneq            PGUID 11 f t f 2 f 16 "26 26" 100 0 0 100  foo bar ));
DATA(insert OID = 186 (  box_same          PGUID 11 f t f 2 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 187 (  box_contain       PGUID 11 f t f 2 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 188 (  box_left          PGUID 11 f t f 2 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 189 (  box_overleft      PGUID 11 f t f 2 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 190 (  box_overright     PGUID 11 f t f 2 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 191 (  box_right         PGUID 11 f t f 2 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 192 (  box_contained     PGUID 11 f t f 2 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 193 (  rt_box_union      PGUID 11 f t f 2 f 603 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 194 (  rt_box_inter      PGUID 11 f t f 2 f 603 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 195 (  rt_box_size       PGUID 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 196 (  rt_bigbox_size    PGUID 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 197 (  rt_poly_union     PGUID 11 f t f 2 f 603 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 198 (  rt_poly_inter     PGUID 11 f t f 2 f 603 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 199 (  rt_poly_size      PGUID 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 200 (  float4in          PGUID 11 f t f 1 f 700 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 201 (  float4out         PGUID 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 202 (  float4mul         PGUID 11 f t f 2 f 700 "700 700" 100 0 0 100  foo bar ));
DATA(insert OID = 203 (  float4div         PGUID 11 f t f 2 f 700 "700 700" 100 0 0 100  foo bar ));
DATA(insert OID = 204 (  float4pl          PGUID 11 f t f 2 f 700 "700 700" 100 0 0 100  foo bar ));
DATA(insert OID = 205 (  float4mi          PGUID 11 f t f 2 f 700 "700 700" 100 0 0 100  foo bar ));
DATA(insert OID = 206 (  float4um          PGUID 11 f t f 1 f 700 "700" 100 0 0 100  foo bar ));
DATA(insert OID = 207 (  float4abs         PGUID 11 f t f 1 f 700 "700 700" 100 0 0 100  foo bar ));
DATA(insert OID = 208 (  float4inc         PGUID 11 f t f 1 f 700 "700 700" 100 0 0 100  foo bar ));
DATA(insert OID = 209 (  float4larger      PGUID 11 f t f 2 f 700 "700 700" 100 0 0 100  foo bar ));
DATA(insert OID = 211 (  float4smaller     PGUID 11 f t f 2 f 700 "700 700" 100 0 0 100  foo bar ));

DATA(insert OID = 212 (  int4um            PGUID 11 f t f 1 f 23 "23" 100 0 0 100  foo bar ));
DATA(insert OID = 213 (  int2um            PGUID 11 f t f 1 f 21 "21" 100 0 0 100  foo bar ));
    
DATA(insert OID = 214 (  float8in          PGUID 11 f t f 1 f 701 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 215 (  float8out         PGUID 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 216 (  float8mul         PGUID 11 f t f 2 f 701 "701 701" 100 0 0 100  foo bar ));
DATA(insert OID = 217 (  float8div         PGUID 11 f t f 2 f 701 "701 701" 100 0 0 100  foo bar ));
DATA(insert OID = 218 (  float8pl          PGUID 11 f t f 2 f 701 "701 701" 100 0 0 100  foo bar ));
DATA(insert OID = 219 (  float8mi          PGUID 11 f t f 2 f 701 "701 701" 100 0 0 100  foo bar ));
DATA(insert OID = 220 (  float8um          PGUID 11 f t f 1 f 701 "701" 100 0 0 100  foo bar ));
DATA(insert OID = 221 (  float8abs         PGUID 11 f t f 1 f 701 "701" 100 0 0 100  foo bar ));
DATA(insert OID = 222 (  float8inc         PGUID 11 f t f 1 f 701 "701" 100 0 0 100  foo bar ));
DATA(insert OID = 223 (  float8larger      PGUID 11 f t f 2 f 701 "701 701" 100 0 0 100  foo bar ));
DATA(insert OID = 224 (  float8smaller     PGUID 11 f t f 2 f 701 "701 701" 100 0 0 100  foo bar ));
DATA(insert OID = 228 (  dround            PGUID 11 f t f 1 f 701 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 229 (  dtrunc            PGUID 11 f t f 1 f 701 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 230 (  dsqrt             PGUID 11 f t f 1 f 701 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 231 (  dcbrt             PGUID 11 f t f 1 f 701 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 232 (  dpow              PGUID 11 f t f 2 f 701 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 233 (  dexp              PGUID 11 f t f 1 f 701 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 234 (  dlog1             PGUID 11 f t f 1 f 701 "0" 100 0 0 100  foo bar ));
    
DATA(insert OID = 240 (  nabstimein         PGUID 11 f t f 1 f 702 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 241 (  nabstimeout        PGUID 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 242 (  reltimein         PGUID 11 f t f 1 f 703 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 243 (  reltimeout        PGUID 11 f t f 1 f 23 "703" 100 0 0 100  foo bar ));
DATA(insert OID = 244 (  timepl            PGUID 11 f t f 2 f 702 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 245 (  timemi            PGUID 11 f t f 2 f 702 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 246 (  tintervalin       PGUID 11 f t f 1 f 704 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 247 (  tintervalout      PGUID 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 248 (  ininterval        PGUID 11 f t f 2 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 249 (  intervalrel       PGUID 11 f t f 1 f 703 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 250 (  timenow           PGUID 11 f t f 0 f 702 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 251 (  abstimeeq         PGUID 11 f t f 2 f 16 "702 702" 100 0 0 100  foo bar ));
DATA(insert OID = 252 (  abstimene         PGUID 11 f t f 2 f 16 "702 702" 100 0 0 100  foo bar ));
DATA(insert OID = 253 (  abstimelt         PGUID 11 f t f 2 f 16 "702 702" 100 0 0 100  foo bar ));
DATA(insert OID = 254 (  abstimegt         PGUID 11 f t f 2 f 16 "702 702" 100 0 0 100  foo bar ));
DATA(insert OID = 255 (  abstimele         PGUID 11 f t f 2 f 16 "702 702" 100 0 0 100  foo bar ));
DATA(insert OID = 256 (  abstimege         PGUID 11 f t f 2 f 16 "702 702" 100 0 0 100  foo bar ));
DATA(insert OID = 257 (  reltimeeq         PGUID 11 f t f 2 f 16 "703 703" 100 0 0 100  foo bar ));
DATA(insert OID = 258 (  reltimene         PGUID 11 f t f 2 f 16 "703 703" 100 0 0 100  foo bar ));
DATA(insert OID = 259 (  reltimelt         PGUID 11 f t f 2 f 16 "703 703" 100 0 0 100  foo bar ));
DATA(insert OID = 260 (  reltimegt         PGUID 11 f t f 2 f 16 "703 703" 100 0 0 100  foo bar ));
DATA(insert OID = 261 (  reltimele         PGUID 11 f t f 2 f 16 "703 703" 100 0 0 100  foo bar ));
DATA(insert OID = 262 (  reltimege         PGUID 11 f t f 2 f 16 "703 703" 100 0 0 100  foo bar ));
DATA(insert OID = 263 (  intervaleq        PGUID 11 f t f 2 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 264 (  intervalct        PGUID 11 f t f 2 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 265 (  intervalov        PGUID 11 f t f 2 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 266 (  intervalleneq     PGUID 11 f t f 2 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 267 (  intervallenne     PGUID 11 f t f 2 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 268 (  intervallenlt     PGUID 11 f t f 2 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 269 (  intervallengt     PGUID 11 f t f 2 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 270 (  intervallenle     PGUID 11 f t f 2 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 271 (  intervallenge     PGUID 11 f t f 2 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 272 (  intervalstart     PGUID 11 f t f 1 f 702 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 273 (  intervalend       PGUID 11 f t f 1 f 702 "0" 100 0 0 100  foo bar ));
    
DATA(insert OID = 287 (  float4eq          PGUID 11 f t f 2 f 16 "700 700" 100 0 0 100  foo bar ));
DATA(insert OID = 288 (  float4ne          PGUID 11 f t f 2 f 16 "700 700" 100 0 0 100  foo bar ));
DATA(insert OID = 289 (  float4lt          PGUID 11 f t f 2 f 16 "700 700" 100 0 0 100  foo bar ));
DATA(insert OID = 290 (  float4le          PGUID 11 f t f 2 f 16 "700 700" 100 0 0 100  foo bar ));
DATA(insert OID = 291 (  float4gt          PGUID 11 f t f 2 f 16 "700 700" 100 0 0 100  foo bar ));
DATA(insert OID = 292 (  float4ge          PGUID 11 f t f 2 f 16 "700 700" 100 0 0 100  foo bar ));

DATA(insert OID = 293 (  float8eq          PGUID 11 f t f 2 f 16 "701 701" 100 0 0 100  foo bar ));
DATA(insert OID = 294 (  float8ne          PGUID 11 f t f 2 f 16 "701 701" 100 0 0 100  foo bar ));
DATA(insert OID = 295 (  float8lt          PGUID 11 f t f 2 f 16 "701 701" 100 0 0 100  foo bar ));
DATA(insert OID = 296 (  float8le          PGUID 11 f t f 2 f 16 "701 701" 100 0 0 100  foo bar ));
DATA(insert OID = 297 (  float8gt          PGUID 11 f t f 2 f 16 "701 701" 100 0 0 100  foo bar ));
DATA(insert OID = 298 (  float8ge          PGUID 11 f t f 2 f 16 "701 701" 100 0 0 100  foo bar ));

DATA(insert OID = 319 (  btreebeginscan    PGUID 11 f t f 3 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 320 (  rtinsert          PGUID 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 321 (  rtdelete          PGUID 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 322 (  rtgettuple        PGUID 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 323 (  rtbuild           PGUID 11 f t f 9 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 324 (  rtbeginscan       PGUID 11 f t f 4 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 325 (  rtendscan         PGUID 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 326 (  rtmarkpos         PGUID 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 327 (  rtrestrpos        PGUID 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 328 (  rtrescan          PGUID 11 f t f 3 f 23 "0" 100 0 0 100  foo bar ));

DATA(insert OID = 330 (  btgettuple        PGUID 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 331 (  btinsert          PGUID 11 f t f 3 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 332 (  btdelete          PGUID 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 333 (  btbeginscan       PGUID 11 f t f 4 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 334 (  btrescan          PGUID 11 f t f 3 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 335 (  btendscan         PGUID 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 336 (  btmarkpos         PGUID 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 337 (  btrestrpos        PGUID 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 338 (  btbuild           PGUID 11 f t f 7 f 23 "0" 100 0 0 100  foo bar ));

DATA(insert OID = 339 (  poly_same         PGUID 11 f t f 2 f 16 "0" 100 0 1 0  foo bar ));
DATA(insert OID = 340 (  poly_contain      PGUID 11 f t f 2 f 16 "0" 100 0 1 0  foo bar ));
DATA(insert OID = 341 (  poly_left         PGUID 11 f t f 2 f 16 "0" 100 0 1 0  foo bar ));
DATA(insert OID = 342 (  poly_overleft     PGUID 11 f t f 2 f 16 "0" 100 0 1 0  foo bar ));
DATA(insert OID = 343 (  poly_overright    PGUID 11 f t f 2 f 16 "0" 100 0 1 0  foo bar ));
DATA(insert OID = 344 (  poly_right        PGUID 11 f t f 2 f 16 "0" 100 0 1 0  foo bar ));
DATA(insert OID = 345 (  poly_contained    PGUID 11 f t f 2 f 16 "0" 100 0 1 0  foo bar ));
DATA(insert OID = 346 (  poly_overlap      PGUID 11 f t f 2 f 16 "0" 100 0 1 0  foo bar ));
DATA(insert OID = 347 (  poly_in           PGUID 11 f t f 1 f 604 "0" 100 0 1 0  foo bar ));
DATA(insert OID = 348 (  poly_out          PGUID 11 f t f 1 f 23 "0" 100 0 1 0  foo bar ));

DATA(insert OID = 350 (  btint2cmp         PGUID 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 351 (  btint4cmp         PGUID 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 352 (  btint42cmp        PGUID 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 353 (  btint24cmp        PGUID 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 354 (  btfloat4cmp       PGUID 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 355 (  btfloat8cmp       PGUID 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 356 (  btoidcmp          PGUID 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 357 (  btabstimecmp      PGUID 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 358 (  btcharcmp         PGUID 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 359 (  btchar16cmp       PGUID 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 360 (  bttextcmp         PGUID 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 361 (  lseg_distance     PGUID 11 f t t 2 f 701 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 362 (  lseg_interpt      PGUID 11 f t t 2 f 600 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 363 (  dist_ps           PGUID 11 f t t 2 f 701 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 364 (  dist_pb           PGUID 11 f t t 2 f 701 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 365 (  dist_sb           PGUID 11 f t t 2 f 701 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 366 (  close_ps          PGUID 11 f t t 2 f 600 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 367 (  close_pb          PGUID 11 f t t 2 f 600 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 368 (  close_sb          PGUID 11 f t t 2 f 600 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 369 (  on_ps             PGUID 11 f t t 2 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 370 (  path_distance     PGUID 11 f t t 2 f 701 "602 602" 100 0 1 0 foo bar ));
DATA(insert OID = 371 (  dist_ppth         PGUID 11 f t t 2 f 701 "600 602" 100 0 1 0 foo bar ));
DATA(insert OID = 372 (  on_sb             PGUID 11 f t t 2 f 701 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 373 (  inter_sb          PGUID 11 f t t 2 f 16 "0" 100 0 0 100  foo bar ));

DATA(insert OID = 612 (  fbtreeinsert      PGUID 11 f t f 4 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 613 (  fbtreedelete      PGUID 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 614 (  fbtreegettuple    PGUID 11 f t f 7 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 615 (  fbtreebuild       PGUID 11 f t f 7 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 632 ( Negation           PGUID 11 f t f 1 f 701 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 633 ( Conjunction        PGUID 11 f t f 2 f 701 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 634 ( Disjunction        PGUID 11 f t f 2 f 701 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 635 ( LInference         PGUID 11 f t f 4 f 701 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 636 ( UInference         PGUID 11 f t f 4 f 701 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 637 ( UIntersection      PGUID 11 f t f 4 f 701 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 638 ( LIntersection      PGUID 11 f t f 4 f 701 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 639 ( UDempster          PGUID 11 f t f 4 f 701 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 640 ( LDempster          PGUID 11 f t f 4 f 701 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 649 ( GetAttributeByName PGUID 11 f t f 3 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 650 (  int4notin         PGUID 11 f t f 2 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 651 (  oidnotin          PGUID 11 f t f 2 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 652 (  int44in           PGUID 11 f t f 1 f 22 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 653 (  int44out          PGUID 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 654 (  GetAttributeByNum PGUID 11 f t f 3 f 23 "0" 100 0 0 100  foo bar ));

DATA(insert OID = 655 (  char16lt          PGUID 11 f t f 2 f 16 "19 19" 100 0 0 100  foo bar ));
DATA(insert OID = 656 (  char16le          PGUID 11 f t f 2 f 16 "19 19" 100 0 0 100  foo bar ));
DATA(insert OID = 657 (  char16gt          PGUID 11 f t f 2 f 16 "19 19" 100 0 0 100  foo bar ));
DATA(insert OID = 658 (  char16ge          PGUID 11 f t f 2 f 16 "19 19" 100 0 0 100  foo bar ));
DATA(insert OID = 659 (  char16ne          PGUID 11 f t f 2 f 16 "19 19" 100 0 0 100  foo bar ));

DATA(insert OID = 700 (  lockadd           PGUID 11 f t f 2 f 31 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 701 (  lockrm            PGUID 11 f t f 2 f 31 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 710 (  pg_username       PGUID 11 f t t 0 f 19 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 711 (  userfntest        PGUID 11 f t t 1 f 23 "0" 100 0 0 100  foo bar ));

DATA(insert OID = 720 (  byteasize	   PGUID 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 721 (  byteagetbyte	   PGUID 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 722 (  byteasetbyte	   PGUID 11 f t f 3 f 17 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 723 (  byteagetbit	   PGUID 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 724 (  byteasetbit	   PGUID 11 f t f 3 f 17 "0" 100 0 0 100  foo bar ));

DATA(insert OID = 730 (  pqtest            PGUID 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));

DATA(insert OID = 740 (  text_lt           PGUID 11 f t f 2 f 16 "25 25" 100 0 0 0  foo bar ));
DATA(insert OID = 741 (  text_le           PGUID 11 f t f 2 f 16 "25 25" 100 0 0 0  foo bar ));
DATA(insert OID = 742 (  text_gt           PGUID 11 f t f 2 f 16 "25 25" 100 0 0 0  foo bar ));
DATA(insert OID = 743 (  text_ge           PGUID 11 f t f 2 f 16 "25 25" 100 0 0 0  foo bar ));

DATA(insert OID = 750 (  array_in          PGUID 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 751 (  array_out         PGUID 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 752 (  filename_in       PGUID 11 f t f 2 f 605 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 753 (  filename_out      PGUID 11 f t f 2 f 19 "0" 100 0 0 100  foo bar ));

DATA(insert OID = 760 (  smgrin		   PGUID 11 f t f 1 f 210 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 761 (  smgrout	   PGUID 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 762 (  smgreq		   PGUID 11 f t f 2 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 763 (  smgrne		   PGUID 11 f t f 2 f 16 "0" 100 0 0 100  foo bar ));

DATA(insert OID = 764 (  lo_filein         PGUID 11 f t f 1 f 605 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 765 (  lo_fileout        PGUID 11 f t f 1 f 19 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 766 (  int4inc           PGUID 11 f t f 1 f 23 "23" 100 0 0 100  foo bar ));
DATA(insert OID = 767 (  int2inc           PGUID 11 f t f 1 f 21 "21" 100 0 0 100  foo bar ));
DATA(insert OID = 768 (  int4larger        PGUID 11 f t f 2 f 23 "23 23" 100 0 0 100  foo bar ));
DATA(insert OID = 769 (  int4smaller       PGUID 11 f t f 2 f 23 "23 23" 100 0 0 100  foo bar ));
DATA(insert OID = 770 (  int2larger        PGUID 11 f t f 2 f 23 "21 21" 100 0 0 100  foo bar ));
DATA(insert OID = 771 (  int2smaller       PGUID 11 f t f 2 f 23 "21 21" 100 0 0 100  foo bar ));

BKI_BEGIN
#ifdef NOBTREE
BKI_END
DATA(insert OID = 800 (  nobtgettuple      PGUID 11 f t f 6 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 801 (  nobtinsert        PGUID 11 f t f 3 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 802 (  nobtdelete        PGUID 11 f t f 2 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 803 (  nobtbeginscan     PGUID 11 f t f 4 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 804 (  nobtrescan        PGUID 11 f t f 3 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 805 (  nobtendscan       PGUID 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 806 (  nobtmarkpos       PGUID 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 807 (  nobtrestrpos      PGUID 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 808 (  nobtbuild         PGUID 11 f t f 7 f 23 "0" 100 0 0 100  foo bar ));
BKI_BEGIN
#endif /* NOBTREE */
BKI_END

DATA(insert OID = 900 (  fimport           PGUID 11 f t f 1 f 26 "25" 100 0 0 100  foo bar ));
DATA(insert OID = 901 (  fexport           PGUID 11 f t f 2 f 23 "25 26" 100 0 0 100  foo bar ));
DATA(insert OID = 902 (  fabstract         PGUID 11 f t f 5 f 23 "25 26 23 23 23" 100 0 0 100  foo bar ));

DATA(insert OID = 920 (  oidint4in	   PGUID 11 f t f 1 f 910 "23" 100 0 0 100  foo bar));
DATA(insert OID = 921 (  oidint4out	   PGUID 11 f t f 1 f 19 "910" 100 0 0 100  foo bar));
DATA(insert OID = 922 (  oidint4lt	   PGUID 11 f t f 2 f 16 "0" 100 0 0 100  foo bar));
DATA(insert OID = 923 (  oidint4le	   PGUID 11 f t f 2 f 16 "0" 100 0 0 100  foo bar));
DATA(insert OID = 924 (  oidint4eq	   PGUID 11 f t f 2 f 16 "0" 100 0 0 100  foo bar));

#define OidInt4EqRegProcedure 924

DATA(insert OID = 925 (  oidint4ge	   PGUID 11 f t f 2 f 16 "0" 100 0 0 100  foo bar));
DATA(insert OID = 926 (  oidint4gt	   PGUID 11 f t f 2 f 16 "0" 100 0 0 100  foo bar));
DATA(insert OID = 927 (  oidint4ne	   PGUID 11 f t f 2 f 16 "0" 100 0 0 100  foo bar));
DATA(insert OID = 928 (  oidint4cmp	   PGUID 11 f t f 2 f 23 "0" 100 0 0 100  foo bar));
DATA(insert OID = 929 (  mkoidint4	   PGUID 11 f t f 2 f 910 "26 23" 100 0 0 100  foo bar));

DATA(insert OID = 940 (  oidchar16in	   PGUID 11 f t f 1 f 911 "23" 100 0 0 100  foo bar));
DATA(insert OID = 941 (  oidchar16out	   PGUID 11 f t f 1 f 19 "910" 100 0 0 100  foo bar));
DATA(insert OID = 942 (  oidchar16lt	   PGUID 11 f t f 2 f 16 "0" 100 0 0 100  foo bar));
DATA(insert OID = 943 (  oidchar16le	   PGUID 11 f t f 2 f 16 "0" 100 0 0 100  foo bar));
DATA(insert OID = 944 (  oidchar16eq	   PGUID 11 f t f 2 f 16 "0" 100 0 0 100  foo bar));

#define OidChar16EqRegProcedure 944

DATA(insert OID = 945 (  oidchar16ge	   PGUID 11 f t f 2 f 16 "0" 100 0 0 100  foo bar));
DATA(insert OID = 946 (  oidchar16gt	   PGUID 11 f t f 2 f 16 "0" 100 0 0 100  foo bar));
DATA(insert OID = 947 (  oidchar16ne	   PGUID 11 f t f 2 f 16 "0" 100 0 0 100  foo bar));
DATA(insert OID = 948 (  oidchar16cmp	   PGUID 11 f t f 2 f 23 "0" 100 0 0 100  foo bar));
DATA(insert OID = 949 (  mkoidchar16	   PGUID 11 f t f 2 f 911 "26 23" 100 0 0 100  foo bar));

DATA(insert OID = 950 (  filetooid         PGUID 11 f t f 1 f 26 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 951 (  locreatoid        PGUID 11 f t f 1 f 26 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 952 (  loopen            PGUID 11 f t f 2 f 23 "25 23" 100 0 0 100  foo bar ));
DATA(insert OID = 953 (  loclose           PGUID 11 f t f 1 f 23 "23" 100 0 0 100  foo bar ));
DATA(insert OID = 954 (  loread            PGUID 11 f t f 2 f 17 "0 0" 100 0 0 100  foo bar ));
DATA(insert OID = 955 (  lowrite           PGUID 11 f t f 2 f 23 "0 0" 100 0 0 100  foo bar ));
DATA(insert OID = 956 (  lolseek           PGUID 11 f t f 3 f 23 "0 0 0" 100 0 0 100  foo bar ));
DATA(insert OID = 957 (  locreat           PGUID 11 f t f 3 f 23 "0 0 0" 100 0 0 100  foo bar ));
DATA(insert OID = 958 (  lotell            PGUID 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 959 (  loftruncate       PGUID 11 f t f 2 f 23 "0 0" 100 0 0 100  foo bar ));
DATA(insert OID = 960 (  lostat            PGUID 11 f t f 1 f 17 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 961 (  lorename          PGUID 11 f t f 2 f 23 "0 0" 100 0 0 100  foo bar ));
DATA(insert OID = 962 (  lomkdir           PGUID 11 f t f 2 f 23 "0 0" 100 0 0 100  foo bar ));
DATA(insert OID = 963 (  lormdir           PGUID 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 964 (  lounlink          PGUID 11 f t f 1 f 23 "0" 100 0 0 100  foo bar ));

DATA(insert OID = 970 (  pftp_read         PGUID 11 f t f 3 f 23 "25 23 26" 100 0 1 0  foo bar ));
DATA(insert OID = 971 (  pftp_write        PGUID 11 f t f 2 f 26 "25 23" 100 0 1 0  foo bar ));

DATA(insert OID = 972 (  RegprocToOid      PGUID 11 f t t 1 f 26 "24" 100 0 0 100  foo bar ));

DATA(insert OID = 973 (  path_inter        PGUID 11 f t t 2 f 16 "0" 100 0 10 100  foo bar ));
DATA(insert OID = 974 (  box_copy          PGUID 11 f t t 1 f 603 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 975 (  box_area          PGUID 11 f t t 1 f 701 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 976 (  box_length        PGUID 11 f t t 1 f 701 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 977 (  box_height        PGUID 11 f t t 1 f 701 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 978 (  box_distance      PGUID 11 f t t 2 f 701 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 980 (  box_intersect     PGUID 11 f t t 2 f 603 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 981 (  box_diagonal      PGUID 11 f t t 1 f 601 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 982 (  path_n_lt         PGUID 11 f t t 2 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 983 (  path_n_gt         PGUID 11 f t t 2 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 984 (  path_n_eq         PGUID 11 f t t 2 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 985 (  path_n_le         PGUID 11 f t t 2 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 986 (  path_n_ge         PGUID 11 f t t 2 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 987 (  path_length       PGUID 11 f t t 1 f 701 "0" 100 0 1 0  foo bar ));
DATA(insert OID = 988 (  point_copy        PGUID 11 f t t 1 f 600 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 989 (  point_vert        PGUID 11 f t t 2 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 990 (  point_horiz       PGUID 11 f t t 2 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 991 (  point_distance    PGUID 11 f t t 2 f 701 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 992 (  point_slope       PGUID 11 f t t 2 f 701 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 993 (  lseg_construct    PGUID 11 f t t 2 f 601 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 994 (  lseg_intersect    PGUID 11 f t t 2 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 995 (  lseg_parallel     PGUID 11 f t t 2 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 996 (  lseg_perp         PGUID 11 f t t 2 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 997 (  lseg_vertical     PGUID 11 f t t 1 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 998 (  lseg_horizontal   PGUID 11 f t t 1 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 999 (  lseg_eq           PGUID 11 f t t 2 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 1029 (  NullValue        PGUID 11 f t f 1 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 1030 (  NonNullValue     PGUID 11 f t f 1 f 16 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 1031 (  aclitemin        PGUID 11 f t f 1 f 1033 "0" 100 0 0 100  foo bar ));
DATA(insert OID = 1032 (  aclitemout       PGUID 11 f t f 1 f 23 "1033" 100 0 0 100  foo bar ));
DATA(insert OID = 1035 (  aclinsert        PGUID 11 f t f 2 f 1034 "1034 1033" 100 0 0 100  foo bar ));
DATA(insert OID = 1036 (  aclremove        PGUID 11 f t f 2 f 1034 "1034 1033" 100 0 0 100  foo bar ));
DATA(insert OID = 1037 (  aclcontains      PGUID 11 f t f 2 f 16 "1034 1033" 100 0 0 100  foo bar ));

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
