/*
 *  btscan.c -- manage scans on btrees.
 *
 *	Because we can be doing an index scan on a relation while we update
 *	it, we need to avoid missing data that moves around in the index.
 *	The routines and global variables in this file guarantee that all
 *	scans in the local address space stay correctly positioned.  This
 *	is all we need to worry about, since write locking guarantees that
 *	no one else will be on the same page at the same time as we are.
 *
 *	The scheme is to manage a list of active scans in the current backend.
 *	Whenever we add or remove records from an index, or whenever we
 *	split a leaf page, we check the list of active scans to see if any
 *	has been affected.  A scan is affected only if it is on the same
 *	relation, and the same page, as the update.
 */

#include "tmp/c.h"
#include "tmp/postgres.h"

#include "storage/bufmgr.h"
#include "storage/bufpage.h"
#include "storage/page.h"

#include "utils/log.h"
#include "utils/rel.h"
#include "utils/excid.h"

#include "access/heapam.h"
#include "access/genam.h"
#include "access/sdir.h"
#include "access/nbtree.h"

RcsId("$Header$");

typedef struct BTScanListData {
	IndexScanDesc		btsl_scan;
	struct BTScanListData	*btsl_next;
} BTScanListData;

typedef BTScanListData	*BTScanList;

static BTScanList	BTScans = (BTScanList) NULL;

/*
 *  _nobt_regscan() -- register a new scan.
 */

void
_nobt_regscan(scan)
    IndexScanDesc scan;
{
    BTScanList new_el;

    new_el = (BTScanList) palloc(sizeof(BTScanListData));
    new_el->btsl_scan = scan;
    new_el->btsl_next = BTScans;
    BTScans = new_el;
}

/*
 *  _nobt_dropscan() -- drop a scan from the scan list
 */

void
_nobt_dropscan(scan)
    IndexScanDesc scan;
{
    BTScanList chk, last;

    last = (BTScanList) NULL;
    for (chk = BTScans;
	 chk != (BTScanList) NULL && chk->btsl_scan != scan;
	 chk = chk->btsl_next) {
	last = chk;
    }

    if (chk == (BTScanList) NULL)
	elog(WARN, "btree scan list trashed; can't find 0x%lx", scan);

    if (last == (BTScanList) NULL)
	BTScans = chk->btsl_next;
    else
	last->btsl_next = chk->btsl_next;

    pfree (chk);
}

void
_nobt_adjscans(rel, tid)
    Relation rel;
    ItemPointer tid;
{
    BTScanList l;
    ObjectId relid, chkrelid;

    relid = rel->rd_id;
    for (l = BTScans; l != (BTScanList) NULL; l = l->btsl_next) {
	if (relid == l->btsl_scan->relation->rd_id)
	    _nobt_scandel(l->btsl_scan, ItemPointerGetBlockNumber(tid),
			ItemPointerGetOffsetNumber(tid, 0));
    }
}

void
_nobt_scandel(scan, blkno, offno)
    IndexScanDesc scan;
    BlockNumber blkno;
    OffsetNumber offno;
{
    ItemPointer current;
    Buffer buf;
    NOBTScanOpaque so;

    if (!_nobt_scantouched(scan, blkno, offno))
	return;

    so = (NOBTScanOpaque) scan->opaque;
    buf = so->nobtso_curbuf;

    current = &(scan->currentItemData);
    if (ItemPointerIsValid(current)
	&& ItemPointerGetBlockNumber(current) == blkno
	&& ItemPointerGetOffsetNumber(current, 0) >= offno) {
	_nobt_step(scan, &buf, BackwardScanDirection);
	so->nobtso_curbuf = buf;
    }

    current = &(scan->currentMarkData);
    if (ItemPointerIsValid(current)
	&& ItemPointerGetBlockNumber(current) == blkno
	&& ItemPointerGetOffsetNumber(current, 0) >= offno) {
	ItemPointerData tmp;
	tmp = *current;
	*current = scan->currentItemData;
	scan->currentItemData = tmp;
	_nobt_step(scan, &buf, BackwardScanDirection);
	so->nobtso_mrkbuf = buf;
	tmp = *current;
	*current = scan->currentItemData;
	scan->currentItemData = tmp;
    }
}

bool
_nobt_scantouched(scan, blkno, offno)
    IndexScanDesc scan;
    BlockNumber blkno;
    OffsetNumber offno;
{
    ItemPointer current;

    current = &(scan->currentItemData);
    if (ItemPointerIsValid(current)
	&& ItemPointerGetBlockNumber(current) == blkno
	&& ItemPointerGetOffsetNumber(current, 0) >= offno)
	return (true);

    current = &(scan->currentMarkData);
    if (ItemPointerIsValid(current)
	&& ItemPointerGetBlockNumber(current) == blkno
	&& ItemPointerGetOffsetNumber(current, 0) >= offno)
	return (true);

    return (false);
}
