/*
 * rac.c --
 *	POSTGRES rule lock access code.
 */

#include "c.h"

#include "block.h"
#include "buf.h"
#include "bufmgr.h"
#include "bufpage.h"
#include "htup.h"
#include "inval.h"
#include "item.h"
#include "itemid.h"
#include "itemptr.h"
#include "log.h"
#include "page.h"
#include "palloc.h"
#include "rel.h"
#include "rlock.h"

#include "rac.h"

RcsId("$Header$");

/*
 * XXX direct structure accesses should be removed
 */
RuleLock
HeapTupleGetRuleLock(tuple, buffer)
	HeapTuple	tuple;
	Buffer		buffer;
{
	Assert(HeapTupleIsValid(tuple));

	if (!BufferIsValid(buffer)) {
		return (tuple->t_lock.l_lock);
	}

	if (!ItemPointerIsValid(&tuple->t_lock.l_ltid)) {

		return (InvalidRuleLock);
	}
{
	BlockNumber	blockNumber;
	Page		page;
	ItemId		itemId;
	Item		item;
	Item		returnItem;

	blockNumber = ItemPointerGetBlockNumber(&tuple->t_lock.l_ltid);
	buffer = RelationGetBuffer(BufferGetRelation(buffer), blockNumber,
		L_PIN);
	page = BufferSimpleGetPage(buffer);
	itemId = PageGetItemId(page,
		ItemPointerSimpleGetOffsetIndex(&tuple->t_lock.l_ltid));
	item = PageGetItem(page, itemId);
	returnItem = palloc(itemId->lp_len);	/* XXX */
	bcopy(item, returnItem, (int)itemId->lp_len);	/* XXX Clib-copy */
	BufferPut(buffer, L_UNPIN);
	return (LintCast(RuleLock, returnItem));
}
}

HeapTuple
HeapTupleSetRuleLock(tuple, buffer, lock)
	HeapTuple	tuple;
	Buffer		buffer;
	RuleLock	lock;
{
	Assert(HeapTupleIsValid(tuple));

	if (BufferIsValid(buffer)) {
		HeapTuple	palloctup();

		tuple = palloctup(tuple, buffer, InvalidRelation);
	}

	tuple->t_lock.l_lock = lock;

	return (tuple);
}

void
HeapTupleStoreRuleLock(tuple, buffer, lock)
	HeapTuple	tuple;
	Buffer		buffer;
	RuleLock	lock;
{
	Relation	relation;
	Page		page;
	BlockNumber	blockNumber;
	OffsetNumber	offsetNumber;

	Assert(HeapTupleIsValid(tuple));
	Assert(BufferIsValid(buffer));

	if (!RuleLockIsValid(lock)) {
		ItemPointerSetInvalid(&tuple->t_lock.l_ltid);
		return;
	}

	relation = BufferGetRelation(buffer);

	/* Assert(0); */
	/* XXX START HERE */
	/*
	 * need to check if sufficient space.  If so, then place it.
	 * Otherwise, allocate a new page.  Etc.  Look at util/access.c
	 * for ideas and to have amputtup and amputnew call this funciton.
	 */

	page = BufferSimpleGetPage(buffer);

	if (ItemPointerIsValid(&tuple->t_lock.l_ltid)) {

		/* XXX Is the physical removal of the lock safe? */

		if (ItemPointerGetBlockNumber(&tuple->t_lock.l_ltid) ==
				BufferGetBlockNumber(buffer)) {

			PageRemoveItem(page,
				PageGetItemId(page,
					ItemPointerSimpleGetOffsetIndex(
						&tuple->t_lock.l_ltid)));
		} else {
			;
			/*
			 * XXX for now, let the vacuum demon remove locks
			 * on other pages
			 */
		}
	}

	if (PageGetFreeSpace(page) >= psize(lock)) {

		blockNumber = BufferGetBlockNumber(buffer);
		offsetNumber = PageAddItem(page, lock, psize(lock),
			InvalidOffsetNumber, LP_USED | LP_LOCK);
	} else {
		Assert(psize(lock) < BLCKSZ);	/* XXX cannot handle this yet */

		buffer = RelationGetBuffer(relation, P_NEW, L_NEW);
		if (!BufferIsValid(buffer)) {
			elog(WARN, "HeapTupleStoreRuleLock: cannot get new buffer");
		}
		BufferSimpleInitPage(buffer);
		blockNumber = BufferGetBlockNumber(buffer);
		offsetNumber = PageAddItem(BufferSimpleGetPage(buffer),
			lock, psize(lock), InvalidOffsetNumber,
			LP_USED | LP_LOCK);
		BufferPut(buffer, L_UN | L_EX | L_WRITE);
	}
	ItemPointerSimpleSet(&tuple->t_lock.l_ltid, blockNumber, offsetNumber);

	RelationInvalidateHeapTuple(relation, tuple);
}
