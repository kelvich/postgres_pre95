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
int32 boolin ARGS((char *b ));
char *boolout ARGS((long b ));

/* char.c */
int32 charin ARGS((char *ch ));
char *charout ARGS((int32 ch ));
int32 cidin ARGS((char *s ));
char *cidout ARGS((int32 c ));
int32 chareq ARGS((int8 arg1 , int8 arg2 ));
int32 charne ARGS((int8 arg1 , int8 arg2 ));
int32 charlt ARGS((int8 arg1 , int8 arg2 ));
int32 charle ARGS((int8 arg1 , int8 arg2 ));
int32 chargt ARGS((int8 arg1 , int8 arg2 ));
int32 charge ARGS((int8 arg1 , int8 arg2 ));
int32 charpl ARGS((int8 arg1 , int8 arg2 ));
int32 charmi ARGS((int8 arg1 , int8 arg2 ));
int32 charmul ARGS((int8 arg1 , int8 arg2 ));
int32 chardiv ARGS((int8 arg1 , int8 arg2 ));
char *char16in ARGS((char *s ));
char *char16out ARGS((char *s ));
int32 char16eq ARGS((char *arg1 , char *arg2 ));
int32 char16ne ARGS((char *arg1 , char *arg2 ));
int32 char16lt ARGS((char *arg1 , char *arg2 ));
int32 char16le ARGS((char *arg1 , char *arg2 ));
int32 char16gt ARGS((char *arg1 , char *arg2 ));
int32 char16ge ARGS((char *arg1 , char *arg2 ));
char *pg_username ARGS((void));

/* int.c */
int32 int2in ARGS((char *num ));
char *int2out ARGS((int16 sh ));
int16 *int28in ARGS((char *shs ));
char *int28out ARGS((int16 (*shs )[]));
int32 *int44in ARGS((char *input_string ));
char *int44out ARGS((int32 an_array []));
int32 int4in ARGS((char *num ));
char *int4out ARGS((int32 l ));
int32 itoi ARGS((int32 arg1 ));
int32 int4eq ARGS((int32 arg1 , int32 arg2 ));
int32 int4ne ARGS((int32 arg1 , int32 arg2 ));
int32 int4lt ARGS((int32 arg1 , int32 arg2 ));
int32 int4le ARGS((int32 arg1 , int32 arg2 ));
int32 int4gt ARGS((int32 arg1 , int32 arg2 ));
int32 int4ge ARGS((int32 arg1 , int32 arg2 ));
int32 keyfirsteq ARGS((int16 *arg1 , int16 arg2 ));
int32 int2eq ARGS((int16 arg1 , int16 arg2 ));
int32 int2ne ARGS((int16 arg1 , int16 arg2 ));
int32 int2lt ARGS((int16 arg1 , int16 arg2 ));
int32 int2le ARGS((int16 arg1 , int16 arg2 ));
int32 int2gt ARGS((int16 arg1 , int16 arg2 ));
int32 int2ge ARGS((int16 arg1 , int16 arg2 ));
int32 int4pl ARGS((int32 arg1 , int32 arg2 ));
int32 int4mi ARGS((int32 arg1 , int32 arg2 ));
int32 int4mul ARGS((int32 arg1 , int32 arg2 ));
int32 int4div ARGS((int32 arg1 , int32 arg2 ));
int32 int4inc ARGS((int32 arg ));
int32 int2pl ARGS((int16 arg1 , int16 arg2 ));
int32 int2mi ARGS((int16 arg1 , int16 arg2 ));
int32 int2mul ARGS((int16 arg1 , int16 arg2 ));
int32 int2div ARGS((int16 arg1 , int16 arg2 ));
int32 int2inc ARGS((int16 arg ));
int16 int2larger ARGS((int16 arg1 , int16 arg2 ));
int16 int2smaller ARGS((int16 arg1 , int16 arg2 ));
int32 int4larger ARGS((int32 arg1 , int32 arg2 ));
int32 int4smaller ARGS((int32 arg1 , int32 arg2 ));

extern int32		inteq();
extern int32		intne();
extern int32		intlt();
extern int32		intle();
extern int32		intgt();
extern int32		intge();
extern int32		intpl();
extern int32		intmi();
extern int32		intdiv();

