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
    oid 	prorettype;
    oid8        proargtypes;
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
#define Natts_pg_proc			11
#define Anum_pg_proc_proname		1
#define Anum_pg_proc_proowner		2
#define Anum_pg_proc_prolang		3
#define Anum_pg_proc_proisinh		4
#define Anum_pg_proc_proistrusted	5
#define Anum_pg_proc_proiscachable	6
#define Anum_pg_proc_pronargs		7
#define Anum_pg_proc_prorettype		8
#define Anum_pg_proc_proargtypes        9
#define Anum_pg_proc_prosrc		10
#define Anum_pg_proc_probin		11

/* ----------------
 *	initial contents of pg_proc
 * ----------------
 */

DATA(insert OID =  28 (  boolin            6 11 f t f 1  16 "0" foo bar ));
DATA(insert OID =  29 (  boolout           6 11 f t f 1  23 "0" foo bar ));
DATA(insert OID =  30 (  byteain           6 11 f t f 1  17 "0" foo bar ));
DATA(insert OID =  31 (  byteaout          6 11 f t f 1  23 "0" foo bar ));
DATA(insert OID =  32 (  charin            6 11 f t f 1  18 "0" foo bar ));
DATA(insert OID =  33 (  charout           6 11 f t f 1  23 "0" foo bar ));
DATA(insert OID =  34 (  char16in          6 11 f t f 1  19 "0" foo bar ));
DATA(insert OID =  35 (  char16out         6 11 f t f 1  23 "0" foo bar ));
DATA(insert OID =  36 (  dtin              6 11 f t f 1  20 "0" foo bar ));
DATA(insert OID =  37 (  dtout             6 11 f t f 1  23 "0" foo bar ));
DATA(insert OID =  38 (  int2in            6 11 f t f 1  21 "0" foo bar ));
DATA(insert OID =  39 (  int2out           6 11 f t f 1  23 "0" foo bar ));
DATA(insert OID =  40 (  int28in           6 11 f t f 1  22 "0" foo bar ));
DATA(insert OID =  41 (  int28out          6 11 f t f 1  23 "0" foo bar ));
DATA(insert OID =  42 (  int4in            6 11 f t f 1  23 "0" foo bar ));
DATA(insert OID =  43 (  int4out           6 11 f t f 1  23 "0" foo bar ));
DATA(insert OID =  44 (  regprocin         6 11 f t f 1  24 "0" foo bar ));
DATA(insert OID =  45 (  regprocout        6 11 f t f 1  23 "0" foo bar ));
DATA(insert OID =  46 (  textin            6 11 f t f 1  25 "0" foo bar ));
#define TextInRegProcedure 46
    
