/*
 * builtins.h --
 *	Declarations for operations on built-in types.
 *
 * Notes:
 *	This should normally only be included by fmgr.h.
 *	Under no circumstances should it ever be included before 
 *	including fmgr.h!
 *
 * Identification:
 *	$Header$
 */

#ifndef BuiltinsIncluded
#define BuiltinsIncluded

#include "tmp/postgres.h"

#include "rules/prs2locks.h"	
#include "rules/prs2stub.h"	

#include "storage/itemptr.h"

#include "utils/large_object.h"

#include "utils/geo-decls.h"

/*
 *	Defined in adt/
 */
/* bool.c */
extern int32 boolin ARGS((char *b));
extern char *boolout ARGS((long b));
extern int32 booleq ARGS((int8 arg1, int8 arg2));
extern int32 boolne ARGS((int8 arg1, int8 arg2));

/* char.c */
extern int32 charin ARGS((char *ch));
extern char *charout ARGS((int32 ch));
extern int32 cidin ARGS((char *s));
extern char *cidout ARGS((int32 c));
extern char *char16in ARGS((char *s));
extern char *char16out ARGS((char *s));
extern int32 chareq ARGS((int8 arg1, int8 arg2));
extern int32 charne ARGS((int8 arg1, int8 arg2));
extern int32 charlt ARGS((int8 arg1, int8 arg2));
extern int32 charle ARGS((int8 arg1, int8 arg2));
extern int32 chargt ARGS((int8 arg1, int8 arg2));
extern int32 charge ARGS((int8 arg1, int8 arg2));
extern int8 charpl ARGS((int8 arg1, int8 arg2));
extern int8 charmi ARGS((int8 arg1, int8 arg2));
extern int8 charmul ARGS((int8 arg1, int8 arg2));
extern int8 chardiv ARGS((int8 arg1, int8 arg2));
extern int32 cideq ARGS((int8 arg1, int8 arg2));
extern int32 char16eq ARGS((char *arg1, char *arg2));
extern int32 char16ne ARGS((char *arg1, char *arg2));
extern int32 char16lt ARGS((char *arg1, char *arg2));
extern int32 char16le ARGS((char *arg1, char *arg2));
extern int32 char16gt ARGS((char *arg1, char *arg2));
extern int32 char16ge ARGS((char *arg1, char *arg2));
extern uint16 char2in ARGS((char *s));
extern char *char2out ARGS((uint16 s));
extern int32 char2eq ARGS((uint16 a, uint16 b));
extern int32 char2ne ARGS((uint16 a, uint16 b));
extern int32 char2lt ARGS((uint16 a, uint16 b));
extern int32 char2le ARGS((uint16 a, uint16 b));
extern int32 char2gt ARGS((uint16 a, uint16 b));
extern int32 char2ge ARGS((uint16 a, uint16 b));
extern int32 char2cmp ARGS((uint16 a, uint16 b));
extern uint32 char4in ARGS((char *s));
extern char *char4out ARGS((uint32 s));
extern int32 char4eq ARGS((uint32 a, uint32 b));
extern int32 char4ne ARGS((uint32 a, uint32 b));
extern int32 char4lt ARGS((uint32 a, uint32 b));
extern int32 char4le ARGS((uint32 a, uint32 b));
extern int32 char4gt ARGS((uint32 a, uint32 b));
extern int32 char4ge ARGS((uint32 a, uint32 b));
extern int32 char4cmp ARGS((uint32 a, uint32 b));
extern char *char8in ARGS((char *s));
extern char *char8out ARGS((char *s));
extern int32 char8eq ARGS((char *arg1, char *arg2));
extern int32 char8ne ARGS((char *arg1, char *arg2));
extern int32 char8lt ARGS((char *arg1, char *arg2));
extern int32 char8le ARGS((char *arg1, char *arg2));
extern int32 char8gt ARGS((char *arg1, char *arg2));
extern int32 char8ge ARGS((char *arg1, char *arg2));
extern int32 char8cmp ARGS((char *arg1, char *arg2));

