/*
 * rac.c --
 *	POSTGRES rule lock access code.
 */

#include "tmp/c.h"

RcsId("$Header$");

#include "access/htup.h"

#include "rules/rac.h"
#include "rules/prs2locks.h"
#include "rules/prs2.h"

#include "storage/block.h"
#include "storage/buf.h"
#include "storage/bufmgr.h"
#include "storage/bufpage.h"
#include "storage/item.h"
#include "storage/itemid.h"
#include "storage/itemptr.h"
#include "storage/page.h"

#include "utils/inval.h"
#include "utils/palloc.h"
#include "utils/rel.h"
#include "utils/log.h"

/*
 * XXX direct structure accesses should be removed
 */

/*-------------------------------------------------------------------
 * HeapTupleFreeRuleLock
 *
 * Frees a lock, but only if it is a main memory lock.
 *
 */
void
HeapTupleFreeRuleLock(tuple)
HeapTuple tuple;
{
        if (tuple->t_locktype == MEM_RULE_LOCK) {
	    prs2FreeLocks(tuple->t_lock.l_lock);
	}
}

/*-------------------------------------------------------------------
 *
 * HeapTupleGetRuleLock
 *
 * A rule lock can be either an ItemPointer to the actual data in a
 * disk page or a RuleLock, i.e. a pointer to a main memory structure.
 * This routine always return the main memory representation (if necessary
 * it makes the conversion)
 *
 * NOTE: When we save the tuple on disk, if the rule lock is empty
 * (i.e. a "(numOfLocks: 0)"::lock, which must not be confused with
 * an 'InvalidRuleLock') then we store an InvalidItemPointer.
 * So, if the disk representation of the tuple's lock is such an
 * InvalidItemPointer we have to create & return an empty lock.
 */