int32 int4mod ARGS((int32 arg1 , int32 arg2 ));
int32 int2mod ARGS((int16 arg1 , int16 arg2 ));
int32 int4fac ARGS((int32 arg1 ));
int32 int2fac ARGS((int16 arg1 ));
extern int32		intmod();

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
extern int		rt_box_size();
extern int		rt_bigbox_size();
extern int		rt_poly_size();
extern POLYGON	*rt_poly_union();
extern POLYGON	*rt_poly_inter();

/* rule locks */
extern Datum GetAttribute();


extern int32 pqtest();

/* arrayfuncs.c */
char *array_in ARGS((char *string , ObjectId element_type ));
char *array_out ARGS((char *items , ObjectId element_type ));

/* date.c */
AbsoluteTime nabstimein ARGS((char *datetime ));
char *nabstimeout ARGS((AbsoluteTime datetime ));
int32 reltimein ARGS((char *timestring ));
char *reltimeout ARGS((int32 timevalue ));
int32 reltimeeq ARGS((RelativeTime t1 , RelativeTime t2 ));
int32 reltimene ARGS((RelativeTime t1 , RelativeTime t2 ));
int32 reltimelt ARGS((int32 t1 , int32 t2 ));
int32 reltimegt ARGS((int32 t1 , int32 t2 ));
int32 reltimele ARGS((int32 t1 , int32 t2 ));
int32 reltimege ARGS((int32 t1 , int32 t2 ));
AbsoluteTime timepl ARGS((AbsoluteTime AbsTime_t1 , RelativeTime RelTime_t2 ));
int32 abstimeeq ARGS((AbsoluteTime t1 , AbsoluteTime t2 ));
int32 abstimene ARGS((AbsoluteTime t1 , AbsoluteTime t2 ));
int32 abstimelt ARGS((int32 t1 , int32 t2 ));
int32 abstimegt ARGS((int32 t1 , int32 t2 ));
int32 abstimele ARGS((int32 t1 , int32 t2 ));
int32 abstimege ARGS((int32 t1 , int32 t2 ));
TimeInterval tintervalin ARGS((char *intervalstr ));
char *tintervalout ARGS((TimeInterval interval ));
int ininterval ARGS((int32 t , TimeInterval interval ));
RelativeTime intervalrel ARGS((TimeInterval interval ));
int32 intervaleq ARGS((TimeInterval i1 , TimeInterval i2 ));
int32 intervalct ARGS((TimeInterval i1 , TimeInterval i2 ));
int32 intervalov ARGS((TimeInterval i1 , TimeInterval i2 ));
int32 intervalleneq ARGS((TimeInterval i , RelativeTime t ));
int32 intervallenne ARGS((TimeInterval i , RelativeTime t ));
int32 intervallenlt ARGS((TimeInterval i , RelativeTime t ));
int32 intervallengt ARGS((TimeInterval i , RelativeTime t ));
int32 intervallenle ARGS((TimeInterval i , RelativeTime t ));
int32 intervallenge ARGS((TimeInterval i , RelativeTime t ));
AbsoluteTime intervalstart ARGS((TimeInterval i ));
AbsoluteTime intervalend ARGS((TimeInterval i ));
AbsoluteTime timemi ARGS((AbsoluteTime AbsTime_t1 , RelativeTime RelTime_t2 ));
AbsoluteTime timenow ARGS((void ));


/* dt.c */
int32 dtin ARGS((char *datetime ));
char *dtout ARGS((int32 datetime ));

/* filename.c */
char *filename_in ARGS((char *file ));
char *filename_out ARGS((char *s ));