/* int.c */
extern int32 int2in ARGS((char *num));
extern char *int2out ARGS((int16 sh));
extern int16 *int28in ARGS((char *shs));
extern char *int28out ARGS((int16 (*shs)[]));
extern int32 *int44in ARGS((char *input_string));
extern char *int44out ARGS((int32 an_array[]));
extern int32 int4in ARGS((char *num));
extern char *int4out ARGS((int32 l));
extern int32 i2toi4 ARGS((int16 arg1));
extern int16 i4toi2 ARGS((int32 arg1));
extern int32 int4eq ARGS((int32 arg1, int32 arg2));
extern int32 int4ne ARGS((int32 arg1, int32 arg2));
extern int32 int4lt ARGS((int32 arg1, int32 arg2));
extern int32 int4le ARGS((int32 arg1, int32 arg2));
extern int32 int4gt ARGS((int32 arg1, int32 arg2));
extern int32 int4ge ARGS((int32 arg1, int32 arg2));
extern int32 int2eq ARGS((int16 arg1, int16 arg2));
extern int32 int2ne ARGS((int16 arg1, int16 arg2));
extern int32 int2lt ARGS((int16 arg1, int16 arg2));
extern int32 int2le ARGS((int16 arg1, int16 arg2));
extern int32 int2gt ARGS((int16 arg1, int16 arg2));
extern int32 int2ge ARGS((int16 arg1, int16 arg2));
extern int32 int24eq ARGS((int32 arg1, int32 arg2));
extern int32 int24ne ARGS((int32 arg1, int32 arg2));
extern int32 int24lt ARGS((int32 arg1, int32 arg2));
extern int32 int24le ARGS((int32 arg1, int32 arg2));
extern int32 int24gt ARGS((int32 arg1, int32 arg2));
extern int32 int24ge ARGS((int32 arg1, int32 arg2));
extern int32 int42eq ARGS((int32 arg1, int32 arg2));
extern int32 int42ne ARGS((int32 arg1, int32 arg2));
extern int32 int42lt ARGS((int32 arg1, int32 arg2));
extern int32 int42le ARGS((int32 arg1, int32 arg2));
extern int32 int42gt ARGS((int32 arg1, int32 arg2));
extern int32 int42ge ARGS((int32 arg1, int32 arg2));
extern int32 keyfirsteq ARGS((int16 *arg1, int16 arg2));
extern int32 int4um ARGS((int32 arg));
extern int32 int4pl ARGS((int32 arg1, int32 arg2));
extern int32 int4mi ARGS((int32 arg1, int32 arg2));
extern int32 int4mul ARGS((int32 arg1, int32 arg2));
extern int32 int4div ARGS((int32 arg1, int32 arg2));
extern int32 int4inc ARGS((int32 arg));
extern int16 int2um ARGS((int16 arg));
extern int16 int2pl ARGS((int16 arg1, int16 arg2));
extern int16 int2mi ARGS((int16 arg1, int16 arg2));
extern int16 int2mul ARGS((int16 arg1, int16 arg2));
extern int16 int2div ARGS((int16 arg1, int16 arg2));
extern int16 int2inc ARGS((int16 arg));
extern int32 int24pl ARGS((int32 arg1, int32 arg2));
extern int32 int24mi ARGS((int32 arg1, int32 arg2));
extern int32 int24mul ARGS((int32 arg1, int32 arg2));
extern int32 int24div ARGS((int32 arg1, int32 arg2));
extern int32 int42pl ARGS((int32 arg1, int32 arg2));
extern int32 int42mi ARGS((int32 arg1, int32 arg2));
extern int32 int42mul ARGS((int32 arg1, int32 arg2));
extern int32 int42div ARGS((int32 arg1, int32 arg2));
extern int32 int4mod ARGS((int32 arg1, int32 arg2));
extern int32 int2mod ARGS((int16 arg1, int16 arg2));
extern int32 int24mod ARGS((int32 arg1, int32 arg2));
extern int32 int42mod ARGS((int32 arg1, int32 arg2));
extern int32 int4fac ARGS((int32 arg1));
extern int32 int2fac ARGS((int16 arg1));
extern int16 int2larger ARGS((int16 arg1, int16 arg2));
extern int16 int2smaller ARGS((int16 arg1, int16 arg2));
extern int32 int4larger ARGS((int32 arg1, int32 arg2));
extern int32 int4smaller ARGS((int32 arg1, int32 arg2));

/* name.c */
extern int namecpy ARGS((Name n1, Name n2));
extern int namecat ARGS((Name n1, Name n2));
extern int namestrcpy ARGS((Name name, char *str));
extern int namestrcat ARGS((Name name, char *str));
extern int namestrcmp ARGS((Name name, char *str));
extern bool NameIsEqual ARGS((Name name1, Name name2));
extern uint32 NameComputeLength ARGS((Name name));

/* numutils.c */
/* XXX hack.  HP-UX has a ltoa (with different arguments) already. */
#ifdef PORTNAME_hpux
#define ltoa pg_ltoa
#endif PORTNAME_hpux
extern int32 pg_atoi ARGS((char *s, int size, int c));
extern int itoa ARGS((int i, char *a));
extern int ltoa ARGS((int32 l, char *a));
extern int ftoa ARGS((double value, char *ascii, int width, int prec1, int format));
extern int atof1 ARGS((char *str, double *val));

