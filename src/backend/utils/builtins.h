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

#ifdef FMGR_ADT
#include "utils/geo-decls.h"
#endif /* FMGR_ADT */


/*
 *	Defined in adt/
 */
extern int32		boolin();
extern char 		*boolout();
extern int32		charin();
extern char		*charout();
extern int32		cidin();
extern char		*cidout();
extern int32		chareq();
extern int32		charne();
extern int32		charlt();
extern int32		charle();
extern int32		chargt();
extern int32		charge();
extern int32		charpl();
extern int32		charmi();
extern int32		charmul();
extern int32		chardiv();

extern char		*char16in();
extern char		*char16out();
extern int32		char16eq();
extern int32		char16ne();
extern int32		char16lt();
extern int32		char16le();
extern int32		char16gt();
extern int32		char16ge();

extern int32		int2eq();
extern int32		int2lt();
extern int32		int4eq();
extern int32		int4lt();
extern int32		int4ne();
extern int32		int2ne();
extern int32		int2gt();
extern int32		int4gt();
extern int32		int2le();
extern int32		int4le();
extern int32		int4ge();
extern int32		int2ge();
extern int32		int2mul();
extern int32		int2div();
extern int32		int4div();
extern int32		int2mod();
extern int32		int2pl();
extern int32		int4pl();
extern int32		int2mi();
extern int32		int4mi();
extern int32		keyfirsteq();
extern Time		abstimein();
extern char		*abstimeout();
extern Time		reltimein();
extern char		*reltimeout();
extern Time		timepl();
extern int32		abstimeeq();
extern int32		abstimene();
extern int32		abstimelt();
extern int32		abstimegt();
extern int32		abstimele();
extern int32		abstimege();
/* XXX Other time stuff goes here ??? */
extern int32		dtin();
extern char		*dtout();
extern float32		float4in();
extern char		*float4out();
extern float32		float4abs();
extern float32		float4um();
extern float64		float8in();
extern char		*float8out();
extern float64		float8abs();
extern float64		float8um();
extern float32		float4pl();
extern float32		float4mi();
extern float32		float4mul();
extern float32		float4div();
extern float64		float8pl();
extern float64		float8mi();
extern float64 		float8mul();
extern float64		float8div();
extern int32		float4eq();
extern int32		float4ne();
extern int32		float4lt();
extern int32		float4le();
extern int32		float4gt();
extern int32		float4ge();
extern int32		float8eq();
extern int32		float8ne();
extern int32		float8lt();
extern int32		float8le();
extern int32		float8gt();
extern int32		float8ge();
extern float64		ftod();
extern float32		dtof();
#ifdef FMGR_MATH
extern float64		dround();
extern float64		dtrunc();
extern float64		dsqrt();
extern float64		dcbrt();
extern float64		dpow();
extern float64		dexp();
extern float64		dlog1();
extern float64		float48pl();
extern float64		float48mi();
extern float64		float48mul();
extern float64		float48div();
extern float64		float84pl();
extern float64		float84mi();
extern float64		float84mul();
extern float64		float84div();
extern int32		float48eq();
extern int32		float48ne();
extern int32		float48lt();
extern int32		float48le();
extern int32		float48gt();
extern int32		float48ge();
extern int32		float84eq();
extern int32		float84ne();
extern int32		float84lt();
extern int32		float84le();
extern int32		float84gt();
extern int32		float84ge();
#endif /* FMGR_MATH */
extern int32		int2in();
extern char		*int2out();
extern int16		*int28in();
extern char		*int28out();
extern int32		*int44in();
extern char		*int44out();
extern int32		int4in();
extern char		*int4out();
extern int32		itoi();
extern int32		inteq();
extern int32		intne();
extern int32		intlt();
extern int32		intle();
extern int32		intgt();
extern int32		intge();
extern int32		intpl();
extern int32		intmi();
extern int32		int4mul();
extern int32		intdiv();
#ifdef FMGR_MATH
extern int32		intmod();
extern int32		int4mod();
extern int32		int4fac();
extern int32		int2fac();
#endif /* FMGR_MATH */
extern ObjectId		*oid8in();
extern char		*oid8out();
extern int32		regprocin();
extern char		*regprocout();
extern float64		eqsel();
extern float64		neqsel();
extern float64		intltsel();
extern float64		intgtsel();
extern float64		eqjoinsel();
extern float64		neqjoinsel();
extern float64		intltjoinsel();
extern float64		intgtjoinsel();
extern struct varlena	*byteain();
extern char		*byteaout();

