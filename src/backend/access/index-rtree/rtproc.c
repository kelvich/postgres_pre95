/*
 *  rtproc.c -- pg_amproc entries for rtrees.
 */
#include <math.h>

#include "tmp/c.h"
#include "tmp/postgres.h"

#include "utils/log.h"
#include "utils/geo-decls.h"

RcsId("$Header$");

BOX *
rt_box_union(a, b)
	BOX *a;
	BOX *b;
{
	BOX *n;

	if ((n = (BOX *) palloc(sizeof (*n))) == (BOX *) NULL)
		elog(WARN, "Cannot allocate box for union");

	n->xh = MAX(a->xh, b->xh);
	n->yh = MAX(a->yh, b->yh);
	n->xl = MIN(a->xl, b->xl);
	n->yl = MIN(a->yl, b->yl);

	return (n);
}

BOX *
rt_box_inter(a, b)
	BOX *a;
	BOX *b;
{
	BOX *n;

	if ((n = (BOX *) palloc(sizeof (*n))) == (BOX *) NULL)
		elog(WARN, "Cannot allocate box for union");

	n->xh = MIN(a->xh, b->xh);
	n->yh = MIN(a->yh, b->yh);
	n->xl = MAX(a->xl, b->xl);
	n->yl = MAX(a->yl, b->yl);

	if (n->xh < n->xl || n->yh < n->yl) {
		pfree (n);
		return ((BOX *) NULL);
	}

	return (n);
}

float *
rt_box_size(a)
	BOX *a;
{
	float *size;

	size = (float *) palloc(sizeof(float));
	if (a == (BOX *) NULL || a->xh <= a->xl || a->yh <= a->yl)
		*size = 0;
	else
	    *size = (float) (fabs(a->xh - a->xl) * fabs(a->yh - a->yl));

	return (size);
}

/*
 *  rt_bigbox_size() -- Compute a size for big boxes.
 *
 *	In an earlier release of the system, this routine did something
 *	different from rt_box_size.  We now use floats, rather than ints,
 *	as the return type for the size routine, so we no longer need to
 *	have a special return type for big boxes.
 */

float *
rt_bigbox_size(a)
	BOX *a;
{
	return (rt_box_size(a));
}

POLYGON *
rt_poly_union(a, b)
	POLYGON *a;
	POLYGON *b;
{
	POLYGON *p;

	p = (POLYGON *)PALLOCTYPE(POLYGON);

	if (!PointerIsValid(p))
		elog(WARN, "Cannot allocate polygon for union");

	p->size = sizeof(POLYGON);
	p->boundbox.xh = MAX(a->boundbox.xh, b->boundbox.xh);
	p->boundbox.yh = MAX(a->boundbox.yh, b->boundbox.yh);
	p->boundbox.xl = MIN(a->boundbox.xl, b->boundbox.xl);
	p->boundbox.yl = MIN(a->boundbox.yl, b->boundbox.yl);
	return p;
}

float *
rt_poly_size(a)
	POLYGON *a;
{
	float *size;
	double xdim, ydim;

	size = (float *) palloc(sizeof(float));
	if (a == (POLYGON *) NULL || 
		a->boundbox.xh <= a->boundbox.xl || 
		a->boundbox.yh <= a->boundbox.yl)
		*size = 0;
	else {
		xdim = fabs(a->boundbox.xh - a->boundbox.xl);
		ydim = fabs(a->boundbox.yh - a->boundbox.yl);

		*size = (float) (xdim * ydim);
	}

	return (size);
}

POLYGON *
rt_poly_inter(a, b)
	POLYGON *a;
	POLYGON *b;
{
	POLYGON *p;

	p = (POLYGON *) PALLOCTYPE(POLYGON);

	if (!PointerIsValid(p))
		elog(WARN, "Cannot allocate polygon for intersection");

	p->boundbox.xh = MIN(a->boundbox.xh, b->boundbox.xh);
	p->boundbox.yh = MIN(a->boundbox.yh, b->boundbox.yh);
	p->boundbox.xl = MAX(a->boundbox.xl, b->boundbox.xl);
	p->boundbox.yl = MAX(a->boundbox.yl, b->boundbox.yl);

	if (p->boundbox.xh < p->boundbox.xl || p->boundbox.yh < p->boundbox.yl)
	{
		pfree (p);
		return ((POLYGON *) NULL);
	}

	return (p);
}