DATA(insert OID =  47 (  textout           6 11 f t f 1  23 "0" foo bar ));
DATA(insert OID =  48 (  tidin             6 11 f t f 1  27 "0" foo bar ));
DATA(insert OID =  49 (  tidout            6 11 f t f 1  23 "0" foo bar ));
DATA(insert OID =  50 (  xidin             6 11 f t f 1  28 "0" foo bar ));
DATA(insert OID =  51 (  xidout            6 11 f t f 1  23 "0" foo bar ));
DATA(insert OID =  52 (  cidin             6 11 f t f 1  29 "0" foo bar ));
DATA(insert OID =  53 (  cidout            6 11 f t f 1  23 "0" foo bar ));
DATA(insert OID =  54 (  oid8in            6 11 f t f 1  30 "0" foo bar ));
DATA(insert OID =  55 (  oid8out           6 11 f t f 1  23 "0" foo bar ));
DATA(insert OID =  56 (  lockin            6 11 f t f 1  31 "0" foo bar ));
DATA(insert OID =  57 (  lockout           6 11 f t f 1  23 "0" foo bar ));
DATA(insert OID =  58 (  stubin            6 11 f t f 1  33 "0" foo bar ));
DATA(insert OID =  59 (  stubout           6 11 f t f 1  23 "0" foo bar ));
DATA(insert OID =  60 (  booleq            6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID =  61 (  chareq            6 11 f t f 2  16 "0" foo bar ));
#define       CharacterEqualRegProcedure      61

DATA(insert OID =  62 (  char16eq          6 11 f t f 2  16 "0" foo bar ));
#define NameEqualRegProcedure		62
#define Character16EqualRegProcedure	62
    
DATA(insert OID =  63 (  int2eq            6 11 f t f 2  16 "0" foo bar ));
#define Integer16EqualRegProcedure	63
    
DATA(insert OID =  64 (  int2lt            6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID =  65 (  int4eq            6 11 f t f 2  16 "0" foo bar ));
#define Integer32EqualRegProcedure	65
    
DATA(insert OID =  66 (  int4lt            6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID =  67 (  texteq            6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID =  68 (  xideq             6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID =  69 (  cideq             6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID =  70 (  charne            6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID =  71 (  charlt            6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID =  72 (  charle            6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID =  73 (  chargt            6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID =  74 (  charge            6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID =  75 (  charpl            6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID =  76 (  charmi            6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID =  77 (  charmul           6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID =  78 (  chardiv           6 11 f t f 2  16 "0" foo bar ));

DATA(insert OID =  79 (  char16regexeq     6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID =  80 (  char16regexne     6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID =  81 (  textregexeq       6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID =  82 (  textregexne       6 11 f t f 2  16 "0" foo bar ));
    
DATA(insert OID =  97 (  rtsel             6 11 f t f 7 109 "0" foo bar ));
DATA(insert OID =  98 (  rtnpage           6 11 f t f 7 109 "0" foo bar ));
DATA(insert OID =  99 (  btreesel          6 11 f t f 7 109 "0" foo bar ));
DATA(insert OID = 100 (  btreenpage        6 11 f t f 7 109 "0" foo bar ));
DATA(insert OID = 101 (  eqsel             6 11 f t f 5 109 "0" foo bar ));
DATA(insert OID = 102 (  neqsel            6 11 f t f 5 109 "0" foo bar ));
DATA(insert OID = 103 (  intltsel          6 11 f t f 5 109 "0" foo bar ));
DATA(insert OID = 104 (  intgtsel          6 11 f t f 5 109 "0" foo bar ));
DATA(insert OID = 105 (  eqjoinsel         6 11 f t f 5 109 "0" foo bar ));
DATA(insert OID = 106 (  neqjoinsel        6 11 f t f 5 109 "0" foo bar ));
DATA(insert OID = 107 (  intltjoinsel      6 11 f t f 5 109 "0" foo bar ));
DATA(insert OID = 108 (  intgtjoinsel      6 11 f t f 5 109 "0" foo bar ));
DATA(insert OID = 109 (  btreeendscan      6 11 f t f 1  23 "0" foo bar ));
DATA(insert OID = 110 (  btreemarkpos      6 11 f t f 1  23 "0" foo bar ));
DATA(insert OID = 111 (  btreerestrpos     6 11 f t f 1  23 "0" foo bar ));
DATA(insert OID = 112 (  btreeinsert       6 11 f t f 3  23 "0" foo bar ));
DATA(insert OID = 113 (  btreedelete       6 11 f t f 2  23 "0" foo bar ));
DATA(insert OID = 114 (  btreegettuple     6 11 f t f 6  23 "0" foo bar ));
DATA(insert OID = 115 (  btreebuild        6 11 f t f 7  23 "0" foo bar ));
DATA(insert OID = 116 (  btreerescan       6 11 f t f 3  23 "0" foo bar ));
DATA(insert OID = 117 (  point_in          6 11 f t f 1 600 "0" foo bar ));
DATA(insert OID = 118 (  point_out         6 11 f t f 1  23 "0" foo bar ));
DATA(insert OID = 119 (  lseg_in           6 11 f t f 1 601 "0" foo bar ));
DATA(insert OID = 120 (  lseg_out          6 11 f t f 1  23 "0" foo bar ));
DATA(insert OID = 121 (  path_in           6 11 f t f 1 602 "0" foo bar ));
DATA(insert OID = 122 (  path_out          6 11 f t f 1  23 "0" foo bar ));
DATA(insert OID = 123 (  box_in            6 11 f t f 1 603 "0" foo bar ));
DATA(insert OID = 124 (  box_out           6 11 f t f 1  23 "0" foo bar ));
DATA(insert OID = 125 (  box_overlap       6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID = 126 (  box_ge            6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID = 127 (  box_gt            6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID = 128 (  box_eq            6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID = 129 (  box_lt            6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID = 130 (  box_le            6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID = 131 (  point_above       6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID = 132 (  point_left        6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID = 133 (  point_right       6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID = 134 (  point_below       6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID = 135 (  point_eq          6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID = 136 (  on_pb             6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID = 137 (  on_ppath          6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID = 138 (  box_center        6 11 f t f 1 600 "0" foo bar ));
DATA(insert OID = 139 (  areasel           6 11 f t f 5 109 "0" foo bar ));
DATA(insert OID = 140 (  areajoinsel       6 11 f t f 5 109 "0" foo bar ));
DATA(insert OID = 141 (  int4mul           6 11 f t f 2  23 "0" foo bar ));
DATA(insert OID = 142 (  int4fac           6 11 f t f 2  23 "0" foo bar ));
DATA(insert OID = 143 (  pointdist         6 11 f t f 2  23 "0" foo bar ));
DATA(insert OID = 144 (  int4ne            6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID = 145 (  int2ne            6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID = 146 (  int2gt            6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID = 147 (  int4gt            6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID = 148 (  int2le            6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID = 149 (  int4le            6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID = 150 (  int4ge            6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID = 151 (  int2ge            6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID = 152 (  int2mul           6 11 f t f 2  21 "0" foo bar ));
DATA(insert OID = 153 (  int2div           6 11 f t f 2  21 "0" foo bar ));
DATA(insert OID = 154 (  int4div           6 11 f t f 2  23 "0" foo bar ));
DATA(insert OID = 155 (  int2mod           6 11 f t f 2  21 "0" foo bar ));
DATA(insert OID = 156 (  int4mod           6 11 f t f 2  23 "0" foo bar ));
DATA(insert OID = 157 (  textne            6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID = 158 (  int24eq           6 11 f t f 2  23 "0" foo bar ));
DATA(insert OID = 159 (  int42eq           6 11 f t f 2  23 "0" foo bar ));
DATA(insert OID = 160 (  int24lt           6 11 f t f 2  23 "0" foo bar ));
DATA(insert OID = 161 (  int42lt           6 11 f t f 2  23 "0" foo bar ));
DATA(insert OID = 162 (  int24gt           6 11 f t f 2  23 "0" foo bar ));
DATA(insert OID = 163 (  int42gt           6 11 f t f 2  23 "0" foo bar ));
DATA(insert OID = 164 (  int24ne           6 11 f t f 2  23 "0" foo bar ));
DATA(insert OID = 165 (  int42ne           6 11 f t f 2  23 "0" foo bar ));
DATA(insert OID = 166 (  int24le           6 11 f t f 2  23 "0" foo bar ));
DATA(insert OID = 167 (  int42le           6 11 f t f 2  23 "0" foo bar ));
DATA(insert OID = 168 (  int24ge           6 11 f t f 2  23 "0" foo bar ));
DATA(insert OID = 169 (  int42ge           6 11 f t f 2  23 "0" foo bar ));
DATA(insert OID = 170 (  int24mul          6 11 f t f 2  23 "0" foo bar ));
DATA(insert OID = 171 (  int42mul          6 11 f t f 2  23 "0" foo bar ));
DATA(insert OID = 172 (  int24div          6 11 f t f 2  23 "0" foo bar ));
DATA(insert OID = 173 (  int42div          6 11 f t f 2  23 "0" foo bar ));
DATA(insert OID = 174 (  int24mod          6 11 f t f 2  23 "0" foo bar ));
DATA(insert OID = 175 (  int42mod          6 11 f t f 2  23 "0" foo bar ));
DATA(insert OID = 176 (  int2pl            6 11 f t f 2  21 "0" foo bar ));
DATA(insert OID = 177 (  int4pl            6 11 f t f 2  23 "0" foo bar ));
DATA(insert OID = 178 (  int24pl           6 11 f t f 2  23 "0" foo bar ));
DATA(insert OID = 179 (  int42pl           6 11 f t f 2  23 "0" foo bar ));
DATA(insert OID = 180 (  int2mi            6 11 f t f 2  21 "0" foo bar ));
DATA(insert OID = 181 (  int4mi            6 11 f t f 2  23 "0" foo bar ));
DATA(insert OID = 182 (  int24mi           6 11 f t f 2  23 "0" foo bar ));
DATA(insert OID = 183 (  int42mi           6 11 f t f 2  23 "0" foo bar ));
DATA(insert OID = 184 (  oideq             6 11 f t f 2  16 "0" foo bar ));
#define ObjectIdEqualRegProcedure	184
    
DATA(insert OID = 185 (  oidneq            6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID = 186 (  box_same          6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID = 187 (  box_contain       6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID = 188 (  box_left          6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID = 189 (  box_overleft      6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID = 190 (  box_overright     6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID = 191 (  box_right         6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID = 192 (  box_contained     6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID = 193 (  rt_box_union      6 11 f t f 2 603 "0" foo bar ));
DATA(insert OID = 194 (  rt_box_inter      6 11 f t f 2 603 "0" foo bar ));
DATA(insert OID = 195 (  rt_box_size       6 11 f t f 1  23 "0" foo bar ));
DATA(insert OID = 196 (  rt_bigbox_size    6 11 f t f 1  23 "0" foo bar ));
DATA(insert OID = 197 (  rt_poly_union     6 11 f t f 2 603 "0" foo bar ));
DATA(insert OID = 198 (  rt_poly_inter     6 11 f t f 2 603 "0" foo bar ));
DATA(insert OID = 199 (  rt_poly_size      6 11 f t f 1  23 "0" foo bar ));
DATA(insert OID = 200 (  float4in          6 11 f t f 1 700 "0" foo bar ));
DATA(insert OID = 201 (  float4out         6 11 f t f 1  23 "0" foo bar ));
DATA(insert OID = 202 (  float4mul         6 11 f t f 2 700 "0" foo bar ));
DATA(insert OID = 203 (  float4div         6 11 f t f 2 700 "0" foo bar ));
DATA(insert OID = 204 (  float4pl          6 11 f t f 2 700 "0" foo bar ));
DATA(insert OID = 205 (  float4mi          6 11 f t f 2 700 "0" foo bar ));
DATA(insert OID = 206 (  float4um          6 11 f t f 1 700 "0" foo bar ));
DATA(insert OID = 207 (  float4abs         6 11 f t f 1 700 "0" foo bar ));
DATA(insert OID = 208 (  float4inc         6 11 f t f 1 700 "0" foo bar ));
DATA(insert OID = 209 (  float4larger      6 11 f t f 2 700 "0" foo bar ));
DATA(insert OID = 211 (  float4smaller     6 11 f t f 2 700 "0" foo bar ));
    
DATA(insert OID = 214 (  float8in          6 11 f t f 1 701 "0" foo bar ));
DATA(insert OID = 215 (  float8out         6 11 f t f 1  23 "0" foo bar ));
DATA(insert OID = 216 (  float8mul         6 11 f t f 2 701 "0" foo bar ));
DATA(insert OID = 217 (  float8div         6 11 f t f 2 701 "0" foo bar ));
DATA(insert OID = 218 (  float8pl          6 11 f t f 2 701 "0" foo bar ));
DATA(insert OID = 219 (  float8mi          6 11 f t f 2 701 "0" foo bar ));
DATA(insert OID = 220 (  float8um          6 11 f t f 1 701 "0" foo bar ));
DATA(insert OID = 221 (  float8abs         6 11 f t f 1 701 "0" foo bar ));
DATA(insert OID = 222 (  float8inc         6 11 f t f 1 701 "0" foo bar ));
DATA(insert OID = 223 (  float8larger      6 11 f t f 2 701 "0" foo bar ));
DATA(insert OID = 224 (  float8smaller     6 11 f t f 2 701 "0" foo bar ));
DATA(insert OID = 228 (  dround            6 11 f t f 1 701 "0" foo bar ));
DATA(insert OID = 229 (  dtrunc            6 11 f t f 1 701 "0" foo bar ));
DATA(insert OID = 230 (  dsqrt             6 11 f t f 1 701 "0" foo bar ));
DATA(insert OID = 231 (  dcbrt             6 11 f t f 1 701 "0" foo bar ));
DATA(insert OID = 232 (  dpow              6 11 f t f 2 701 "0" foo bar ));
DATA(insert OID = 233 (  dexp              6 11 f t f 1 701 "0" foo bar ));
DATA(insert OID = 234 (  dlog1             6 11 f t f 1 701 "0" foo bar ));
    
DATA(insert OID = 240 (  abstimein         6 11 f t f 1 702 "0" foo bar ));
DATA(insert OID = 241 (  abstimeout        6 11 f t f 1  23 "0" foo bar ));
DATA(insert OID = 242 (  reltimein         6 11 f t f 1 703 "0" foo bar ));
DATA(insert OID = 243 (  reltimeout        6 11 f t f 1  23 "0" foo bar ));
DATA(insert OID = 244 (  timepl            6 11 f t f 2 702 "0" foo bar ));
DATA(insert OID = 245 (  timemi            6 11 f t f 2 702 "0" foo bar ));
DATA(insert OID = 246 (  tintervalin       6 11 f t f 1 704 "0" foo bar ));
DATA(insert OID = 247 (  tintervalout      6 11 f t f 1  23 "0" foo bar ));
DATA(insert OID = 248 (  ininterval        6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID = 249 (  intervalrel       6 11 f t f 1 703 "0" foo bar ));
DATA(insert OID = 250 (  timenow           6 11 f t f 0 702 "0" foo bar ));
DATA(insert OID = 251 (  abstimeeq         6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID = 252 (  abstimene         6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID = 253 (  abstimelt         6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID = 254 (  abstimegt         6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID = 255 (  abstimele         6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID = 256 (  abstimege         6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID = 257 (  reltimeeq         6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID = 258 (  reltimene         6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID = 259 (  reltimelt         6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID = 260 (  reltimegt         6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID = 261 (  reltimele         6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID = 262 (  reltimege         6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID = 263 (  intervaleq        6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID = 264 (  intervalct        6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID = 265 (  intervalov        6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID = 266 (  intervalleneq     6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID = 267 (  intervallenne     6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID = 268 (  intervallenlt     6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID = 269 (  intervallengt     6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID = 270 (  intervallenle     6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID = 271 (  intervallenge     6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID = 272 (  intervalstart     6 11 f t f 1 702 "0" foo bar ));
DATA(insert OID = 273 (  intervalend       6 11 f t f 1 702 "0" foo bar ));
    
DATA(insert OID = 287 (  float4eq          6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID = 288 (  float4ne          6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID = 289 (  float4lt          6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID = 290 (  float4le          6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID = 291 (  float4gt          6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID = 292 (  float4ge          6 11 f t f 2  16 "0" foo bar ));
    
DATA(insert OID = 293 (  float8eq          6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID = 294 (  float8ne          6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID = 295 (  float8lt          6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID = 296 (  float8le          6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID = 297 (  float8gt          6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID = 298 (  float8ge          6 11 f t f 2  16 "0" foo bar ));

DATA(insert OID = 319 (  btreebeginscan    6 11 f t f 3  23 "0" foo bar ));
DATA(insert OID = 320 (  rtinsert          6 11 f t f 3  23 "0" foo bar ));
DATA(insert OID = 321 (  rtdelete          6 11 f t f 2  23 "0" foo bar ));
DATA(insert OID = 322 (  rtgettuple        6 11 f t f 6  23 "0" foo bar ));
DATA(insert OID = 323 (  rtbuild           6 11 f t f 7  23 "0" foo bar ));
DATA(insert OID = 324 (  rtbeginscan       6 11 f t f 4  23 "0" foo bar ));
DATA(insert OID = 325 (  rtendscan         6 11 f t f 1  23 "0" foo bar ));
DATA(insert OID = 326 (  rtmarkpos         6 11 f t f 1  23 "0" foo bar ));
DATA(insert OID = 327 (  rtrestrpos        6 11 f t f 1  23 "0" foo bar ));
DATA(insert OID = 328 (  rtrescan          6 11 f t f 3  23 "0" foo bar ));

DATA(insert OID = 330 (  btgettuple        6 11 f t f 6  23 "0" foo bar ));
DATA(insert OID = 331 (  btinsert          6 11 f t f 3  23 "0" foo bar ));
DATA(insert OID = 332 (  btdelete          6 11 f t f 2  23 "0" foo bar ));
DATA(insert OID = 333 (  btbeginscan       6 11 f t f 4  23 "0" foo bar ));
DATA(insert OID = 334 (  btrescan          6 11 f t f 3  23 "0" foo bar ));
DATA(insert OID = 335 (  btendscan         6 11 f t f 1  23 "0" foo bar ));
DATA(insert OID = 336 (  btmarkpos         6 11 f t f 1  23 "0" foo bar ));
DATA(insert OID = 337 (  btrestrpos        6 11 f t f 1  23 "0" foo bar ));
DATA(insert OID = 338 (  btbuild           6 11 f t f 7  23 "0" foo bar ));

DATA(insert OID = 339 (  poly_same         6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID = 340 (  poly_contain      6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID = 341 (  poly_left         6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID = 342 (  poly_overleft     6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID = 343 (  poly_overright    6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID = 344 (  poly_right        6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID = 345 (  poly_contained    6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID = 346 (  poly_overlap      6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID = 347 (  poly_in           6 11 f t f 1 604 "0" foo bar ));
DATA(insert OID = 348 (  poly_out          6 11 f t f 1  23 "0" foo bar ));

DATA(insert OID = 350 (  btint2cmp         6 11 f t f 2  23 "0" foo bar ));
DATA(insert OID = 351 (  btint4cmp         6 11 f t f 2  23 "0" foo bar ));
DATA(insert OID = 352 (  btint42cmp        6 11 f t f 2  23 "0" foo bar ));
DATA(insert OID = 353 (  btint24cmp        6 11 f t f 2  23 "0" foo bar ));
DATA(insert OID = 354 (  btfloat4cmp       6 11 f t f 2  23 "0" foo bar ));
DATA(insert OID = 355 (  btfloat8cmp       6 11 f t f 2  23 "0" foo bar ));
DATA(insert OID = 356 (  btoidcmp          6 11 f t f 2  23 "0" foo bar ));
DATA(insert OID = 357 (  btabstimecmp      6 11 f t f 2  23 "0" foo bar ));
DATA(insert OID = 358 (  btcharcmp         6 11 f t f 2  23 "0" foo bar ));
DATA(insert OID = 359 (  btchar16cmp       6 11 f t f 2  23 "0" foo bar ));
DATA(insert OID = 360 (  bttextcmp         6 11 f t f 2  23 "0" foo bar ));

DATA(insert OID = 612 (  fbtreeinsert      6 11 f t f 4  23 "0" foo bar ));
DATA(insert OID = 613 (  fbtreedelete      6 11 f t f 2  23 "0" foo bar ));
DATA(insert OID = 614 (  fbtreegettuple    6 11 f t f 7  23 "0" foo bar ));
DATA(insert OID = 615 (  fbtreebuild       6 11 f t f 7  23 "0" foo bar ));
DATA(insert OID = 632 ( Negation           6 11 f t f 1 701 "0" foo bar ));
DATA(insert OID = 633 ( Conjunction        6 11 f t f 2 701 "0" foo bar ));
DATA(insert OID = 634 ( Disjunction        6 11 f t f 2 701 "0" foo bar ));
DATA(insert OID = 635 ( LInference         6 11 f t f 4 701 "0" foo bar ));
DATA(insert OID = 636 ( UInference         6 11 f t f 4 701 "0" foo bar ));
DATA(insert OID = 637 ( UIntersection      6 11 f t f 4 701 "0" foo bar ));
DATA(insert OID = 638 ( LIntersection      6 11 f t f 4 701 "0" foo bar ));
DATA(insert OID = 639 ( UDempster          6 11 f t f 4 701 "0" foo bar ));
DATA(insert OID = 640 ( LDempster          6 11 f t f 4 701 "0" foo bar ));
DATA(insert OID = 650 (  int4notin         6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID = 651 (  oidnotin          6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID = 652 (  int44in           6 11 f t f 1  22 "0" foo bar ));
DATA(insert OID = 653 (  int44out          6 11 f t f 1  23 "0" foo bar ));
DATA(insert OID = 654 (  GetAttribute      6 11 f t f 1  23 "0" foo bar ));

DATA(insert OID = 655 (  char16lt          6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID = 656 (  char16le          6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID = 657 (  char16gt          6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID = 658 (  char16ge          6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID = 659 (  char16ne          6 11 f t f 2  16 "0" foo bar ));

DATA(insert OID = 700 (  lockadd           6 11 f t f 2  31 "0" foo bar ));
DATA(insert OID = 701 (  lockrm            6 11 f t f 2  31 "0" foo bar ));
DATA(insert OID = 710 (  pg_username       6 11 f t t 0  19 "0" foo bar ));
DATA(insert OID = 711 (  userfntest        6 11 f t t 1  23 "0" foo bar ));

DATA(insert OID = 720 (  byteasize	   6 11 f t f 1  23 "0" foo bar ));
DATA(insert OID = 721 (  byteagetbyte	   6 11 f t f 2  23 "0" foo bar ));
DATA(insert OID = 722 (  byteasetbyte	   6 11 f t f 3  17 "0" foo bar ));
DATA(insert OID = 723 (  byteagetbit	   6 11 f t f 2  23 "0" foo bar ));
DATA(insert OID = 724 (  byteasetbit	   6 11 f t f 3  17 "0" foo bar ));
    
DATA(insert OID = 730 (  pqtest            6 11 f t f 1  23 "0" foo bar ));

DATA(insert OID = 740 (  text_lt           6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID = 741 (  text_le           6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID = 742 (  text_gt           6 11 f t f 2  16 "0" foo bar ));
DATA(insert OID = 743 (  text_ge           6 11 f t f 2  16 "0" foo bar ));

DATA(insert OID = 750 (  array_in          6 11 f t f 2  23 "0" foo bar ));
DATA(insert OID = 751 (  array_out         6 11 f t f 2  23 "0" foo bar ));
DATA(insert OID = 752 (  filename_in       6 11 f t f 2  605 "0" foo bar ));
DATA(insert OID = 753 (  filename_out      6 11 f t f 2  19 "0" foo bar ));

DATA(insert OID = 760 (  smgrin		   6 11 f t f 1  210 "0" foo bar ));
DATA(insert OID = 761 (  smgrout	   6 11 f t f 1   23 "0" foo bar ));
DATA(insert OID = 762 (  smgreq		   6 11 f t f 2   16 "0" foo bar ));
DATA(insert OID = 763 (  smgrne		   6 11 f t f 2   16 "0" foo bar ));

DATA(insert OID = 764 (  lo_filein         6 11 f t f 1  605 "0" foo bar ));
DATA(insert OID = 765 (  lo_fileout        6 11 f t f 1  19 "0" foo bar ));
DATA(insert OID = 766 (  int4inc             6 11 f t f 1  23 "0" foo bar ));
DATA(insert OID = 767 (  int2inc             6 11 f t f 1  23 "0" foo bar ));
DATA(insert OID = 768 (  int4larger          6 11 f t f 2  23 "0" foo bar ));
DATA(insert OID = 769 (  int4smaller         6 11 f t f 2  23 "0" foo bar ));
DATA(insert OID = 770 (  int2larger          6 11 f t f 2  23 "0" foo bar ));
DATA(insert OID = 771 (  int2smaller         6 11 f t f 2  23 "0" foo bar ));

BKI_BEGIN
#ifdef NOBTREE
BKI_END
DATA(insert OID = 800 (  nobtgettuple        6 11 f t f 6  23 "0" foo bar ));
DATA(insert OID = 801 (  nobtinsert          6 11 f t f 3  23 "0" foo bar ));
DATA(insert OID = 802 (  nobtdelete          6 11 f t f 2  23 "0" foo bar ));
DATA(insert OID = 803 (  nobtbeginscan       6 11 f t f 4  23 "0" foo bar ));
DATA(insert OID = 804 (  nobtrescan          6 11 f t f 3  23 "0" foo bar ));
DATA(insert OID = 805 (  nobtendscan         6 11 f t f 1  23 "0" foo bar ));
DATA(insert OID = 806 (  nobtmarkpos         6 11 f t f 1  23 "0" foo bar ));
DATA(insert OID = 807 (  nobtrestrpos        6 11 f t f 1  23 "0" foo bar ));
DATA(insert OID = 808 (  nobtbuild           6 11 f t f 7  23 "0" foo bar ));
BKI_BEGIN
#endif /* NOBTREE */
BKI_END

DATA(insert OID = 900 (  fimport             6 11 f t f 1  26 "25" foo bar ));
DATA(insert OID = 901 (  fexport             6 11 f t f 2  23 "25 26" foo bar ));
DATA(insert OID = 902 (  fabstract           6 11 f t f 5  23 "25 26 23 23 23" foo bar ));

DATA(insert OID = 920 (  oidseqin	     6 11 f t f 1 910 "23" foo bar));
DATA(insert OID = 921 (  oidseqout	     6 11 f t f 1  19 "910" foo bar));
DATA(insert OID = 922 (  oidseqlt	     6 11 f t f 2  16 "0" foo bar));
DATA(insert OID = 923 (  oidseqle	     6 11 f t f 2  16 "0" foo bar));
DATA(insert OID = 924 (  oidseqeq	     6 11 f t f 2  16 "0" foo bar));
DATA(insert OID = 925 (  oidseqge	     6 11 f t f 2  16 "0" foo bar));
DATA(insert OID = 926 (  oidseqgt	     6 11 f t f 2  16 "0" foo bar));
DATA(insert OID = 927 (  oidseqne	     6 11 f t f 2  16 "0" foo bar));
DATA(insert OID = 928 (  oidseqcmp	     6 11 f t f 2  23 "0" foo bar));
DATA(insert OID = 929 (  mkoidseq	     6 11 f t f 2 910 "26 23" foo bar));

DATA(insert OID = 950 (  filetooid           6 11 f t f 1  26 "0" foo bar ));
DATA(insert OID = 951 (  locreatoid          6 11 f t f 1  26 "0" foo bar ));
DATA(insert OID = 952 (  loopen              6 11 f t f 2  23 "25 23" foo bar ));
DATA(insert OID = 953 (  loclose             6 11 f t f 1  23 "23" foo bar ));
DATA(insert OID = 954 (  loread              6 11 f t f 2  17 "0 0" foo bar ));
DATA(insert OID = 955 (  lowrite             6 11 f t f 2  23 "0 0" foo bar ));
DATA(insert OID = 956 (  lolseek             6 11 f t f 3  23 "0 0 0" foo bar ));
DATA(insert OID = 957 (  locreat             6 11 f t f 2  23 "0 0" foo bar ));
DATA(insert OID = 958 (  lotell              6 11 f t f 1  23 "0" foo bar ));
DATA(insert OID = 959 (  loftruncate         6 11 f t f 2  23 "0 0" foo bar ));
DATA(insert OID = 960 (  lostat              6 11 f t f 1  17 "0" foo bar ));
DATA(insert OID = 961 (  lorename            6 11 f t f 2  23 "0 0" foo bar ));
DATA(insert OID = 962 (  lomkdir             6 11 f t f 2  23 "0 0" foo bar ));
DATA(insert OID = 963 (  lormdir             6 11 f t f 1  23 "0" foo bar ));
DATA(insert OID = 964 (  lounlink            6 11 f t f 1  23 "0" foo bar ));

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
	OID	prorettype;
	oid8    proargtypes;
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
void ProcedureDefine ARGS((Name procedureName , Name returnTypeName , Name languageName , char *prosrc , char *probin , Boolean canCache , List argList ));

#endif PgProcIncluded
