/*
 * shmem.h -- shared memory management structures
 *
 */

#ifndef	ShMemIncluded		/* Included this file only once */
#define ShMemIncluded	1

/*
 * Identification:
 */
#define SHMEM_H	"$Header$"

/* The shared memory region can start at a different address
 * in every process.  Shared memory "pointers" are actually
 * offsets relative to the start of the shared memory region(s).
 */
typedef unsigned int SHMEM_OFFSET;
#define INVALID_OFFSET (-1)
#define BAD_LOCATION (-1)

/* start of the lowest shared memory region.  For now, assume that
 * there is only one shared memory region 
 */
extern SHMEM_OFFSET ShmemBase;


/* coerce an offset into a pointer in this process's address space */
#define MAKE_PTR(xx_offs)\
  (ShmemBase+((unsigned int)(xx_offs)))

/* coerce a pointer into a shmem offset */
#define MAKE_OFFSET(xx_ptr)\
  (SHMEM_OFFSET) (((unsigned int)(xx_ptr))-ShmemBase)

#define SHM_PTR_VALID(xx_ptr)\
  (((unsigned int)xx_ptr) > ShmemBase)

/* cannot have an offset to ShmemFreeStart (offset 0) */
#define SHM_OFFSET_VALID(xx_offs)\
  ((xx_offs != 0) && (xx_offs != INVALID_OFFSET))

/* shmemqueue.c */
typedef struct SHM_QUEUE {
  SHMEM_OFFSET	prev;
  SHMEM_OFFSET	next;
} SHM_QUEUE;

/* shmem.c */
int *ShmemAlloc();
int *ShmemInitStruct();
/* dont declare this so we avoid nested include files */
/*HTAB *ShmemInitHash(); */

typedef int TableID;
typedef char *Addr;

/* size constants for the binding table */
        /* max size of data structure string name */
#define BTABLE_KEYSIZE  (50)
        /* data in binding table hash bucket */
#define BTABLE_DATASIZE (sizeof(BindingEnt) - BTABLE_KEYSIZE)
        /* maximum size of the binding table */
#define BTABLE_SIZE      (100)

/* this is a hash bucket in the binding table */
typedef struct {
        /* string name */
  char          key[BTABLE_KEYSIZE];
        /* location in shared mem */
  unsigned int  location;
        /* numbytes allocated for the structure */
  unsigned int  size;
} BindingEnt;
#endif	/* !defined(ShMemIncluded) */
