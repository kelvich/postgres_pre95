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

#include "c.h"
#include "postgres.h"
#include "oid.h"
#include "tim.h"
#include "xid.h"

#ifdef FMGR_ADT
#include "geo-decls.h"
#include "kwlist.h"
#endif /* FMGR_ADT */


/*
 *	Defined in adt/
 */
extern int32		boolin();
extern char 		*boolout();
extern int32		charin();
extern char		*charout();
extern int32		chareq();
extern char		*char16in();
extern char		*char16out();
extern int32		char16eq();
extern int32		char16ne();
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

/*
 *	B-tree code.
 *	Defined in btree/
 */
extern char		*btreeinsert();
extern char		*btreedelete();
extern char		*btreegetnext();
extern void		btreebuild();

/*
 *	Functional B-tree code.
 *	Defined in ftree/
 */
#ifdef FMGR_ADT
extern char		*ftreeinsert();
extern char		*ftreedelete();
extern char		*ftreegetnext();
extern void		ftreebuild();
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
extern float64	areasel();
extern float64	areajoinsel();
extern long	pointdist();

extern long	textregexeq();
extern long	char16regexeq();
#endif /* FMGR_ADT */

#endif !BuiltinsIncluded
