/*
 * shmqueue.c -- shared memory linked lists
 *
 * Identification:
 *	$Header$
 *
 * Package for managing doubly-linked lists in shared memory.
 * The only tricky thing is that SHM_QUEUE will usually be a field 
 * in a larger record.  SHMQueueGetFirst has to return a pointer
 * to the record itself instead of a pointer to the SHMQueue field
 * of the record.  It takes an extra pointer and does some extra
 * pointer arithmetic to do this correctly.
 *
 * NOTE: These are set up so they can be turned into macros some day.
 */
#include "tmp/postgres.h"
#include "storage/shmem.h"
#include "utils/log.h"

/*
 * ShmemQueueInit -- make the head of a new queue point
 * 	to itself
 */
SHMQueueInit(queue)
SHM_QUEUE	*queue;
{
  Assert(SHM_PTR_VALID(queue));
  (queue)->prev = (queue)->next = MAKE_OFFSET(queue);
}

/*
 * SHMQueueIsDetached -- TRUE if element is not currently
 *	in a queue.
 */
SHMQueueIsDetached(queue)
SHM_QUEUE 	*queue;
{
  Assert(SHM_PTR_VALID(queue));
  return ((queue)->prev == INVALID_OFFSET);
}

/*
 * SHMQueueElemInit -- clear an element's links
 */
SHMQueueElemInit(queue)
SHM_QUEUE 	*queue;
{
  Assert(SHM_PTR_VALID(queue));
  (queue)->prev = (queue)->next = INVALID_OFFSET;
}

/*
 * SHMQueueDelete -- remove an element from the queue and
 * 	close the links
 */
SHMQueueDelete(queue)
SHM_QUEUE	*queue;
{
  SHM_QUEUE *nextElem = (SHM_QUEUE *) MAKE_PTR((queue)->next);
  SHM_QUEUE *prevElem = (SHM_QUEUE *) MAKE_PTR((queue)->prev);

/*
  dumpQ(queue);
*/

  Assert(SHM_PTR_VALID(queue));
  Assert(SHM_PTR_VALID(nextElem));
  Assert(SHM_PTR_VALID(prevElem));
  prevElem->next =  (queue)->next;
  nextElem->prev =  (queue)->prev;

/*
  dumpQ((SHM_QUEUE *)MAKE_PTR(queue->prev));
*/

}

dumpQ(q)
SHM_QUEUE	*q;
{
    char elem[16];
    char buf[512];
    SHM_QUEUE	*start = q;
    int count = 0;

    sprintf(buf, "q prevs: %x", MAKE_OFFSET(q));
    q = (SHM_QUEUE *)MAKE_PTR(q->prev);
    while (q != start)
    {
	sprintf(elem, "--->%x", MAKE_OFFSET(q));
	strcat(buf, elem);
	q = (SHM_QUEUE *)MAKE_PTR(q->prev);
	if (q->prev == MAKE_OFFSET(q))
	    break;
	if (count++ > 40)
	{
	    strcat(buf, "BAD PREV QUEUE!!");
	    break;
	}
    }
    sprintf(elem, "--->%x", MAKE_OFFSET(q));
    strcat(buf, elem);
    elog(DEBUG, buf);

    sprintf(buf, "q nexts: %x", MAKE_OFFSET(q));
    count = 0;
    q = (SHM_QUEUE *)MAKE_PTR(q->next);
    while (q != start)
    {
	sprintf(elem, "--->%x", MAKE_OFFSET(q));
	strcat(buf, elem);
	q = (SHM_QUEUE *)MAKE_PTR(q->next);
	if (q->next == MAKE_OFFSET(q))
	    break;
	if (count++ > 10)
	{
	    strcat(buf, "BAD NEXT QUEUE!!");
	    break;
	}
    }
    sprintf(elem, "--->%x", MAKE_OFFSET(q));
    strcat(buf, elem);
    elog(DEBUG, buf);
}

/*
 * SHMQueueInsertHD -- put elem in queue between the queue head
 *	and its "prev" element.
 */
