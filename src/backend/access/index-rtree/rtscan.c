/*
 *  rtscan.c -- routines to manage scans on index relations
 */
#include "tmp/c.h"
#include "tmp/postgres.h"

#include "storage/bufmgr.h"
#include "storage/bufpage.h"
#include "storage/page.h"

#include "utils/log.h"
#include "utils/rel.h"

#include "access/heapam.h"
#include "access/genam.h"
#include "access/ftup.h"
#include "access/rtree.h"

RcsId("$Header$");

IndexScanDesc
rtbeginscan(r, fromEnd, nkeys, key)
    Relation r;
    Boolean fromEnd;
    uint16 nkeys;
    ScanKey key;
{
    IndexScanDesc s;
    RTreeScanOpaque so;

    RelationSetLockForRead(r);
    return (RelationGetIndexScan(r, fromEnd, nkeys, key));
}

void
rtrescan(s, fromEnd, key)
    IndexScanDesc s;
    Boolean fromEnd;
    ScanKey key;
{
    RTreeScanOpaque p;
    int nbytes;

    if (!IndexScanIsValid(s)) {
	elog(WARN, "rtrescan: invalid scan.");
	return;
    }

    /*
     *  Clear all the pointers.
     */

    ItemPointerSetInvalid(&s->previousItemData);
    ItemPointerSetInvalid(&s->currentItemData);
    ItemPointerSetInvalid(&s->nextItemData);
    ItemPointerSetInvalid(&s->previousMarkData);
    ItemPointerSetInvalid(&s->currentMarkData);
    ItemPointerSetInvalid(&s->nextMarkData);

    /*
     *  Set flags.
     */
    if (RelationGetNumberOfBlocks(s->relation) == 0) {
	s->flags = ScanUnmarked;
    } else if (fromEnd) {
	s->flags = ScanUnmarked | ScanUncheckedPrevious;
    } else {
	s->flags = ScanUnmarked | ScanUncheckedNext;
    }

    s->scanFromEnd = fromEnd;

    if (s->numberOfKeys > 0) {
	bcopy( (Pointer)&key->data[0],			/* from */
	       (Pointer)&s->keyData.data[0],		/* to */
	       s->numberOfKeys * sizeof (key->data[0])	/* nbytes */
	);
    }

    p = (RTreeScanOpaque) s->opaque;
    if (p != (RTreeScanOpaque) NULL) {
	freestack(p->s_stack);
	freestack(p->s_markstk);
	p->s_stack = p->s_markstk = (RTSTACK *) NULL;
    } else {
	int i;

	/* initialize opaque data */
	nbytes = sizeof(RTreeScanOpaqueData);
	if (s->numberOfKeys > 0) {
	    nbytes += (s->numberOfKeys - 1)
		      * sizeof(ScanKeyEntryData);
	}

	s->opaque = (Pointer) palloc(nbytes);
	p = (RTreeScanOpaque) s->opaque;
	p->s_stack = p->s_markstk = (RTSTACK *) NULL;
	p->s_internalNKey = s->numberOfKeys;
	if (s->numberOfKeys > 0) {
	    nbytes = s->numberOfKeys * sizeof(ScanKeyEntryData);
	    bcopy(&(s->keyData.data[0]), &(p->s_internalKey.data[0]), nbytes);

	    for (i = 0; i < s->numberOfKeys; i++) {
		p->s_internalKey.data[i].procedure =
		   RTMapOperator(s->relation,
				 s->keyData.data[i].attributeNumber,
				 s->keyData.data[i].procedure);
	    }
	}
    }
}

void
rtmarkpos(s)
    IndexScanDesc s;
{
    RTreeScanOpaque p;
    RTSTACK *o, *n, *tmp;

    s->currentMarkData = s->currentItemData;
    p = (RTreeScanOpaque) s->opaque;

    o = (RTSTACK *) NULL;
    n = p->s_stack;

    /* copy the parent stack from the current item data */
    while (n != (RTSTACK *) NULL) {
	tmp = (RTSTACK *) palloc(sizeof(RTSTACK));
	tmp->rts_child = n->rts_child;
	tmp->rts_blk = n->rts_blk;
	tmp->rts_parent = o;
	o = tmp;
	n = n->rts_parent;
    }

    freestack(p->s_markstk);
    p->s_markstk = o;
}

void
rtrestrpos(s)
    IndexScanDesc s;
{
    RTreeScanOpaque p;
    RTSTACK *o, *n, *tmp;

    s->currentItemData = s->currentMarkData;
    p = (RTreeScanOpaque) s->opaque;

    o = (RTSTACK *) NULL;
    n = p->s_markstk;

    /* copy the parent stack from the current item data */
    while (n != (RTSTACK *) NULL) {
	tmp = (RTSTACK *) palloc(sizeof(RTSTACK));
	tmp->rts_child = n->rts_child;
	tmp->rts_blk = n->rts_blk;
	tmp->rts_parent = o;
	o = tmp;
	n = n->rts_parent;
    }

    freestack(p->s_stack);
    p->s_stack = o;
}

void
rtendscan(s)
    IndexScanDesc s;
{
    RTreeScanOpaque p;

    p = (RTreeScanOpaque) s->opaque;

    if (p != (RTreeScanOpaque) NULL) {
	freestack(p->s_stack);
	freestack(p->s_markstk);
    }

    /* XXX don't unset read lock -- two-phase locking */
}
