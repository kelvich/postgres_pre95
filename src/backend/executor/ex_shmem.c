/* ----------------------------------------------------------------
 *   FILE
 *	ex_shmem.c
 *	
 *   DESCRIPTION
 *	code for a specialized memory allocator to manage the shared memory
 *	for the parallel executor
 *
 *   INTERFACE ROUTINES
 *	ExecSMInit - initializes the shared mem meta-data
 *	ExecGetSMSegment - allocates a segment of shared memory
 *	ExecSMSegmentFree - frees a segment of shared memory
 *	ExecSMSegmentFreeUnused	- frees unused memory in a segment
 *
 *   NOTES
 *	the key consideration is to avoid allocation overhead.  two
 *	important reasons lead to this special memory allocator:
 *	1. only the master backend allocates the shared memory;
 *	2. the shared memory is used for passing querydescs and reldescs,
 *	   which consists of small nodes.
 *	basically, the pattern of memory allocation we are dealing with is
 *	that the master issues a bunch of small requests then frees them
 *	at the same time.  so here the idea is to pack these
 *	small chunks of allocated memory into one segment.
 *
 *	memory is managed as a a linked list of variable-size segments,
 *	efforts has been made to merge consecutive segments into
 *	larger segments, initially the entire executor shared memory is
 *	one segment.
 *
 *	the functions here only deal with segments, allocations inside
 *	a segment is handled by specialied functions ProcGroupSMBeginAlloc(),
 *	ProcGroupSMEndAlloc() and ProcGroupSMAlloc() in tcop/slaves.c.
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */
 
#include "tmp/postgres.h"

 RcsId("$Header$");

#include "tmp/align.h"
#include "utils/log.h"
#include "executor/x_shmem.h"

static MemoryHeader FreeSMQueue;  /* queue of free SM */
extern char *ExecutorSharedMemory;
extern int ExecutorSharedMemorySize;

/* -------------------------------
 *	ExecSMReserve
 *
 *	reserve a certain number of bytes in the executor shared memory
 *	for the global shared variables
 * -------------------------------
 */
char *
ExecSMReserve(size)
int size;
{
    char *p;

    p = ExecutorSharedMemory;
    ExecutorSharedMemory = (char*)LONGALIGN(ExecutorSharedMemory + size);
    ExecutorSharedMemorySize -= (ExecutorSharedMemory - p);
    return p;
}

/* --------------------------------
 *	ExecSMInit
 *
 *	This function initializes executor shared memory.
 *	Initially, the whole thing is a segment.
 * --------------------------------
 */
void
ExecSMInit()
{
    FreeSMQueue = (MemoryHeader)ExecutorSharedMemory;
    /* -------------------
     * assume sizeof(MemoryHeaderData) is multiple of sizeof(long),
     * too lazy to put LONGALIGN() all over the places,
     * not good for portability, eventually will put LONGALIGN() in.
     * -------------------
     */
    FreeSMQueue->beginaddr = ExecutorSharedMemory + sizeof(MemoryHeaderData);
    FreeSMQueue->size = ExecutorSharedMemorySize - sizeof(MemoryHeaderData);
    FreeSMQueue->next = NULL;
}

/* -------------------------------
 *	ExecGetSMSegment
 *
 *	get a free memory segment from the free memory queue
 * -------------------------------
 */
MemoryHeader
ExecGetSMSegment()
{
    MemoryHeader p;

    if (FreeSMQueue == NULL)
	elog(WARN, "out of shared memory segments for parallel executor.");
    p = FreeSMQueue;
    FreeSMQueue = FreeSMQueue->next;

    return p;
}

/* ---------------------------------
 *	mergeSMSegment
 *
 *	try to merge two segments, returns true if successful, false otherwise
 * ---------------------------------
 */
static bool
mergeSMSegment(lowSeg, highSeg)
MemoryHeader lowSeg, highSeg;
{
    if (lowSeg == NULL || highSeg == NULL)
	return false;
    if ((char*)LONGALIGN(lowSeg->beginaddr + lowSeg->size) == (char*)highSeg) {
	lowSeg->size = highSeg->beginaddr + highSeg->size - lowSeg->beginaddr;
	lowSeg->next = highSeg->next;
	return true;
      }
    return false;
}

/* ----------------------------
 *	ExecSMSegmentFree
 *
 *	frees a memory segment
 *	insert the segment into the free queue in ascending order of
 *	the begining address
 *	merge with neighbors if possible
 * -----------------------------
 */
void
ExecSMSegmentFree(mp)
MemoryHeader mp;
{
    MemoryHeader prev, cur;
    prev = NULL;
    for (cur=FreeSMQueue; cur!=NULL; cur=cur->next) {
	if (cur->beginaddr > mp->beginaddr)
	    break;
	prev = cur;
      }
    if (prev == NULL) {
	FreeSMQueue = mp;
	if (!mergeSMSegment(mp, cur))
	    mp->next = cur;
      }
    else {
	mp->next = cur;
	if (mergeSMSegment(prev, mp)) {
	    mergeSMSegment(prev, cur);
	  }
	else {
	    prev->next = mp;
	    mergeSMSegment(mp, cur);
	  }
      }
}

#define MINFREESIZE	10*sizeof(MemoryHeaderData)

/* ---------------------------
 *	ExecSMSegmentFreeUnused
 *
 *	frees unused memory in a segment
 * ---------------------------
 */
void
ExecSMSegmentFreeUnused(mp, usedsize)
MemoryHeader mp;
int usedsize;
{
    MemoryHeader newmp;

    if (mp->size - usedsize < MINFREESIZE)
	return;
    newmp = (MemoryHeader)LONGALIGN(mp->beginaddr + usedsize);
    newmp->beginaddr = (char*)newmp + sizeof(MemoryHeaderData);
    newmp->size = mp->beginaddr + mp->size - newmp->beginaddr;
    mp->size = usedsize;
    ExecSMSegmentFree(newmp);
}
