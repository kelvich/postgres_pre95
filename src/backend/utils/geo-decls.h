/***********************************************************************
**
**	geo-decls.h
**
**	Declarations for various 2D constructs.
**
**	These routines do *not* use the float types from adt/.
**
**	XXX These routines were not written by a numerical analyst.
**
** Identification:
**	$Header$
**
***********************************************************************/

#ifndef	GeoDeclsIncluded
#define	GeoDeclsIncluded

#ifndef FmgrIncluded
/*--------------------------------------------------------------------
 *	Useful floating point utilities and constants.
 *-------------------------------------------------------------------*/

#include <math.h>

#define	EPSILON			1.0E-06

#define	FPzero(A)		(fabs(A) <= EPSILON)
#define	FPeq(A,B)		(fabs((A) - (B)) <= EPSILON)
#define	FPlt(A,B)		((B) - (A) > EPSILON)
#define	FPle(A,B)		((A) - (B) <= EPSILON)
#define	FPgt(A,B)		((A) - (B) > EPSILON)
#define	FPge(A,B)		((B) - (A) <= EPSILON)

#ifndef MAX
#define	MAX(A,B)		((A) > (B) ? (A) : (B))
#define	MIN(A,B)		((A) < (B) ? (A) : (B))
#endif !MAX

#define	HYPOT(A, B)		sqrt((A) * (A) + (B) * (B))

/*--------------------------------------------------------------------
 *	Memory management.
 *-------------------------------------------------------------------*/

/*#define	palloc	malloc	/* if standalone */

extern char			*palloc();

#define	PALLOC(SIZE)		palloc(SIZE)
#define	PFREE(P)		pfree((char *) (P))
#define	PALLOCTYPE(TYPE)	(TYPE *) PALLOC(sizeof(TYPE))

/*--------------------------------------------------------------------
 *	Handy things...
 *-------------------------------------------------------------------*/

#ifdef sun
extern char			*sprintf();	/* XXX */
#endif sun

#endif !FmgrIncluded

/*---------------------------------------------------------------------
 *	POINT	-	(x,y)
 *-------------------------------------------------------------------*/
typedef struct {
	double	x, y;
} POINT;


/*---------------------------------------------------------------------
 *	LSEG	- 	A straight line, specified by endpoints.
 *-------------------------------------------------------------------*/
typedef	struct {
	POINT	p[2];

	double	m;	/* precomputed to save time, not in tuple */
} LSEG;


/*---------------------------------------------------------------------
 *	PATH	- 	Specified by vertex points.
 *-------------------------------------------------------------------*/
typedef	struct {
	short	closed;	/* is this a closed polygon? */
	long	npts;
	POINT	p[1];	/* variable length array of POINTs */
} PATH;


/*---------------------------------------------------------------------
 *	LINE	-	Specified by its general equation (Ax+By+C=0).
 *			If there is a y-intercept, it is C, which
 *			 incidentally gives a freebie point on the line
 *			 (if B=0, then C is the x-intercept).
 *			Slope m is precalculated to save time; if
 *			 the line is not vertical, m == A.
 *-------------------------------------------------------------------*/
typedef struct {
	double	A, B, C;
	double	m;
} LINE;


/*---------------------------------------------------------------------
 *	BOX	- 	Specified by two corner points, which are
 *			 sorted to save calculation time later.
 *-------------------------------------------------------------------*/
typedef struct {
	double	xh, yh, xl, yl;		/* high and low coords */
} BOX;

#endif !GeoDeclsIncluded