SHMQueueInsertHD(queue,elem)
SHM_QUEUE *queue,*elem;
{
  SHM_QUEUE *prevPtr = (SHM_QUEUE *) MAKE_PTR((queue)->prev);
  SHMEM_OFFSET	elemOffset = MAKE_OFFSET(elem);

  Assert(SHM_PTR_VALID(queue));
  Assert(SHM_PTR_VALID(elem));

  (elem)->next = prevPtr->next;
  (elem)->prev = queue->prev;
  (queue)->prev = elemOffset;
  prevPtr->next = elemOffset;
}

SHMQueueInsertTL(queue,elem)
SHM_QUEUE *queue,*elem;
{
  SHM_QUEUE *nextPtr = (SHM_QUEUE *) MAKE_PTR((queue)->next);
  SHMEM_OFFSET	elemOffset = MAKE_OFFSET(elem);

  Assert(SHM_PTR_VALID(queue));
  Assert(SHM_PTR_VALID(elem));

/*
  dumpQ(queue);
*/

  (elem)->prev = nextPtr->prev;
  (elem)->next = queue->next;
  (queue)->next = elemOffset;
  nextPtr->prev = elemOffset;

/*
  dumpQ(queue);
*/
}

/*
 * SHMQueueFirst -- Get the first element from a queue
 *
 * First element is queue->next.  If SHMQueue is part of
 * a larger structure, we want to return a pointer to the
 * whole structure rather than a pointer to its SHMQueue field.
 * I.E. struct {
 *	int 		stuff;
 *	SHMQueue 	elem;
 * } ELEMType; 
 * when this element is in a queue (queue->next) is struct.elem.
 * nextQueue allows us to calculate the offset of the SHMQueue
 * field in the structure.
 *
 * call to SHMQueueFirst should take these parameters:
 *
 *   &(queueHead),&firstElem,&(firstElem->next)
 *
 * Note that firstElem may well be uninitialized.  if firstElem
 * is initially K, &(firstElem->next) will be K+ the offset to
 * next.
 */
SHMQueueFirst(queue,nextPtrPtr,nextQueue)
SHM_QUEUE	*queue;
Addr 	*nextPtrPtr;
SHM_QUEUE	*nextQueue;
{
  SHM_QUEUE *elemPtr = (SHM_QUEUE *) MAKE_PTR((queue)->next);

  Assert(SHM_PTR_VALID(queue));
  *nextPtrPtr = (Addr) (((unsigned) *nextPtrPtr) +
    ((unsigned) elemPtr) - ((unsigned) nextQueue)); 

  /*
  nextPtrPtr a ptr to a structure linked in the queue
  nextQueue is the SHMQueue field of the structure
  *nextPtrPtr - nextQueue is 0 minus the offset of the queue 
  	field n the record 
  elemPtr + (*nextPtrPtr - nexQueue) is the start of the
  	structure containing elemPtr.
  */
}

/*
 * SHMQueueDoLRU -- move elem to the front of the list
 */
SHMQueueDoLRU(queue,elem)
SHM_QUEUE	*queue;
SHM_QUEUE	*elem;
{
  Assert(SHM_PTR_VALID(queue));

  /* do not LRU if the element is currently detached from the queue */
  if ((elem)->prev == INVALID_OFFSET)
    return;

  SHMQueueDelete(elem);
  SHMQueueInsertTL(queue,elem);
}


SHMQueueCheck(queue)
SHM_QUEUE	*queue;
{
  SHM_QUEUE *nextElem = (SHM_QUEUE *) MAKE_PTR((queue)->next);
  SHM_QUEUE *prevElem = (SHM_QUEUE *) MAKE_PTR((queue)->prev);

/*
  printf("curr: %x next %x prev %x: prev.next %x next.prev %x\n",
	 MAKE_OFFSET(queue),queue->next,queue->prev,
	 nextElem->prev,prevElem->next);
*/
  Assert(nextElem->prev == MAKE_OFFSET(queue));
  Assert(prevElem->next == MAKE_OFFSET(queue));
}

/*
 * SHMQueueEmpty -- TRUE if queue head is only element, FALSE otherwise
 */
SHMQueueEmpty(queue)
SHM_QUEUE       *queue;
{
  Assert(SHM_PTR_VALID(queue));

  if (queue->prev == MAKE_OFFSET(queue)) 
  {
    Assert(queue->next = MAKE_OFFSET(queue));
    return(TRUE);
  }
  return(FALSE);
}
