/*
 * freelist.c -- routines for manipulating the buffer pool's
 *	replacement strategy freelist.
 *
 * Identification:
 *	$Header$
 *
 * Data Structures:
 *	SharedFreeList is a circular queue.  Notice that this
 *	is a shared memory queue so the next/prev "ptrs" are
 *	buffer ids, not addresses.
 *
 * Sync: all routines in this file assume that the buffer
 * 	semaphore has been acquired by the caller.
 */
#include "internal.h"
#include "storage/spin.h"
#include "utils/log.h"

static
BufferDesc 	*SharedFreeList;

/* only actually used in debugging.  The lock
 * should be acquired before calling the freelist manager.
 */
extern SPINLOCK BufMgrLock;

#define IsInQueue(bf) \
  Assert((bf->freeNext != INVALID_DESCRIPTOR));\
  Assert((bf->freePrev != INVALID_DESCRIPTOR));\
  Assert((bf->flags & BM_FREE))

#define NotInQueue(bf) \
  Assert((bf->freeNext == INVALID_DESCRIPTOR));\
  Assert((bf->freePrev == INVALID_DESCRIPTOR));\
  Assert(! (bf->flags & BM_FREE))


/*
 * AddBufferToFreelist --  
 *
 * In theory, this is the only routine that needs to be changed
 * if the buffer replacement strategy changes.  Just change
 * the manner in which buffers are added to the freelist queue.
 * Currently, they are added on an LRU basis.
 */
AddBufferToFreelist(bf)
BufferDesc *bf;
{
  NotInQueue(bf);

  /* change bf so it points to inFrontOfNew and its successor */
  bf->freePrev = SharedFreeList->freePrev;
  bf->freeNext = Free_List_Descriptor;

  /* insert new into chain */
  BufferDescriptors[bf->freeNext].freePrev = bf->id;
  BufferDescriptors[bf->freePrev].freeNext = bf->id;
}

/*
 * PinBuffer -- make buffer unavailable for replacement.
 */
PinBuffer(buf)
    BufferDesc *buf;
{
  is_LOCKED(BufMgrLock);

  /* Assert (buf->refcount < 25); */

  if (buf->refcount == 0) {
    IsInQueue(buf);

    /* remove from freelist queue */
    BufferDescriptors[buf->freeNext].freePrev = buf->freePrev;
    BufferDescriptors[buf->freePrev].freeNext = buf->freeNext;
    buf->freeNext = buf->freePrev = INVALID_DESCRIPTOR;

    /* mark buffer as no longer free */
    buf->flags &= ~BM_FREE;
  } else {
    NotInQueue(buf);
  }

  buf->refcount++;

}

/*
 * UnpinBuffer -- make buffer available for replacement.
 */
UnpinBuffer(buf)
    BufferDesc *buf;
{
  is_LOCKED(BufMgrLock);

  Assert (buf->refcount);
  buf->refcount--;
  NotInQueue(buf);

  if (buf->refcount == 0) {
    AddBufferToFreelist(buf);
    buf->flags |= BM_FREE;
  } else {
    /* do nothing */
  }

}


/*
 * GetFreeBuffer() -- get the 'next' buffer from the freelist.
 *
 */
BufferDesc *
GetFreeBuffer()
{
    BufferDesc *buf;

    is_LOCKED(BufMgrLock);


    if (Free_List_Descriptor == SharedFreeList->freeNext) {

      /* queue is empty. All buffers in the buffer pool are pinned. */
      elog(DEBUG,"out of free buffers: time to abort !\n");
      return(NULL);
    }
    buf = &(BufferDescriptors[SharedFreeList->freeNext]);

    /* remove from freelist queue */
    BufferDescriptors[buf->freeNext].freePrev = buf->freePrev;
    BufferDescriptors[buf->freePrev].freeNext = buf->freeNext;
    buf->freeNext = buf->freePrev = INVALID_DESCRIPTOR;

    buf->flags &= ~(BM_FREE);

    return(buf);
}

/*
 * InitFreeList -- initialize the dummy buffer descriptor used
 *   	as a freelist head.
 *
 * Assume: All of the buffers are already linked in a circular
 *	queue.   Only called by postmaster and only during 
 * 	initialization.
 */
InitFreeList(init)
Boolean init;
{
  SharedFreeList = &(BufferDescriptors[Free_List_Descriptor]);

  if (init) {
      /* we only do this once, normally the postmaster */
      SharedFreeList->data = NULL;
      SharedFreeList->flags = 0;
      SharedFreeList->flags &= ~(BM_VALID | BM_DELETED | BM_FREE);
      SharedFreeList->id = Free_List_Descriptor;

      /* insert it into a random spot in the circular queue */
      SharedFreeList->freeNext = BufferDescriptors[0].freeNext;
      SharedFreeList->freePrev = 0;
      BufferDescriptors[SharedFreeList->freeNext].freePrev = 
	BufferDescriptors[SharedFreeList->freePrev].freeNext = 
	  Free_List_Descriptor;
    }
}


/*
 * print out the free list and check for breaks.
 */
DBG_FreeListCheck(nfree)
int nfree;
{
  int i;
  BufferDesc *buf;

  buf = &(BufferDescriptors[SharedFreeList->freeNext]);
  for (i=0;i<nfree;i++,buf = &(BufferDescriptors[buf->freeNext])) {

    if (! (buf->flags & (BM_FREE))){
      if (buf != SharedFreeList) {
	printf("\tfree list corrupted: %d flags %x\n",
	       buf->id,buf->flags);
      } else  {
	printf("\tfree list corrupted: too short -- %d not %d\n",
	       i,nfree);

      }
      

    }
    if ((BufferDescriptors[buf->freeNext].freePrev != buf->id) ||
	(BufferDescriptors[buf->freePrev].freeNext != buf->id)) {
      printf("\tfree list links corrupted: %d %d %d\n",
	     buf->id,buf->freePrev,buf->freeNext);
    }

  }
  if (buf != SharedFreeList) {
    printf("\tfree list corrupted: %d-th buffer is %d\n",
	   nfree,buf->id);
	   
  }
}

