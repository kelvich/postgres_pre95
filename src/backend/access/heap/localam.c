/*
 * localam.c --
 *	POSTGRES heap access method "fast" code for temp or "retrieve into"
 *  relations.  NOTE:  Since these relations are not visible to anyone but
 *  the backend that is generating them, this code completely ignores the
 *  multi-user buffer pool and utilities, and is optimized for fast appends.
 *
 *  ! DO NOT USE THIS CODE FOR ANYTHING BUT LOCAL RELATIONS !
 */

#include <sys/file.h>

#include "tmp/c.h"

RcsId("$Header$");

#include "access/heapam.h"
#include "access/hio.h"
#include "access/htup.h"

#include "storage/block.h"
#include "storage/buf.h"
#include "storage/bufmgr.h"
#include "storage/bufpage.h"
#include "storage/itemid.h"
#include "storage/itemptr.h"

#include "utils/memutils.h"
#include "utils/log.h"
#include "utils/rel.h"


void
heap_local_insert(relation, tup)
    Relation	relation;
    HeapTuple	tup;
{
    /* ----------------
     *	increment access statistics
     * ----------------
     */
    IncrHeapAccessStat(local_insert);
    IncrHeapAccessStat(global_insert);

    if (!ObjectIdIsValid(tup->t_oid)) {
	tup->t_oid = newoid();
	LastOidProcessed = tup->t_oid;
    }

    TransactionIdStore(GetCurrentTransactionId(), (Pointer)tup->t_xmin);
    tup->t_cmin = GetCurrentCommandId();
    PointerStoreInvalidTransactionId((Pointer)tup->t_xmax);
    tup->t_tmin = InvalidTime;
    tup->t_tmax = InvalidTime;

    local_doinsert(relation, tup);
}

/*
 * Cache of "local" stuff.  At this time, this code will only work for
 * a single relation at a time.
 */
 
PageHeader  page_header = NULL;
int local_blocknum = 0;
bool initialized = false;

/* ----------------
 *  heap_local_end - ends a set of insertions; necessary so that buffers
 *  are flushed to disk.
 * ----------------
 */

heap_local_end(relation)

Relation relation;

{
    FlushLocalPage(relation, page_header, local_blocknum);
	initialized = false;
}

heap_local_start(relation)

Relation relation;

{
    local_blocknum = 0;

    if (page_header == NULL)
    {
        page_header = (PageHeader) malloc(BLCKSZ);
    }
	bzero(page_header, BLCKSZ);
    PageInit(page_header, BLCKSZ, 0);
	initialized = true;
}

local_doinsert(relation, tuple)

Relation relation;
HeapTuple tuple;

{
    Size len = LONGALIGN(tuple->t_len);
    OffsetIndex offsetIndex;
    ItemId      itemId;
    HeapTuple   page_tuple;

	if (!initialized) heap_local_start(relation);

    if (len > PageGetFreeSpace(page_header))
    {
        FlushLocalPage(relation, page_header, local_blocknum);
        local_blocknum++;
		bzero(page_header, BLCKSZ);
        PageInit(page_header, BLCKSZ, 0);
    }

    offsetIndex = PageAddItem((Page)page_header, (Item)tuple,
                              tuple->t_len, InvalidOffsetNumber, LP_USED) - 1;

    itemId = PageGetItemId((Page)page_header, offsetIndex);

    page_tuple = (HeapTuple) PageGetItem((Page) page_header, itemId);

    /*
     * As this code is being used for "retrieve into" or tmp relations, 
     * the RuleLock can be set to invalid as no tuple rule can exist on
     * such relations.
     */

    page_tuple->t_locktype = DISK_RULE_LOCK;
    ItemPointerSetInvalid(&page_tuple->t_lock.l_ltid);

    ItemPointerSimpleSet(&page_tuple->t_ctid, local_blocknum, 1 + offsetIndex);
}

FlushLocalPage(relation, page, block)

Relation relation;
PageHeader page;
BlockNumber block;

{
    unsigned long offset;
    int nbytes;

	offset = block ? block * BLCKSZ : 0;

    FileSeek(relation->rd_fd, offset, L_SET);
    nbytes = FileWrite(relation->rd_fd, (char *) page, BLCKSZ);
    Assert(nbytes == BLCKSZ);
}