/*
 *  	New btree code.
 *	Defined in nbtree/
 */
extern char		*btgettuple();
extern char		*btinsert();
extern char		*btdelete();
extern char		*btgetnext();
extern char		*btbeginscan();
extern void		btendscan();
extern void		btbuild();
extern void		btmarkpos();
extern void		btrestrpos();
extern void		btrescan();
extern char		*nobtgettuple();
extern char		*nobtinsert();
extern char		*nobtdelete();
extern char		*nobtgetnext();
extern char		*nobtbeginscan();
extern void		nobtendscan();
extern void		nobtbuild();
extern void		nobtmarkpos();
extern void		nobtrestrpos();
extern void		nobtrescan();

/*
 *	Per-opclass comparison functions for new btrees.  These are
 *	stored in pg_amproc and defined in nbtree/
 */
extern int32		btint2cmp();
extern int32		btint4cmp();
extern int32		btint24cmp();
extern int32		btint42cmp();
extern int32		btfloat4cmp();
extern int32		btfloat8cmp();
extern int32		btoidcmp();
extern int32		btabstimecmp();
extern int32		btcharcmp();
extern int32		btchar16cmp();
extern int32		bttextcmp();

/*
 *	RTree code.
 *	Defined in access/index-rtree/
 */
extern char		*rtinsert();
extern char		*rtdelete();
extern char		*rtgettuple();
extern char		*rtbeginscan();
extern void		rtendscan();
extern void		rtreebuild();
extern void		rtmarkpos();
extern void		rtrestrpos();
extern void		rtrescan();
extern void		rtbuild();

/* support routines for the rtree access method, by opclass */
extern BOX		*rt_box_union();
extern BOX		*rt_box_inter();
extern float		*rt_box_size();
extern float		*rt_bigbox_size();
extern float		*rt_poly_size();
extern POLYGON	*rt_poly_union();
extern POLYGON	*rt_poly_inter();

/* projection utilities */
extern char *GetAttributeByName();
extern char *GetAttributeByNum();


extern int32 pqtest();

/* arrayfuncs.c */
char *array_in ARGS((char *string , ObjectId element_type ));
char *array_out ARGS((char *items , ObjectId element_type ));

/* date.c */
extern int32 reltimein ARGS((char *timestring));
extern char *reltimeout ARGS((int32 timevalue));
extern TimeInterval tintervalin ARGS((char *intervalstr));
extern char *tintervalout ARGS((TimeInterval interval));
extern TimeInterval mktinterval ARGS((AbsoluteTime t1, AbsoluteTime t2));
extern AbsoluteTime timepl ARGS((AbsoluteTime t1, RelativeTime t2));
extern AbsoluteTime timemi ARGS((AbsoluteTime t1, RelativeTime t2));
extern RelativeTime abstimemi ARGS((AbsoluteTime t1, AbsoluteTime t2));
extern int ininterval ARGS((AbsoluteTime t, TimeInterval interval));
extern RelativeTime intervalrel ARGS((TimeInterval interval));
extern AbsoluteTime timenow ARGS((void));
extern int32 reltimeeq ARGS((RelativeTime t1, RelativeTime t2));
extern int32 reltimene ARGS((RelativeTime t1, RelativeTime t2));
extern int32 reltimelt ARGS((RelativeTime t1, RelativeTime t2));
extern int32 reltimegt ARGS((RelativeTime t1, RelativeTime t2));
extern int32 reltimele ARGS((RelativeTime t1, RelativeTime t2));
extern int32 reltimege ARGS((RelativeTime t1, RelativeTime t2));
extern int32 intervaleq ARGS((TimeInterval i1, TimeInterval i2));
extern int32 intervalleneq ARGS((TimeInterval i, RelativeTime t));
extern int32 intervallenne ARGS((TimeInterval i, RelativeTime t));
extern int32 intervallenlt ARGS((TimeInterval i, RelativeTime t));
extern int32 intervallengt ARGS((TimeInterval i, RelativeTime t));
extern int32 intervallenle ARGS((TimeInterval i, RelativeTime t));
extern int32 intervallenge ARGS((TimeInterval i, RelativeTime t));
extern int32 intervalct ARGS((TimeInterval i1, TimeInterval i2));
extern int32 intervalov ARGS((TimeInterval i1, TimeInterval i2));
extern AbsoluteTime intervalstart ARGS((TimeInterval i));
extern AbsoluteTime intervalend ARGS((TimeInterval i));
extern int isreltime ARGS((char *timestring, int *sign, long *quantity, int *unitnr));
extern int correct_unit ARGS((char unit[], int *unptr));
extern int correct_dir ARGS((char direction[], int *signptr));
extern int istinterval ARGS((char *i_string, AbsoluteTime *i_start, AbsoluteTime *i_end));