/* float.c */
float32 float4in ARGS((char *num ));
char *float4out ARGS((float32 num ));
float64 float8in ARGS((char *num ));
char *float8out ARGS((float64 num ));
float32 float4abs ARGS((float32 arg1 ));
float32 float4um ARGS((float32 arg1 ));
float32 float4larger ARGS((float32 arg1 , float32 arg2 ));
float32 float4smaller ARGS((float32 arg1 , float32 arg2 ));
float64 float8abs ARGS((float64 arg1 ));
float64 float8um ARGS((float64 arg1 ));
float64 float8larger ARGS((float64 arg1 , float64 arg2 ));
float64 float8smaller ARGS((float64 arg1 , float64 arg2 ));
float32 float4pl ARGS((float32 arg1 , float32 arg2 ));
float32 float4mi ARGS((float32 arg1 , float32 arg2 ));
float32 float4mul ARGS((float32 arg1 , float32 arg2 ));
float32 float4div ARGS((float32 arg1 , float32 arg2 ));
float32 float4inc ARGS((float32 arg1 ));
float64 float8pl ARGS((float64 arg1 , float64 arg2 ));
float64 float8mi ARGS((float64 arg1 , float64 arg2 ));
float64 float8mul ARGS((float64 arg1 , float64 arg2 ));
float64 float8div ARGS((float64 arg1 , float64 arg2 ));
float64 float8inc ARGS((float64 arg1 ));
long float4eq ARGS((float32 arg1 , float32 arg2 ));
long float4ne ARGS((float32 arg1 , float32 arg2 ));
long float4lt ARGS((float32 arg1 , float32 arg2 ));
long float4le ARGS((float32 arg1 , float32 arg2 ));
long float4gt ARGS((float32 arg1 , float32 arg2 ));
long float4ge ARGS((float32 arg1 , float32 arg2 ));
long float8eq ARGS((float64 arg1 , float64 arg2 ));
long float8ne ARGS((float64 arg1 , float64 arg2 ));
long float8lt ARGS((float64 arg1 , float64 arg2 ));
long float8le ARGS((float64 arg1 , float64 arg2 ));
long float8gt ARGS((float64 arg1 , float64 arg2 ));
long float8ge ARGS((float64 arg1 , float64 arg2 ));
float64 ftod ARGS((float32 num ));
float32 dtof ARGS((float64 num ));
float64 dround ARGS((float64 arg1 ));
float64 dtrunc ARGS((float64 arg1 ));
float64 dsqrt ARGS((float64 arg1 ));
float64 dcbrt ARGS((float64 arg1 ));
float64 dpow ARGS((float64 arg1 , float64 arg2 ));
float64 dexp ARGS((float64 arg1 ));
float64 dlog1 ARGS((float64 arg1 ));
float64 float48pl ARGS((float32 arg1 , float64 arg2 ));
float64 float48mi ARGS((float32 arg1 , float64 arg2 ));
float64 float48mul ARGS((float32 arg1 , float64 arg2 ));
float64 float48div ARGS((float32 arg1 , float64 arg2 ));
float64 float84pl ARGS((float64 arg1 , float32 arg2 ));
float64 float84mi ARGS((float64 arg1 , float32 arg2 ));
float64 float84mul ARGS((float64 arg1 , float32 arg2 ));
float64 float84div ARGS((float64 arg1 , float32 arg2 ));
long float48eq ARGS((float32 arg1 , float64 arg2 ));
long float48ne ARGS((float32 arg1 , float64 arg2 ));
long float48lt ARGS((float32 arg1 , float64 arg2 ));
long float48le ARGS((float32 arg1 , float64 arg2 ));
long float48gt ARGS((float32 arg1 , float64 arg2 ));
long float48ge ARGS((float32 arg1 , float64 arg2 ));
long float84eq ARGS((float64 arg1 , float32 arg2 ));
long float84ne ARGS((float64 arg1 , float32 arg2 ));
long float84lt ARGS((float64 arg1 , float32 arg2 ));
long float84le ARGS((float64 arg1 , float32 arg2 ));
long float84gt ARGS((float64 arg1 , float32 arg2 ));
long float84ge ARGS((float64 arg1 , float32 arg2 ));

/* in utils/adt/ftype.c -- mao's stuff */
ObjectId fimport ARGS((struct varlena *name ));
int32 fexport ARGS((struct varlena *name , ObjectId foid ));
int32 fabstract ARGS((struct varlena *name , ObjectId foid , int32 blksize , int32 offset , int32 size ));