extern struct varlena	*textin();
extern char		*textout();
extern int32		texteq();
extern int32		textne();
extern int32		text_lt();
extern int32		text_le();
extern int32		text_gt();
extern int32		text_ge();

extern ItemPointer	tidin();
extern char		*tidout();

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
 *  Selectivity functions for btrees in utils/adt/selfuncs.c
 */
extern float64		btreesel();
extern float64		btreenpage();

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

/*
 *  Selectivity functions for rtrees in utils/adt/selfuncs.c
 */
extern float64		rtsel();
extern float64		rtnpage();

/* support routines for the rtree access method, by opclass */
#ifdef FMGR_ADT
extern BOX		*rt_box_union();
extern BOX		*rt_box_inter();
extern int		rt_box_size();
extern int		rt_bigbox_size();
extern int		rt_poly_size();
extern POLYGON	*rt_poly_union();
extern POLYGON	*rt_poly_inter();
#endif /* FMGR_ADT */

/*
 *	Defined in useradt/
 *
 * XXX These shouldn't be here at all -- they shouldn't be built-in.
 */
#ifdef FMGR_ADT
extern POINT	*point_in();
extern char	*point_out();
extern LSEG	*lseg_in();
extern char	*lseg_out();
extern PATH	*path_in();
extern char	*path_out();
extern BOX	*box_in();
extern char	*box_out();
extern long	box_overlap();
extern long	box_same();
extern long	box_contain();
extern long	box_left();
extern long	box_overleft();
extern long	box_overright();
extern long	box_right();
extern long	box_contained();
extern long	box_ge();
extern long	box_gt();
extern long	box_eq();
extern long	box_lt();
extern long	box_le();
extern long	point_above();
extern long	point_left();
extern long	point_right();
extern long	point_below();
extern long	point_eq();
extern long	on_pb();
extern long	on_ppath();
extern POINT	*box_center();
extern POLYGON	*poly_in();
extern char	*poly_out();
extern long	poly_overlap();
extern long	poly_same();
extern long	poly_contain();
extern long	poly_left();
extern long	poly_overleft();
extern long	poly_overright();
extern long	poly_right();
extern long	poly_contained();
extern float64	areasel();
extern float64	areajoinsel();
extern long	pointdist();

extern bool	char16regexeq();
extern bool	char16regexne();
extern bool	textregexeq();
extern bool	textregexne();
#endif /* FMGR_ADT */

extern bool int4notin();
extern bool oidnotin();

/* rule locks */
extern RuleLock		StringToRuleLock();
extern char		*RuleLockToString();
extern Datum GetAttribute();

/* rule stub records */
extern char * stubout();
extern Prs2RawStub stubin();

extern int32 byteaGetSize();
extern int32 byteaGetByte();
extern struct varlena * byteaSetByte();
extern int32 byteaGetBit();
extern struct varlena * byteaSetBit();

extern int32 userfntest();

extern char *pg_username();

extern char *array_in();
extern char *array_out();

extern char *filename_in();
extern char *filename_out();

extern char *lo_filein();
extern char *lo_fileout();

extern int2 smgrin();
extern char *smgrout();
extern bool smgreq();
extern bool smgrne();

extern int32 pqtest();
extern int32 int2inc();
extern int32 int4inc();

#endif !BuiltinsIncluded