/* dt.c */
extern int32 dtin ARGS((char *datetime));
extern char *dtout ARGS((int32 datetime));

/* filename.c */
extern char *filename_in ARGS((char *file));
extern char *filename_out ARGS((char *s));

/* float.c */
extern float32 float4in ARGS((char *num));
extern char *float4out ARGS((float32 num));
extern float64 float8in ARGS((char *num));
extern char *float8out ARGS((float64 num));
extern float32 float4abs ARGS((float32 arg1));
extern float32 float4um ARGS((float32 arg1));
extern float32 float4larger ARGS((float32 arg1, float32 arg2));
extern float32 float4smaller ARGS((float32 arg1, float32 arg2));
extern float64 float8abs ARGS((float64 arg1));
extern float64 float8um ARGS((float64 arg1));
extern float64 float8larger ARGS((float64 arg1, float64 arg2));
extern float64 float8smaller ARGS((float64 arg1, float64 arg2));
extern float32 float4pl ARGS((float32 arg1, float32 arg2));
extern float32 float4mi ARGS((float32 arg1, float32 arg2));
extern float32 float4mul ARGS((float32 arg1, float32 arg2));
extern float32 float4div ARGS((float32 arg1, float32 arg2));
extern float32 float4inc ARGS((float32 arg1));
extern float64 float8pl ARGS((float64 arg1, float64 arg2));
extern float64 float8mi ARGS((float64 arg1, float64 arg2));
extern float64 float8mul ARGS((float64 arg1, float64 arg2));
extern float64 float8div ARGS((float64 arg1, float64 arg2));
extern float64 float8inc ARGS((float64 arg1));
extern long float4eq ARGS((float32 arg1, float32 arg2));
extern long float4ne ARGS((float32 arg1, float32 arg2));
extern long float4lt ARGS((float32 arg1, float32 arg2));
extern long float4le ARGS((float32 arg1, float32 arg2));
extern long float4gt ARGS((float32 arg1, float32 arg2));
extern long float4ge ARGS((float32 arg1, float32 arg2));
extern long float8eq ARGS((float64 arg1, float64 arg2));
extern long float8ne ARGS((float64 arg1, float64 arg2));
extern long float8lt ARGS((float64 arg1, float64 arg2));
extern long float8le ARGS((float64 arg1, float64 arg2));
extern long float8gt ARGS((float64 arg1, float64 arg2));
extern long float8ge ARGS((float64 arg1, float64 arg2));
extern float64 ftod ARGS((float32 num));
extern float32 dtof ARGS((float64 num));
extern float64 dround ARGS((float64 arg1));
extern float64 dtrunc ARGS((float64 arg1));
extern float64 dsqrt ARGS((float64 arg1));
extern float64 dcbrt ARGS((float64 arg1));
extern float64 dpow ARGS((float64 arg1, float64 arg2));
extern float64 dexp ARGS((float64 arg1));
extern float64 dlog1 ARGS((float64 arg1));
extern float64 float48pl ARGS((float32 arg1, float64 arg2));
extern float64 float48mi ARGS((float32 arg1, float64 arg2));
extern float64 float48mul ARGS((float32 arg1, float64 arg2));
extern float64 float48div ARGS((float32 arg1, float64 arg2));
extern float64 float84pl ARGS((float64 arg1, float32 arg2));
extern float64 float84mi ARGS((float64 arg1, float32 arg2));
extern float64 float84mul ARGS((float64 arg1, float32 arg2));
extern float64 float84div ARGS((float64 arg1, float32 arg2));
extern long float48eq ARGS((float32 arg1, float64 arg2));
extern long float48ne ARGS((float32 arg1, float64 arg2));
extern long float48lt ARGS((float32 arg1, float64 arg2));
extern long float48le ARGS((float32 arg1, float64 arg2));
extern long float48gt ARGS((float32 arg1, float64 arg2));
extern long float48ge ARGS((float32 arg1, float64 arg2));
extern long float84eq ARGS((float64 arg1, float32 arg2));
extern long float84ne ARGS((float64 arg1, float32 arg2));
extern long float84lt ARGS((float64 arg1, float32 arg2));
extern long float84le ARGS((float64 arg1, float32 arg2));
extern long float84gt ARGS((float64 arg1, float32 arg2));
extern long float84ge ARGS((float64 arg1, float32 arg2));