/* geo-ops.c */
BOX *box_in ARGS((char *str ));
char *box_out ARGS((BOX *box ));
long box_same ARGS((BOX *box1 , BOX *box2 ));
long box_overlap ARGS((BOX *box1 , BOX *box2 ));
long box_overleft ARGS((BOX *box1 , BOX *box2 ));
long box_left ARGS((BOX *box1 , BOX *box2 ));
long box_right ARGS((BOX *box1 , BOX *box2 ));
long box_overright ARGS((BOX *box1 , BOX *box2 ));
long box_contained ARGS((BOX *box1 , BOX *box2 ));
long box_contain ARGS((BOX *box1 , BOX *box2 ));
long box_below ARGS((BOX *box1 , BOX *box2 ));
long box_above ARGS((BOX *box1 , BOX *box2 ));
long box_lt ARGS((BOX *box1 , BOX *box2 ));
long box_gt ARGS((BOX *box1 , BOX *box2 ));
long box_eq ARGS((BOX *box1 , BOX *box2 ));
long box_le ARGS((BOX *box1 , BOX *box2 ));
long box_ge ARGS((BOX *box1 , BOX *box2 ));
POINT *box_center ARGS((BOX *box ));
PATH *path_in ARGS((char *str ));
char *path_out ARGS((PATH *path ));
POINT *point_in ARGS((char *str ));
char *point_out ARGS((POINT *pt ));
long point_left ARGS((POINT *pt1 , POINT *pt2 ));
long point_right ARGS((POINT *pt1 , POINT *pt2 ));
long point_above ARGS((POINT *pt1 , POINT *pt2 ));
long point_below ARGS((POINT *pt1 , POINT *pt2 ));
long point_eq ARGS((POINT *pt1 , POINT *pt2 ));
long pointdist ARGS((POINT *p1 , POINT *p2 ));
LSEG *lseg_in ARGS((char *str ));
char *lseg_out ARGS((LSEG *ls ));
POLYGON *poly_in ARGS((char *s ));
char *poly_out ARGS((POLYGON *poly ));
long poly_left ARGS((POLYGON *polya , POLYGON *polyb ));
long poly_overleft ARGS((POLYGON *polya , POLYGON *polyb ));
long poly_right ARGS((POLYGON *polya , POLYGON *polyb ));
long poly_overright ARGS((POLYGON *polya , POLYGON *polyb ));
long poly_same ARGS((POLYGON *polya , POLYGON *polyb ));
long poly_overlap ARGS((POLYGON *polya , POLYGON *polyb ));
long poly_contain ARGS((POLYGON *polya , POLYGON *polyb ));
long poly_contained ARGS((POLYGON *polya , POLYGON *polyb ));
double *path_distance ARGS((PATH *p1, PATH *p2));
double *dist_ppth ARGS((POINT *pt, PATH *path));

/* geo-selfuncs.c */
float64 areasel ARGS((ObjectId opid , ObjectId relid , AttributeNumber attno , char *value , int32 flag ));
float64 areajoinsel ARGS((ObjectId opid , ObjectId relid , AttributeNumber attno , char *value , int32 flag ));

/* lo_regprocs.c */
char *lo_filein ARGS((char *filename ));
char *lo_fileout ARGS((LargeObject *object ));

/* misc.c */
int32 userfntest ARGS((int i ));

/* not_in.c */
bool int4notin ARGS((int16 not_in_arg , char *relation_and_attr ));
bool oidnotin ARGS((ObjectId the_oid , char *compare ));

/* oid.c */
ObjectId *oid8in ARGS((char *oidString ));
char *oid8out ARGS((ObjectId (*oidArray )[]));

/* regexp.c */
bool char16regexeq ARGS((char *s , char *p ));
bool char16regexne ARGS((char *s , char *p ));
bool textregexeq ARGS((struct varlena *s , struct varlena *p ));
bool textregexne ARGS((char *s , char *p ));

/* regproc.c */
int32 regprocin ARGS((char *proname ));
char *regprocout ARGS((RegProcedure proid ));
ObjectId RegprocToOid ARGS((RegProcedure rp));

