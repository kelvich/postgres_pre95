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
#include "access/xact.h"

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

    Page read_page, write_page;
    BlockNumber read_blocknum, write_blocknum;

    OffsetIndex offset_index;
    bool active;
    struct l_rellist *next;
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
    if (p->write_page != NULL) WriteLocalPage(p);
    DeleteRelListEntry(relation);
}

local_heap_destroy(relation)

Relation relation;

{
    LocalRelList *p = GetRelListEntry(relation);
    FileUnlink(p->relation->rd_fd);
}

local_heap_open(relation)

Relation relation;

{
    MemoryContext    oldCxt;
    MemoryContext    portalContext;
    Portal	     portal;

    /* ----------------
     *	get the blank portal and its memory context
     * ----------------
     */
    portal = GetPortalByName(NULL);
    portalContext = (MemoryContext) PortalGetHeapMemory(portal);

    oldCxt = MemoryContextSwitchTo(portalContext);
    AddToRelList(relation);
    (void) MemoryContextSwitchTo(oldCxt);
}

void
local_heap_insert(relation, tup)
    Relation    relation;
    HeapTuple    tup;
{
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

    local_doinsert(relation, tup);
}

/*
 * These routines are called by the transaction manager.  They are useful
 * in cleaning up dead temp relations and such from an AbortTransaction
 * or an elog(WARN).  They do not completely solve the problem of dead
 * tmp relations, however; we have to do something about those left over from
 * a crashed backend.
 */

AtAbort_LocalRels()

{
    DestroyLocalRelList();
}

AtCommit_LocalRels()

{
    DestroyLocalRelList();
}

/*
 * Private routines
 */

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
    tail->read_page = tail->write_page = NULL;
    tail->read_blocknum = tail->write_blocknum = 0;
    tail->offset_index = 0;
    tail->next = NULL;
    tail->active = true;
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

    if (p->read_page != NULL) pfree(p->read_page);

    if (p->write_page != NULL) pfree(p->write_page);

    p->read_page = p->write_page = NULL;

    if (current == p)
    {
        for (p = head; p->next != NULL && ! p->active; p = p->next);

        if (p->next == NULL) current = NULL;
        else current = p;
    }
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
            if (p->read_page != NULL) pfree(p->read_page);
            if (p->write_page != NULL) pfree(p->write_page);
        }
        q = p->next;
        pfree(p);
    }
    head = current = tail = NULL;
}

local_doinsert(relation, tuple)

Relation relation;
HeapTuple tuple;

{
    LocalRelList *rel_info;
    Size len = LONGALIGN(tuple->t_len);
    OffsetIndex offsetIndex;
    ItemId      itemId;
    HeapTuple   page_tuple;

    rel_info = GetRelListEntry(relation);

    if (rel_info->write_page == NULL)
    {
        rel_info->write_page = (Page) palloc(BLCKSZ);
        PageInit(rel_info->write_page, BLCKSZ, 0);
    }

    if (len > PageGetFreeSpace(rel_info->write_page))
    {
        WriteLocalPage(rel_info);
        rel_info->write_blocknum++;
        bzero(rel_info->write_page, BLCKSZ);
        PageInit(rel_info->write_page, BLCKSZ, 0);
    }

    offsetIndex = PageAddItem((Page)rel_info->write_page, (Item)tuple,
                              tuple->t_len, InvalidOffsetNumber, LP_USED) - 1;

    itemId = PageGetItemId((Page)rel_info->write_page, offsetIndex);

    page_tuple = (HeapTuple) PageGetItem(rel_info->write_page, itemId);

    /*
     * As this code is being used for "retrieve into" or tmp relations, 
     * the RuleLock can be set to invalid as no tuple rule can exist on
     * such relations.
     */

    page_tuple->t_locktype = DISK_RULE_LOCK;
    ItemPointerSetInvalid(&page_tuple->t_lock.l_ltid);

    ItemPointerSimpleSet(&page_tuple->t_ctid,
                         rel_info->write_blocknum,
                         1 + offsetIndex);
}

WriteLocalPage(rel_info)

LocalRelList *rel_info;

{
    unsigned long offset;
    int nbytes;

    offset = rel_info->write_blocknum * BLCKSZ;

    FileSeek(rel_info->relation->rd_fd, offset, L_SET);

    nbytes = FileWrite(rel_info->relation->rd_fd,
                       (char *) rel_info->write_page,
                       BLCKSZ);

    Assert(nbytes == BLCKSZ);
}

ReadLocalPage(rel_info)

LocalRelList *rel_info;

{
    unsigned long offset;
    int nbytes;

    offset = rel_info->read_blocknum * BLCKSZ;

    FileSeek(rel_info->relation->rd_fd, offset, L_SET);

    nbytes = FileRead(rel_info->relation->rd_fd,
                       (char *) rel_info->read_page,
                       BLCKSZ);
    return(nbytes);
}