/* in utils/adt/ftype.c -- mao's stuff */
/* public interfaces only */
extern ObjectId pftp_write ARGS((struct varlena *host, int port));
extern int pftp_read ARGS((struct varlena *host, int port, ObjectId foid));
extern int pftp_open ARGS((struct varlena *host, int port));
extern ObjectId fimport ARGS((struct varlena *name));
extern int32 fexport ARGS((struct varlena *name, ObjectId foid));
extern int32 fabstract ARGS((struct varlena *name, ObjectId foid, int32 blksize, int32 offset, int32 size));

/* geo-ops.c */
extern BOX *box_in ARGS((char *str));
extern char *box_out ARGS((BOX *box));
extern BOX *box_construct ARGS((double x1, double x2, double y1, double y2));
extern BOX *box_fill ARGS((BOX *result, double x1, double x2, double y1, double y2));
extern BOX *box_copy ARGS((BOX *box));
extern long box_same ARGS((BOX *box1, BOX *box2));
extern long box_overlap ARGS((BOX *box1, BOX *box2));
extern long box_overleft ARGS((BOX *box1, BOX *box2));
extern long box_left ARGS((BOX *box1, BOX *box2));
extern long box_right ARGS((BOX *box1, BOX *box2));
extern long box_overright ARGS((BOX *box1, BOX *box2));
extern long box_contained ARGS((BOX *box1, BOX *box2));
extern long box_contain ARGS((BOX *box1, BOX *box2));
extern long box_below ARGS((BOX *box1, BOX *box2));
extern long box_above ARGS((BOX *box1, BOX *box2));
extern long box_lt ARGS((BOX *box1, BOX *box2));
extern long box_gt ARGS((BOX *box1, BOX *box2));
extern long box_eq ARGS((BOX *box1, BOX *box2));
extern long box_le ARGS((BOX *box1, BOX *box2));
extern long box_ge ARGS((BOX *box1, BOX *box2));
extern double *box_area ARGS((BOX *box));
extern double *box_length ARGS((BOX *box));
extern double *box_height ARGS((BOX *box));
extern double *box_distance ARGS((BOX *box1, BOX *box2));
extern POINT *box_center ARGS((BOX *box));
extern double box_ar ARGS((BOX *box));
extern double box_ln ARGS((BOX *box));
extern double box_ht ARGS((BOX *box));
extern double box_dt ARGS((BOX *box1, BOX *box2));
extern BOX *box_intersect ARGS((BOX *box1, BOX *box2));
extern LSEG *box_diagonal ARGS((BOX *box));
extern LINE *line_construct_pm ARGS((POINT *pt, double m));
extern LINE *line_construct_pp ARGS((POINT *pt1, POINT *pt2));
extern long line_intersect ARGS((LINE *l1, LINE *l2));
extern long line_parallel ARGS((LINE *l1, LINE *l2));
extern long line_perp ARGS((LINE *l1, LINE *l2));
extern long line_vertical ARGS((LINE *line));
extern long line_horizontal ARGS((LINE *line));
extern long line_eq ARGS((LINE *l1, LINE *l2));
extern double *line_distance ARGS((LINE *l1, LINE *l2));
extern POINT *line_interpt ARGS((LINE *l1, LINE *l2));
extern PATH *path_in ARGS((char *str));
extern char *path_out ARGS((PATH *path));
extern long path_n_lt ARGS((PATH *p1, PATH *p2));
extern long path_n_gt ARGS((PATH *p1, PATH *p2));
extern long path_n_eq ARGS((PATH *p1, PATH *p2));
extern long path_n_le ARGS((PATH *p1, PATH *p2));
extern long path_n_ge ARGS((PATH *p1, PATH *p2));
extern long path_inter ARGS((PATH *p1, PATH *p2));
extern double *path_distance ARGS((PATH *p1, PATH *p2));
extern double *path_length ARGS((PATH *path));
extern double path_ln ARGS((PATH *path));
extern POINT *point_in ARGS((char *str));
extern char *point_out ARGS((POINT *pt));
extern POINT *point_construct ARGS((double x, double y));
extern POINT *point_copy ARGS((POINT *pt));
extern long point_left ARGS((POINT *pt1, POINT *pt2));
extern long point_right ARGS((POINT *pt1, POINT *pt2));
extern long point_above ARGS((POINT *pt1, POINT *pt2));
extern long point_below ARGS((POINT *pt1, POINT *pt2));
extern long point_vert ARGS((POINT *pt1, POINT *pt2));
extern long point_horiz ARGS((POINT *pt1, POINT *pt2));
extern long point_eq ARGS((POINT *pt1, POINT *pt2));
extern long pointdist ARGS((POINT *p1, POINT *p2));
extern double *point_distance ARGS((POINT *pt1, POINT *pt2));
extern double point_dt ARGS((POINT *pt1, POINT *pt2));
extern double *point_slope ARGS((POINT *pt1, POINT *pt2));
extern double point_sl ARGS((POINT *pt1, POINT *pt2));
extern LSEG *lseg_in ARGS((char *str));
extern char *lseg_out ARGS((LSEG *ls));
extern LSEG *lseg_construct ARGS((POINT *pt1, POINT *pt2));
extern void statlseg_construct ARGS((LSEG *lseg, POINT *pt1, POINT *pt2));
extern long lseg_intersect ARGS((LSEG *l1, LSEG *l2));
extern long lseg_parallel ARGS((LSEG *l1, LSEG *l2));
extern long lseg_perp ARGS((LSEG *l1, LSEG *l2));
extern long lseg_vertical ARGS((LSEG *lseg));
extern long lseg_horizontal ARGS((LSEG *lseg));
extern long lseg_eq ARGS((LSEG *l1, LSEG *l2));
extern double *lseg_distance ARGS((LSEG *l1, LSEG *l2));
extern double lseg_dt ARGS((LSEG *l1, LSEG *l2));
extern POINT *lseg_interpt ARGS((LSEG *l1, LSEG *l2));
extern double *dist_pl ARGS((POINT *pt, LINE *line));
extern double *dist_ps ARGS((POINT *pt, LSEG *lseg));
extern double *dist_ppth ARGS((POINT *pt, PATH *path));
extern double *dist_pb ARGS((POINT *pt, BOX *box));
extern double *dist_sl ARGS((LSEG *lseg, LINE *line));
extern double *dist_sb ARGS((LSEG *lseg, BOX *box));
extern double *dist_lb ARGS((LINE *line, BOX *box));
extern POINT *interpt_sl ARGS((LSEG *lseg, LINE *line));
extern POINT *close_pl ARGS((POINT *pt, LINE *line));
extern POINT *close_ps ARGS((POINT *pt, LSEG *lseg));
extern POINT *close_pb ARGS((POINT *pt, BOX *box));
extern POINT *close_sl ARGS((LSEG *lseg, LINE *line));
extern POINT *close_sb ARGS((LSEG *lseg, BOX *box));
extern POINT *close_lb ARGS((LINE *line, BOX *box));
extern long on_pl ARGS((POINT *pt, LINE *line));
extern long on_ps ARGS((POINT *pt, LSEG *lseg));
extern long on_pb ARGS((POINT *pt, BOX *box));
extern long on_ppath ARGS((POINT *pt, PATH *path));
extern long on_sl ARGS((LSEG *lseg, LINE *line));
extern long on_sb ARGS((LSEG *lseg, BOX *box));
extern long inter_sl ARGS((LSEG *lseg, LINE *line));
extern long inter_sb ARGS((LSEG *lseg, BOX *box));
extern long inter_lb ARGS((LINE *line, BOX *box));
extern void make_bound_box ARGS((POLYGON *poly));
extern POLYGON *poly_in ARGS((char *s));
extern long poly_pt_count ARGS((char *s, int delim));
extern char *poly_out ARGS((POLYGON *poly));
extern double poly_max ARGS((double *coords, int ncoords));
extern double poly_min ARGS((double *coords, int ncoords));
extern long poly_left ARGS((POLYGON *polya, POLYGON *polyb));
extern long poly_overleft ARGS((POLYGON *polya, POLYGON *polyb));
extern long poly_right ARGS((POLYGON *polya, POLYGON *polyb));
extern long poly_overright ARGS((POLYGON *polya, POLYGON *polyb));
extern long poly_same ARGS((POLYGON *polya, POLYGON *polyb));
extern long poly_overlap ARGS((POLYGON *polya, POLYGON *polyb));
extern long poly_contain ARGS((POLYGON *polya, POLYGON *polyb));
extern long poly_contained ARGS((POLYGON *polya, POLYGON *polyb));

