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
#include "tmp/c.h"

#define	EPSILON			1.0E-06

#define	FPzero(A)		(fabs(A) <= EPSILON)
#define	FPeq(A,B)		(fabs((A) - (B)) <= EPSILON)
#define	FPlt(A,B)		((B) - (A) > EPSILON)
#define	FPle(A,B)		((A) - (B) <= EPSILON)
#define	FPgt(A,B)		((A) - (B) > EPSILON)
#define	FPge(A,B)		((B) - (A) <= EPSILON)

#define	HYPOT(A, B)		sqrt((A) * (A) + (B) * (B))

/*--------------------------------------------------------------------
 *	Memory management.
 *-------------------------------------------------------------------*/

#define	PALLOC(SIZE)		palloc(SIZE)
#define	PFREE(P)		pfree((char *) (P))
#define	PALLOCTYPE(TYPE)	(TYPE *) PALLOC(sizeof(TYPE))

/*--------------------------------------------------------------------
 *	Handy things...
 *-------------------------------------------------------------------*/

#ifdef sun
extern char *sprintf();
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

/*---------------------------------------------------------------------
 *  POLYGON - Specified by an array of doubles defining the points, 
 *			  keeping the number of points and the bounding box for 
 *			  speed purposes.
 *-------------------------------------------------------------------*/
typedef struct {
	long size;
	long npts;
	BOX boundbox;
	char pts[1];
} POLYGON;


/* ----------------------
 * function prototypes -- not called by function manager
 * ----------------------
 */
double poly_max ARGS((double *coords , int ncoords ));
double poly_min ARGS((double *coords , int ncoords ));
BOX *box_fill ARGS((BOX *result , double x1 , double x2 , double y1 , double y2 ));
BOX *box_construct ARGS((double x1 , double x2 , double y1 , double y2 ));
BOX *box_copy ARGS((BOX *box ));
double *box_area ARGS((BOX *box ));
double *box_length ARGS((BOX *box ));
double *box_height ARGS((BOX *box ));
double *box_distance ARGS((BOX *box1 , BOX *box2 ));
double box_ar ARGS((BOX *box ));
double box_ln ARGS((BOX *box ));
double box_ht ARGS((BOX *box ));
double box_dt ARGS((BOX *box1 , BOX *box2 ));
BOX *box_intersect ARGS((BOX *box1 , BOX *box2 ));
LSEG *box_diagonal ARGS((BOX *box ));
LINE *line_construct_pm ARGS((POINT *pt , double m ));
LINE *line_construct_pp ARGS((POINT *pt1 , POINT *pt2 ));
long line_intersect ARGS((LINE *l1 , LINE *l2 ));
long line_parallel ARGS((LINE *l1 , LINE *l2 ));
long line_perp ARGS((LINE *l1 , LINE *l2 ));
long line_vertical ARGS((LINE *line ));
long line_horizontal ARGS((LINE *line ));
long line_eq ARGS((LINE *l1 , LINE *l2 ));
double *line_distance ARGS((LINE *l1 , LINE *l2 ));
POINT *line_interpt ARGS((LINE *l1 , LINE *l2 ));
long path_n_lt ARGS((PATH *p1 , PATH *p2 ));
long path_n_gt ARGS((PATH *p1 , PATH *p2 ));
long path_n_eq ARGS((PATH *p1 , PATH *p2 ));
long path_n_le ARGS((PATH *p1 , PATH *p2 ));
long path_n_ge ARGS((PATH *p1 , PATH *p2 ));
long path_inter ARGS((PATH *p1 , PATH *p2 ));
double *path_length ARGS((PATH *path ));
double path_ln ARGS((PATH *path ));
POINT *point_construct ARGS((double x , double y ));
POINT *point_copy ARGS((POINT *pt ));
long point_vert ARGS((POINT *pt1 , POINT *pt2 ));
long point_horiz ARGS((POINT *pt1 , POINT *pt2 ));
double *point_distance ARGS((POINT *pt1 , POINT *pt2 ));
double point_dt ARGS((POINT *pt1 , POINT *pt2 ));
double *point_slope ARGS((POINT *pt1 , POINT *pt2 ));
double point_sl ARGS((POINT *pt1 , POINT *pt2 ));
LSEG *lseg_construct ARGS((POINT *pt1 , POINT *pt2 ));
long lseg_intersect ARGS((LSEG *l1 , LSEG *l2 ));
long lseg_parallel ARGS((LSEG *l1 , LSEG *l2 ));
long lseg_perp ARGS((LSEG *l1 , LSEG *l2 ));
long lseg_vertical ARGS((LSEG *lseg ));
long lseg_horizontal ARGS((LSEG *lseg ));
long lseg_eq ARGS((LSEG *l1 , LSEG *l2 ));
double *lseg_distance ARGS((LSEG *l1 , LSEG *l2 ));
double lseg_dt ARGS((LSEG *l1 , LSEG *l2 ));
POINT *lseg_interpt ARGS((LSEG *l1 , LSEG *l2 ));
double *dist_pl ARGS((POINT *pt , LINE *line ));
double *dist_ps ARGS((POINT *pt , LSEG *lseg ));
double *dist_pb ARGS((POINT *pt , BOX *box ));
double *dist_sl ARGS((LSEG *lseg , LINE *line ));
double *dist_sb ARGS((LSEG *lseg , BOX *box ));
double *dist_lb ARGS((LINE *line , BOX *box ));
POINT *interpt_sl ARGS((LSEG *lseg , LINE *line ));
POINT *close_pl ARGS((POINT *pt , LINE *line ));
POINT *close_ps ARGS((POINT *pt , LSEG *lseg ));
POINT *close_pb ARGS((POINT *pt , BOX *box ));
POINT *close_sl ARGS((LSEG *lseg , LINE *line ));
POINT *close_sb ARGS((LSEG *lseg , BOX *box ));
POINT *close_lb ARGS((LINE *line , BOX *box ));
long on_pl ARGS((POINT *pt , LINE *line ));
long on_ps ARGS((POINT *pt , LSEG *lseg ));
long on_pb ARGS((POINT *pt , BOX *box ));
long on_ppath ARGS((POINT *pt , PATH *path ));
long on_sl ARGS((LSEG *lseg , LINE *line ));
long on_sb ARGS((LSEG *lseg , BOX *box ));
long inter_sl ARGS((LSEG *lseg , LINE *line ));
long inter_sb ARGS((LSEG *lseg , BOX *box ));
long inter_lb ARGS((LINE *line , BOX *box ));
void make_bound_box ARGS((POLYGON *poly ));
long poly_pt_count ARGS((char *s , char delim ));
#endif !GeoDeclsIncluded
