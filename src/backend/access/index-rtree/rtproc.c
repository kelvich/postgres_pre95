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

int
rt_box_size(a)
	BOX *a;
{
	int size;

	if (a == (BOX *) NULL || a->xh <= a->xl || a->yh <= a->yl)
		return (0);

	size = (int) (fabs(a->xh - a->xl) * fabs(a->yh - a->yl));

	return (size);
}

/*
 *  rt_bigbox_size() -- Compute a scaled size for big boxes.
 *
 *	If the field on which rectangles lie is large, or if the rectangles
 *	themselves are large, then size computations on them can cause
 *	arithmetic overflows.  On some architectures (eg, Sequent), the
 *	overflow causes a core dump when we try to cast the double to an
 *	int.  Rather than do non-portable arithmetic exception handling,
 *	we declare an operator class "bigbox_ops" that's exactly like
 *	"box_ops", except that sizes are scaled by four orders of decimal
 *	magnitude.
 */

int
rt_bigbox_size(a)
	BOX *a;
{
	int size;
	double xdim, ydim;

	if (a == (BOX *) NULL || a->xh <= a->xl || a->yh <= a->yl)
		return (0);

	/* scale boxes by 100 in both dimensions */
	xdim = fabs(a->xh - a->xl) / 100.0;
	ydim = fabs(a->yh - a->yl) / 100.0;

	size = (int) (xdim * ydim);

	return (size);
}