/* geo-selfuncs.c */
extern float64 areasel ARGS((ObjectId opid, ObjectId relid, AttributeNumber attno, char *value, int32 flag));
extern float64 areajoinsel ARGS((ObjectId opid, ObjectId relid, AttributeNumber attno, char *value, int32 flag));
extern float64 leftsel ARGS((ObjectId opid, ObjectId relid, AttributeNumber attno, char *value, int32 flag));
extern float64 leftjoinsel ARGS((ObjectId opid, ObjectId relid, AttributeNumber attno, char *value, int32 flag));
extern float64 contsel ARGS((ObjectId opid, ObjectId relid, AttributeNumber attno, char *value, int32 flag));
extern float64 contjoinsel ARGS((ObjectId opid, ObjectId relid, AttributeNumber attno, char *value, int32 flag));

/* lo_regprocs.c */
extern char *lo_filein ARGS((char *filename));
extern char *lo_fileout ARGS((LargeObject *object));

/* misc.c */
extern bool NullValue ARGS((Datum value, Boolean *isNull));
extern bool NonNullValue ARGS((Datum value, Boolean *isNull));
extern int32 userfntest ARGS((int i));

/* not_in.c */
extern bool int4notin ARGS((int16 not_in_arg, char *relation_and_attr));
extern bool oidnotin ARGS((ObjectId the_oid, char *compare));
extern int my_varattno ARGS((Relation rd, char *a));