/* selfuncs.c */
float64 eqsel ARGS((ObjectId opid , ObjectId relid , AttributeNumber attno , char *value , int32 flag ));
float64 neqsel ARGS((ObjectId opid , ObjectId relid , AttributeNumber attno , char *value , int32 flag ));
float64 intltsel ARGS((ObjectId opid , ObjectId relid , AttributeNumber attno , int32 value , int32 flag ));
float64 intgtsel ARGS((ObjectId opid , ObjectId relid , AttributeNumber attno , int32 value , int32 flag ));
float64 eqjoinsel ARGS((ObjectId opid , ObjectId relid1 , AttributeNumber attno1 , ObjectId relid2 , AttributeNumber attno2 ));
float64 neqjoinsel ARGS((ObjectId opid , ObjectId relid1 , AttributeNumber attno1 , ObjectId relid2 , AttributeNumber attno2 ));
float64 intltjoinsel ARGS((ObjectId opid , ObjectId relid1 , AttributeNumber attno1 , ObjectId relid2 , AttributeNumber attno2 ));
float64 intgtjoinsel ARGS((ObjectId opid , ObjectId relid1 , AttributeNumber attno1 , ObjectId relid2 , AttributeNumber attno2 ));
/*
 *  Selectivity functions for btrees in utils/adt/selfuncs.c
 */
float64 btreesel ARGS((ObjectId operatorObjectId , ObjectId indrelid , AttributeNumber attributeNumber , char *constValue , int32 constFlag , int32 nIndexKeys , ObjectId indexrelid ));
float64 btreenpage ARGS((ObjectId operatorObjectId , ObjectId indrelid , AttributeNumber attributeNumber , char *constValue , int32 constFlag , int32 nIndexKeys , ObjectId indexrelid ));
/*
 *  Selectivity functions for rtrees in utils/adt/selfuncs.c
 */
float64 rtsel ARGS((ObjectId operatorObjectId , ObjectId indrelid , AttributeNumber attributeNumber , char *constValue , int32 constFlag , int32 nIndexKeys , ObjectId indexrelid ));
float64 rtnpage ARGS((ObjectId operatorObjectId , ObjectId indrelid , AttributeNumber attributeNumber , char *constValue , int32 constFlag , int32 nIndexKeys , ObjectId indexrelid ));

/* smgr.c */
int2 smgrin ARGS((char *s ));
char *smgrout ARGS((int2 i ));
bool smgreq ARGS((int2 a , int2 b ));
bool smgrne ARGS((int2 a , int2 b ));

/* tid.c */
ItemPointer tidin ARGS((char *str ));
char *tidout ARGS((ItemPointer itemPtr ));

/* varlena.c */
struct varlena *byteain ARGS((char *inputText ));
char *byteaout ARGS((struct varlena *vlena ));
struct varlena *textin ARGS((char *inputText ));
char *textout ARGS((struct varlena *vlena ));
int32 texteq ARGS((struct varlena *arg1 , struct varlena *arg2 ));
int32 textne ARGS((struct varlena *arg1 , struct varlena *arg2 ));
int32 text_lt ARGS((struct varlena *arg1 , struct varlena *arg2 ));
int32 text_le ARGS((struct varlena *arg1 , struct varlena *arg2 ));
int32 text_gt ARGS((struct varlena *arg1 , struct varlena *arg2 ));
int32 text_ge ARGS((struct varlena *arg1 , struct varlena *arg2 ));
int32 byteaGetSize ARGS((struct varlena *v ));
int32 byteaGetByte ARGS((struct varlena *v , int32 n ));
int32 byteaGetBit ARGS((struct varlena *v , int32 n ));
struct varlena *byteaSetByte ARGS((struct varlena *v , int32 n , int32 newByte ));
struct varlena *byteaSetBit ARGS((struct varlena *v , int32 n , int32 newBit ));

     /* be-stubs.c */
int LOopen ARGS((char *fname , int mode ));
int LOclose ARGS((int fd ));
struct varlena *LOread ARGS((int fd , int len ));
int LOwrite ARGS((int fd , struct varlena *wbuf ));
int LOlseek ARGS((int fd , int offset , int whence ));
int LOcreat ARGS((char *path , int mode ));
int LOtell ARGS((int fd ));
int LOftruncate ARGS((void ));
struct varlena *LOstat ARGS((char *path ));
int LOmkdir ARGS((char *path , int mode ));
int LOrmdir ARGS((char *path ));
int LOunlink ARGS((char *path ));

/* naming.c */
oid FilenameToOID ARGS((char *fname ));
oid LOcreatOID ARGS((char *fname , int mode ));
int LOrename ARGS((char *path , char *newpath ));

int pftp_read ARGS((struct varlena *host, int port, oid foid));
oid pftp_write ARGS((struct varlena *host, int port));

#endif !BuiltinsIncluded

