/*
 * localam.c --
 *    POSTGRES heap access method "fast" code for temp or "retrieve into"
 *  relations.  NOTE:  Since these relations are not visible to anyone but
 *  the backend that is generating them, this code completely ignores the
 *  multi-user buffer pool and utilities, and is optimized for fast appends.
 *
 *  ! DO NOT USE THIS CODE FOR ANYTHING BUT TEMP OR "RETRIEVE INTO" RELATIONS !
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


typedef struct l_rellist
{
    Relation relation;
    PageHeader page_header;
    BlockNumber blocknum;
	struct l_rellist *next;
    bool active;
}
LocalRelList;

LocalRelList *head = NULL, *current = NULL, *tail = NULL;
LocalRelList *GetRelListEntry(), *AddToRelList(); 

/*
 * Public interface routines:
 */

local_heap_close(relation)

Relation relation;

{
    LocalRelList *p = GetRelListEntry(relation);
    FlushLocalPage(p);
    DeleteRelListEntry(relation);
}

local_heap_open(relation)

Relation relation;

{
    AddToRelList(relation);
}

void
local_heap_insert(relation, tup)
    Relation    relation;
    HeapTuple    tup;
{
    LocalRelList *p = GetRelListEntry(relation);

    /* ----------------
     *    increment access statistics
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


    local_doinsert(p, tup);
}

LocalRelList *
AddToRelList(relation)

Relation relation;

{
    if (tail == NULL)
    {
        Assert(head == NULL);
        head = (LocalRelList *) palloc(sizeof(LocalRelList));
        tail = head;
    }
    else
    {
        tail->next = (LocalRelList *) palloc(sizeof(LocalRelList));
        tail = tail->next;
    }

    tail->relation = relation;
    tail->page_header = (PageHeader) palloc(BLCKSZ);
    tail->blocknum = 0;
    tail->next = NULL;
    tail->active = true;
    bzero(tail->page_header, BLCKSZ);
    PageInit(tail->page_header, BLCKSZ, 0);
    current = tail;
    return(tail);
}

LocalRelList *
GetRelListEntry(relation)

Relation relation;

{
    LocalRelList *p;

    Assert(current != NULL);
    if (current->relation == relation) return(current);

    for (p = head; p != NULL && p->relation != relation; p = p->next);

    if (p == NULL) elog(WARN, "GetRelListEntry: can't find rel in list!");
    return(p);
}

/*
 * This does not do an actual list delete, but instead marks the list
 * entry as inactive and frees its page.  This is done so that all the
 * temp relations can be saved and blown away at TransactionAbort time
 * if necessary by a call to DestroyLocalRelList.
 */

DeleteRelListEntry(relation)

Relation relation;

{
    LocalRelList *p;

    Assert(head != NULL);

    for (p = head; p != NULL && p->relation != relation; p = p->next);

    Assert(p != NULL);

    p->active = false;

    /* don't waste excessive memory */

    pfree(p->page_header);
    p->page_header = NULL;

    if (current == p)
    {
        for (p = head; p->next != NULL && ! p->active; p = p->next);

        if (p->next == NULL) current = NULL;
        else current = p;
    }
}

AtAbort_LocalRels()

{
    DestroyLocalRelList();
}

AtCommit_LocalRels()

{
    DestroyLocalRelList();
}

/*
 * This routine is useful for AbortTransaction - it deletes the temp
 * relations in the list if they are still active.  They should not ever
 * be active at TransactionCommit time, although this routine needs to
 * be called at the end of a transaction no matter what happens.
 */

DestroyLocalRelList()

{
    LocalRelList *p, *q;
    for (p = head; p != NULL; p = q)
    {
        if (p->active)
        {
            FileUnlink(p->relation->rd_fd);
            pfree(p->page_header);
        }
        q = p->next;
        pfree(p);
    }
    head = current = tail = NULL;
}

local_doinsert(rel_info, tuple)

LocalRelList *rel_info;
HeapTuple tuple;

{
    Size len = LONGALIGN(tuple->t_len);
    OffsetIndex offsetIndex;
    ItemId      itemId;
    HeapTuple   page_tuple;

    if (len > PageGetFreeSpace(rel_info->page_header))
    {
        FlushLocalPage(rel_info);
        rel_info->blocknum++;
        bzero(rel_info->page_header, BLCKSZ);
        PageInit(rel_info->page_header, BLCKSZ, 0);
    }

    offsetIndex = PageAddItem((Page)rel_info->page_header, (Item)tuple,
                              tuple->t_len, InvalidOffsetNumber, LP_USED) - 1;

    itemId = PageGetItemId((Page)rel_info->page_header, offsetIndex);

    page_tuple = (HeapTuple) PageGetItem((Page) rel_info->page_header, itemId);

    /*
     * As this code is being used for "retrieve into" or tmp relations, 
     * the RuleLock can be set to invalid as no tuple rule can exist on
     * such relations.
     */

    page_tuple->t_locktype = DISK_RULE_LOCK;
    ItemPointerSetInvalid(&page_tuple->t_lock.l_ltid);

    ItemPointerSimpleSet(&page_tuple->t_ctid,
                         rel_info->blocknum,
                         1 + offsetIndex);
}

FlushLocalPage(rel_info)

LocalRelList *rel_info;

{
    unsigned long offset;
    int nbytes;

    offset = rel_info->blocknum * BLCKSZ;

    FileSeek(rel_info->relation->rd_fd, offset, L_SET);

    nbytes = FileWrite(rel_info->relation->rd_fd,
                       (char *) rel_info->page_header,
                       BLCKSZ);

    Assert(nbytes == BLCKSZ);
}