/* oid.c */
extern ObjectId *oid8in ARGS((char *oidString));
extern char *oid8out ARGS((ObjectId (*oidArray)[]));
extern ObjectId oidin ARGS((char *s));
extern char *oidout ARGS((ObjectId o));
extern int32 oideq ARGS((ObjectId arg1, ObjectId arg2));
extern int32 oidne ARGS((ObjectId arg1, ObjectId arg2));
extern int32 oid8eq ARGS((ObjectId arg1[], ObjectId arg2[]));

/* regexp.c */
extern bool char2regexeq ARGS((uint16 arg1, struct varlena *p));
extern bool char2regexne ARGS((uint16 arg1, struct varlena *p));
extern bool char4regexeq ARGS((uint32 arg1, struct varlena *p));
extern bool char4regexne ARGS((uint32 arg1, struct varlena *p));
extern bool char8regexeq ARGS((char *s, struct varlena *p));
extern bool char8regexne ARGS((char *s, struct varlena *p));
extern bool char16regexeq ARGS((char *s, struct varlena *p));
extern bool char16regexne ARGS((char *s, struct varlena *p));
extern bool textregexeq ARGS((struct varlena *s, struct varlena *p));
extern bool textregexne ARGS((char *s, char *p));

/* regproc.c */
extern int32 regprocin ARGS((char *proname));
extern char *regprocout ARGS((RegProcedure proid));
extern ObjectId RegprocToOid ARGS((RegProcedure rp));

/* selfuncs.c */
extern float64 eqsel ARGS((ObjectId opid, ObjectId relid, AttributeNumber attno, char *value, int32 flag));
extern float64 neqsel ARGS((ObjectId opid, ObjectId relid, AttributeNumber attno, char *value, int32 flag));
extern float64 intltsel ARGS((ObjectId opid, ObjectId relid, AttributeNumber attno, int32 value, int32 flag));
extern float64 intgtsel ARGS((ObjectId opid, ObjectId relid, AttributeNumber attno, int32 value, int32 flag));
extern float64 eqjoinsel ARGS((ObjectId opid, ObjectId relid1, AttributeNumber attno1, ObjectId relid2, AttributeNumber attno2));
extern float64 neqjoinsel ARGS((ObjectId opid, ObjectId relid1, AttributeNumber attno1, ObjectId relid2, AttributeNumber attno2));
extern float64 intltjoinsel ARGS((ObjectId opid, ObjectId relid1, AttributeNumber attno1, ObjectId relid2, AttributeNumber attno2));
extern float64 intgtjoinsel ARGS((ObjectId opid, ObjectId relid1, AttributeNumber attno1, ObjectId relid2, AttributeNumber attno2));
extern int32 getattnvals ARGS((ObjectId relid, AttributeNumber attnum));
extern int gethilokey ARGS((ObjectId relid, AttributeNumber attnum, ObjectId opid, char **high, char **low));
extern float64 btreesel ARGS((ObjectId operatorObjectId, ObjectId indrelid, AttributeNumber attributeNumber, char *constValue, int32 constFlag, int32 nIndexKeys, ObjectId indexrelid));
extern float64 btreenpage ARGS((ObjectId operatorObjectId, ObjectId indrelid, AttributeNumber attributeNumber, char *constValue, int32 constFlag, int32 nIndexKeys, ObjectId indexrelid));
extern float64 hashsel ARGS((ObjectId operatorObjectId, ObjectId indrelid, AttributeNumber attributeNumber, char *constValue, int32 constFlag, int32 nIndexKeys, ObjectId indexrelid));
extern float64 hashnpage ARGS((ObjectId operatorObjectId, ObjectId indrelid, AttributeNumber attributeNumber, char *constValue, int32 constFlag, int32 nIndexKeys, ObjectId indexrelid));
extern float64 rtsel ARGS((ObjectId operatorObjectId, ObjectId indrelid, AttributeNumber attributeNumber, char *constValue, int32 constFlag, int32 nIndexKeys, ObjectId indexrelid));
extern float64 rtnpage ARGS((ObjectId operatorObjectId, ObjectId indrelid, AttributeNumber attributeNumber, char *constValue, int32 constFlag, int32 nIndexKeys, ObjectId indexrelid));

