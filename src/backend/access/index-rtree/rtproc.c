/*
 *  rtproc.c -- pg_amproc entries for rtrees.
 */
#include <math.h>

#include "tmp/c.h"
#include "tmp/postgres.h"

#include "utils/log.h"
#include "utils/geo-decls.h"

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