RuleLock
HeapTupleGetRuleLock(tuple, buffer)
	HeapTuple	tuple;
	Buffer		buffer;
{
	Assert(HeapTupleIsValid(tuple));

	/*
	 * sanity check
	 */
        if (tuple->t_locktype != MEM_RULE_LOCK &&
            tuple->t_locktype != DISK_RULE_LOCK) {
            elog(WARN,"HeapTupleGetRuleLock: locktype = '%c',(%d)\n",
                tuple->t_locktype, tuple->t_locktype);
        }

	/*
	 * if this is a main memory lock, then return it.
	 */
        if (tuple->t_locktype == MEM_RULE_LOCK) {
            return(tuple->t_lock.l_lock);
        }

	/*
	 * no, it is a disk lock. Check if it the item pointer is
	 * invalid and if yes, return an empty lock.
	 */
	if (!ItemPointerIsValid(&tuple->t_lock.l_ltid)) {
		return (prs2MakeLocks());
	}

	/*
	 * If this is a disk lock, then we *must* pass a valid
	 * buffer...
	 * Otherwise ... booooooom!
	 */
        if (!BufferIsValid(buffer)) {
            elog(WARN,"HeapTupleGetRuleLock: Invalid buffer");
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

/*------------------------------------------------------------
 *
 * HeapTupleSetRuleLock
 *
 * Set the rule lock of a tuple.
 *
 * NOTE: the old lock of the tuple is pfreed (if it is a main
 *	memory pointer of course... If it is an item in a disk page
 *	it is left intact)
 *
 * NOTE: No copies of the tuple or the lock are made any more (this
 * routine used to -sometimes- make a copy of the tuple)
 */
void
HeapTupleSetRuleLock(tuple, buffer, lock)
	HeapTuple	tuple;
	Buffer		buffer;
	RuleLock	lock;
{
	RuleLock oldLock;
	Assert(HeapTupleIsValid(tuple));

	/*
	 * sanity check...
	 */
        if (tuple->t_locktype != MEM_RULE_LOCK &&
            tuple->t_locktype != DISK_RULE_LOCK) {
            elog(WARN,"HeapTupleGetRuleLock: locktype = '%c',(%d)\n",
                tuple->t_locktype, tuple->t_locktype);
        }

	HeapTupleFreeRuleLock(tuple);

        tuple->t_locktype = MEM_RULE_LOCK;
	tuple->t_lock.l_lock = lock;

}

/*------------------------------------------------------------
 *
 * HeapTupleStoreRuleLock
 *
 * Store a rule lock in a page.
 * The 'tuple' has a lock which can be either a "disk" or "main memory"
 * rule lock. In the first case we don't need to do anything because
 * the lock is already there! In the second case, add the lock data
 * somewhere in the disk page & update the tuple->t_lock stuff...
 *
 * NOTE: When we save the tuple on disk, if the rule lock is empty
 * (i.e. a "(numOfLocks: 0)"::lock, which must not be confused with
 * an 'InvalidRuleLock') then we store an InvalidItemPointer.
 */
void
HeapTupleStoreRuleLock(tuple, buffer)
	HeapTuple	tuple;
	Buffer		buffer;
{
	Relation	relation;
	Page		page;
	BlockNumber	blockNumber;
	OffsetNumber	offsetNumber;
	RuleLock	lock;
	Size		lockSize;

	Assert(HeapTupleIsValid(tuple));
	Assert(BufferIsValid(buffer));

	/*
	 * If it is a disk lock, then there is nothing to do...
	 */
	if (tuple->t_locktype == DISK_RULE_LOCK) {
	    return;
	}

	/*
	 * sanity check
	 */
        if (tuple->t_locktype != MEM_RULE_LOCK) {
            elog(WARN,"HeapTupleGetRuleLock: locktype = '%c',(%d)\n",
                tuple->t_locktype, tuple->t_locktype);
        }

	lock = tuple->t_lock.l_lock;

	/*
	 * if it is an empty lock, treat the trivial case..
	 */
        if (prs2RuleLockIsEmpty(lock)) {
		/*
		 * change it to an empty  disk lock 
		 */
                tuple->t_locktype = DISK_RULE_LOCK;
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

	lockSize = prs2LockSize(prs2GetNumberOfLocks(lock));

	if (PageGetFreeSpace(page) >= lockSize) {

		blockNumber = BufferGetBlockNumber(buffer);
		offsetNumber = PageAddItem(page, lock, lockSize,
			InvalidOffsetNumber, LP_USED | LP_LOCK);
	} else {
		Assert(lockSize < BLCKSZ);	/* XXX cannot handle this yet */

		buffer = RelationGetBuffer(relation, P_NEW, L_NEW);
#ifndef NO_BUFFERISVALID
		if (!BufferIsValid(buffer)) {
			elog(WARN, "HeapTupleStoreRuleLock: cannot get new buffer");
		}
#endif
		BufferSimpleInitPage(buffer);
		blockNumber = BufferGetBlockNumber(buffer);
		offsetNumber = PageAddItem(BufferSimpleGetPage(buffer),
			lock, lockSize, InvalidOffsetNumber,
			LP_USED | LP_LOCK);
		BufferPut(buffer, L_UN | L_EX | L_WRITE);
	}
	tuple->t_locktype = DISK_RULE_LOCK;
	ItemPointerSimpleSet(&tuple->t_lock.l_ltid, blockNumber, offsetNumber);

	/*
	 * And now the tricky & funny part... (not submitted to
	 * Rec.puzzles yet...)
	 *
	 * Shall we free the old memory locks or not ????
	 * "I say yes! It is safe to pfree them..." (Spyros said, just before
	 * the lights went out...)
	 *
	 * ---
	 * The lights went out, so DO NOT FREE the locks! I think the
	 * problem is that when 'RelationPutHeapTuple' calls this routine
	 * it passes to it an item pointer, i.e. a "disk" tuple, and
	 * not the main memory tuple. So finally when the executor does
	 * its cleanup, it ends up freeing this lock twice!
	 */
	/* prs2FreeLocks(lock); */

	/*
	 * And our last question for $200!
	 * the following call to "RelationInvalidateHeapTuple()"
	 * invalidates the cache...  Do we really need it ???
	 * 
	 * The correct answer is NO! "HeapTupleStoreRuleLock()"
	 * in only called by "RelationPutHeapTuple()"
	 * and "RelationPutLongHeapTuple()", which are called
	 * by "amreplace()", "aminsert()" and company, which
	 * invalidate the cache anyway...
	 */

	/* RelationInvalidateHeapTuple(relation, tuple); */

}


#ifdef OBSOLETE_BEYOND_BELIEF
/*=====================================================================
 * HeapStoreRuleLock used to be a little bit different:
 * It was *replacing* the lock of a tuple with a new one,
 * so it was first trying to free the disk space occupied by the first
 * lock. This probably screws time range queries involving rules
 * though..
 *
 * Anyway, there was some obscure code for "disk" rule lock removal
 * and as probably its writer (M. Hirohama???) is far away, and
 * nobody ever will want to duplicate his effort I decided to keep this
 * piece of code just in case we might need it again (God forbids!)
 *
 * sp.
 *=====================================================================
 */

        if (tuple->t_locktype == DISK_RULE_LOCK &&
            ItemPointerIsValid(&tuple->t_lock.l_ltid)) {
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
#endif OBSOLETE_BEYOND_BELIEF
