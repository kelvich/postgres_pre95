/* ----------------------------------------------------------------
 *      FILE
 *     	execshmem.h
 *     
 *      DESCRIPTION
 *     	support for executor allocated shared memory.  used by
 *      wei's slave backend code.
 *
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef ExecShMemIncluded
#define ExecShMemIncluded 1

struct memoryheaderdata {
    char                        *beginaddr;
    int                         size;
    struct memoryheaderdata     *next;
};
typedef struct memoryheaderdata MemoryHeaderData;
typedef MemoryHeaderData *MemoryHeader;

/* ex_shmem.c */
extern char *ExecSMReserve ARGS((int size));
extern void ExecSMInit ARGS((void));
extern MemoryHeader ExecGetSMSegment ARGS((void));
extern void ExecSMSegmentFree ARGS((MemoryHeader mp));
extern void ExecSMSegmentFreeUnused ARGS((MemoryHeader mp, int usedsize));

#endif /* ExecShMemIncluded */
