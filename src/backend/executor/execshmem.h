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
char *ExecSMReserve ARGS((int size ));
void ExecSMInit ARGS((void ));
MemoryHeader ExecGetSMSegment ARGS((void ));
void ExecSMSegmentFree ARGS((MemoryHeader mp ));
void ExecSMSegmentFreeUnused ARGS((MemoryHeader mp , int usedsize ));

#endif /* ExecShMemIncluded */