/* storage/smgr/smgrtype.c */
extern int2 smgrin ARGS((char *s));
extern char *smgrout ARGS((int2 i));
extern bool smgreq ARGS((int2 a, int2 b));
extern bool smgrne ARGS((int2 a, int2 b));

/* tid.c */
extern ItemPointer tidin ARGS((char *str));
extern char *tidout ARGS((ItemPointer itemPtr));

/* varlena.c */
extern struct varlena *byteain ARGS((char *inputText));
extern struct varlena *shove_bytes ARGS((unsigned char *stuff, int len));
extern char *byteaout ARGS((struct varlena *vlena));
extern struct varlena *textin ARGS((char *inputText));
extern char *textout ARGS((struct varlena *vlena));
extern int32 texteq ARGS((struct varlena *arg1, struct varlena *arg2));
extern int32 textne ARGS((struct varlena *arg1, struct varlena *arg2));
extern int32 text_lt ARGS((struct varlena *arg1, struct varlena *arg2));
extern int32 text_le ARGS((struct varlena *arg1, struct varlena *arg2));
extern int32 text_gt ARGS((struct varlena *arg1, struct varlena *arg2));
extern int32 text_ge ARGS((struct varlena *arg1, struct varlena *arg2));
extern int32 byteaGetSize ARGS((struct varlena *v));
extern int32 byteaGetByte ARGS((struct varlena *v, int32 n));
extern int32 byteaGetBit ARGS((struct varlena *v, int32 n));
extern struct varlena *byteaSetByte ARGS((struct varlena *v, int32 n, int32 newByte));
extern struct varlena *byteaSetBit ARGS((struct varlena *v, int32 n, int32 newBit));

/* libpq/be-fsstubs.c */
extern int LOopen ARGS((char *fname, int mode));
extern int LOclose ARGS((int fd));
extern struct varlena *LOread ARGS((int fd, int len));
extern int LOwrite ARGS((int fd, struct varlena *wbuf));
extern int LO_read ARGS((char *buf, int len, int fd));
extern int LO_write ARGS((char *wbuf, int len, int fd));
extern int LOlseek ARGS((int fd, int offset, int whence));
extern int LOcreat ARGS((char *path, int mode, int objtype));
extern int LOtell ARGS((int fd));
extern int LOftruncate ARGS((void));
extern struct varlena *LOstat ARGS((char *path));
extern int LOmkdir ARGS((char *path, int mode));
extern int LOrmdir ARGS((char *path));
extern int LOunlink ARGS((char *path));
extern int NewLOfd ARGS((char *lobjCookie, int objtype));
extern void DeleteLOfd ARGS((int fd));

/* catalog/pg_naming.c */
extern oid FilenameToOID ARGS((char *fname));
extern void CreateNameTuple ARGS((oid parentID, char *name, oid ourid));
extern oid CreateNewNameTuple ARGS((oid parentID, char *name));
extern oid DeleteNameTuple ARGS((oid parentID, char *name));
extern oid LOcreatOID ARGS((char *fname, int mode));
extern oid LOpathOID ARGS((char *fname, int mode));
extern int LOunlinkOID ARGS((char *fname));
extern int LOisemptydir ARGS((char *path));
extern int LOisdir ARGS((char *path));
extern int LOrename ARGS((char *path, char *newpath));
extern int path_parse ARGS((char *pathname, char *pathcomp[], int n_comps));
extern void to_basename ARGS((char *fname, char *bname, char *tname));

/* acl.c */
/* public interfaces only -- these are fake, the real ones are in tmp/acl.h */
extern struct varlena *aclitemin ARGS((char *s));
extern char *aclitemout ARGS((struct varlena *s));
extern struct varlena *aclinsert ARGS((struct varlena *acl, char *aclitem));
extern struct varlena *aclremove ARGS((struct varlena *acl, char *aclitem));
extern int aclcontains ARGS((struct varlena *acl, char *aclitem));

#endif !BuiltinsIncluded
