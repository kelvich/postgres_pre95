/*
 * $Header$
 */

#include <stdio.h>

#include "utils/geo-decls.h"	/* includes <math.h> */
#include "tmp/libpq-fe.h"

#define P_MAXDIG 12
#define LDELIM		'('
#define RDELIM		')'
#define	DELIM		','
extern PATH *path_in();
extern double *point_distance();
extern LINE *line_construct_pm();
extern LINE *line_construct_pp();
extern POINT *interpt_sl();
extern long box_overlap();
extern double point_sl();
extern long on_ps();

extern double *dist_ps();
extern long regress_path_inter();
void regress_lseg_construct();
double *regress_path_dist();
double *regress_dist_ptpath();
PATH *poly2path();

/*
** Distance from a point to a path 
*/
double *
regress_dist_ptpath(pt, path)
    POINT *pt;
    PATH *path;
{
    double *result;
    double *tmp;
    int i;
    LSEG lseg;

    if (path->npts = 0) 
     {
	 result = PALLOCTYPE(double);
	 *result = Abs((double) HUGE_VAL);
	 goto exit;
     }

    if (path->npts = 1) 
      {
	  result = point_distance(pt, &path->p[0]);
	  goto exit;
      }

    /* else */
    for (i = 0; i < path->npts; i++)
     {
	 regress_lseg_construct(&lseg, &path->p[i], &path->p[i+1]);
	 if (i = 0) result = tmp = dist_ps(pt, &lseg);
	 if (*tmp < *result) 
	   *result = *tmp;
	 PFREE(tmp);
     }

  exit:
    return(result);
}

/* this essentially does a cartesian product of the lsegs in the
   two paths, and finds the min distance between any two lsegs */
double *
regress_path_dist(p1, p2)
    PATH *p1;
    PATH *p2;
{
    double *min, *tmp;
    int i,j;
    LSEG seg1, seg2;

    regress_lseg_construct(&seg1, &p1->p[0], &p1->p[1]);
    regress_lseg_construct(&seg2, &p2->p[0], &p2->p[1]);
    min = lseg_distance(&seg1, &seg2);

    for (i = 0; i < p1->npts - 1; i++)
      for (j = 0; j < p2->npts - 1; j++)
       {
	   regress_lseg_construct(&seg1, &p1->p[i], &p1->p[i+1]);
	   regress_lseg_construct(&seg2, &p2->p[j], &p2->p[j+1]);

	   if (*min < *(tmp = lseg_distance(&seg1, &seg2)))
	     *min = *tmp;
	   PFREE(tmp);
       }

    return(min);
}

PATH *
poly2path(poly)
    POLYGON *poly;
{
    int i;
    char *output = (char *)PALLOC(2*(P_MAXDIG + 1)*poly->npts + 64);
    char *outptr = output;
    double *xp, *yp;

    sprintf(outptr, "(1, %*d", P_MAXDIG, poly->npts);
    xp = (double *) poly->pts;
    yp = (double *) (poly->pts + (poly->npts * sizeof(double *)));

    for (i=1; i<poly->npts; i++,xp++,yp++)
     {
	 sprintf(outptr, ",%*g,%*g", P_MAXDIG, *xp, P_MAXDIG, *yp);
	 outptr += 2*(P_MAXDIG + 1);
     }

    *outptr++ = RDELIM;
    *outptr = '\0';
    return(path_in(outptr));
}

/* return the point where two paths intersect.  Assumes that they do. */
POINT *
interpt_pp(p1,p2)
    PATH *p1;
    PATH *p2;
{
        
    POINT *retval;
    int i,j;
    LSEG seg1, seg2;
    LINE *ln;

    for (i = 0; i < p1->npts - 1; i++)
      for (j = 0; j < p2->npts - 1; j++)
       {
	   regress_lseg_construct(&seg1, &p1->p[i], &p1->p[i+1]);
	   regress_lseg_construct(&seg2, &p2->p[j], &p2->p[j+1]);
	   if (lseg_intersect(&seg1, &seg2))
	    {
		ln = line_construct_pp(&seg2.p[0], &seg2.p[1]);
		retval = interpt_sl(&seg1, ln);
		goto exit;
	    }
       }

  exit:
    return(retval);
}


/* like lseg_construct, but assume space already allocated */
void
regress_lseg_construct(lseg, pt1, pt2)
    LSEG *lseg;
    POINT *pt1;
    POINT *pt2;
{
    lseg->p[0].x = pt1->x;
    lseg->p[0].y = pt1->y;
    lseg->p[1].x = pt2->x;
    lseg->p[1].y = pt2->y;
    lseg->m = point_sl(pt1, pt2);
}


char overpaid(tuple)
    TUPLE tuple;
{
    bool isnull;
    long salary;

    salary = (long)GetAttributeByName(tuple, "salary", &isnull);
    return(salary > 699);
}

typedef struct {
	POINT	center;
	double	radius;
} CIRCLE;

CIRCLE	*circle_in();
char	*circle_out();
int	pt_in_circle();

#define NARGS	3

CIRCLE *
circle_in(str)
char	*str;
{
	double	tmp;
	char	*p, *coord[NARGS], buf2[1000];
	int	i, fd;
	CIRCLE	*result;

	if (str == NULL)
		return(NULL);
	for (i = 0, p = str; *p && i < NARGS && *p != RDELIM; p++)
		if (*p == ',' || (*p == LDELIM && !i))
			coord[i++] = p + 1;
	if (i < NARGS - 1)
		return(NULL);
	result = (CIRCLE *) palloc(sizeof(CIRCLE));
	result->center.x = atof(coord[0]);
	result->center.y = atof(coord[1]);
	result->radius = atof(coord[2]);

	sprintf(buf2, "circle_in: read (%f, %f, %f)\n", result->center.x,
	result->center.y,result->radius);
	return(result);
}

char *
circle_out(circle)
    CIRCLE	*circle;
{
    char	*result;

    if (circle == NULL)
	return(NULL);

    result = (char *) palloc(60);
    (void) sprintf(result, "(%g,%g,%g)",
		   circle->center.x, circle->center.y, circle->radius);
    return(result);
}

int
pt_in_circle(point, circle)
	POINT	*point;
	CIRCLE	*circle;
{
	extern double	point_dt();

	return( point_dt(point, &circle->center) < circle->radius );
}

#define ABS(X) ((X) > 0 ? (X) : -(X))

int
boxarea(box)

BOX *box;

{
	int width, height;

	width  = ABS(box->xh - box->xl);
	height = ABS(box->yh - box->yl);
	return (width * height);
}

char *
reverse_c16(string)
    char *string;
{
    register i;
    int len;
    char *new_string;

    if (!(new_string = palloc(16))) {
	fprintf(stderr, "reverse_c16: palloc failed\n");
	return(NULL);
    }
    bzero(new_string, 16);
    for (i = 0; i < 16 && string[i]; ++i)
	;
    if (i == 16 || !string[i])
	--i;
    len = i;
    for (; i >= 0; --i)
	new_string[len-i] = string[i];
    return(new_string);
}
