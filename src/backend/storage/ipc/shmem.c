/*
 * shmem.c -- create shared memory and initialize shared memory
 *	data structures.
 *
 * Identification:
 *	$Header$
 *
 * POSTGRES processes share one or more regions of shared memory.
 * The shared memory is created by a postmaster and is "attached to"
 * by each of the backends.  The routines in this file are used for
 * allocating and binding to shared memory data structures.
 *
 * NOTES:
 *	(a) There are three kinds of shared memory data structures
 *  available to POSTGRES: fixed-size structures, queues and hash 
 *  tables.  Fixed-size structures contain things like global variables
 *  for a module and should never be allocated after the process 
 *  initialization phase.  Hash tables have a fixed maximum size, but
 *  their actual size can vary dynamically.  When entries are added
 *  to the table, more space is allocated.  Queues link data structures 
 *  that have been allocated either as fixed size structures or as hash 
 *  buckets.  Each shared data structure has a string name to identify 
 *  it (assigned in the module that declares it).
 *
 *	(b) During initialization, each module looks for its
 *  shared data structures in a hash table called the "Binding Table".
 *  If the data structure is not present, the caller can allocate
 *  a new one and initialize it.  If the data structure is present, 
 *  the caller "attaches" to the structure by initializing a pointer 
 *  in the local address space.  
 * 	The binding table has two purposes: first, it gives us
 *  a simple model of how the world looks when a backend process 
 *  initializes.  If something is present in the binding table,
 *  it is initialized.  If it is not, it is uninitialized.  Second,
 *  the binding table allows us to allocate shared memory on demand
 *  instead of trying to preallocate structures and hard-wire the
 *  sizes and locations in header files.  If you are using a lot
 *  of shared memory in a lot of different places (and changing
 *  things during development), this is important.
 *
 *	(c) memory allocation model: shared memory can never be 
 *  freed, once allocated.   Each hash table has its own free list,
 *  so hash buckets can be reused when an item is deleted.  However,
 *  if one hash table grows very large and then shrinks, its space
 *  cannot be redistributed to other tables.  We could build a simple
 *  hash bucket garbage collector if need be.  Right now, it seems
 *  unnecessary.
 *
 *   	See InitSem() in sem.c for an example of how to use the
 *  binding table.
 *
 */

#include "tmp/postgres.h"
#include "storage/ipc.h"
#include "storage/ipci.h"
#include "storage/shmem.h"
#include "storage/spin.h"
#include "utils/hsearch.h"
#include "utils/log.h"

/* default shared memory size */
#define SHMEM_SIZE	(300000)

/* shared memory global variables */
	/* start and end address of shared memory */
unsigned int  ShmemBase = 0;
unsigned int  ShmemEnd = 0;
	/* current size (and default) */
unsigned int  ShmemSize = SHMEM_SIZE;

	/* pointer to the OFFSET of first free shared memory */
unsigned int *ShmemFreeStart = NULL;
	/* start of the binding table (for bootstrap) */
unsigned int *ShmemBindingTabOffset = NULL;
	/* flag becomes true when shared mem is created by POSTMASTER*/
int	      ShmemBootstrap = FALSE;
	/* lock for shared memory allocation */
SPINLOCK      *ShmemLock;
	/* lock for binding table access */
SPINLOCK      *BindingLock;

/* forward */
HTAB *ShmemInitHash();

/* size constants for the binding table */
	/* max size of data structure string name */
#define BTABLE_KEYSIZE 	(50)
	/* data in binding table hash bucket */
#define BTABLE_DATASIZE (sizeof(BindingEnt) - BTABLE_KEYSIZE)
	/* maximum size of the binding table */
#define BTABLE_SIZE 	(100)

/* this is a hash bucket in the binding table */
typedef struct {
	/* string name */
  char 		key[BTABLE_KEYSIZE];
	/* location in shared mem */
  unsigned int 	location;
	/* numbytes allocated for the structure */
  unsigned int 	size;
} BindingEnt;

HTAB *BindingTable = NULL;

/*
 *  CreateSharedRegion() --
 *
 *  This routine is called once by the postmaster to
 *  initialize the shared buffer pool.  Assume there is
 *  only one postmaster so no synchronization is necessary
 *  until after this routine completes successfully.
 *
 * key is a unique identifier for the shmem region.
 * size is the size of the region.
 */
static IpcMemoryId ShmemId;

ShmemCreate(key,size)
unsigned int	key,size;
{
  if (size)
      ShmemSize = size;
  /* create shared mem region */
  if ((ShmemId=IpcMemoryCreate(key,ShmemSize,IPCProtection))
      ==IpcMemCreationFailed) {
    elog(FATAL,"ShmemCreate: cannot create region");
    exit(1);
  }

  /* ShmemBootstrap is true if shared memory has been
   * created, but not yet initialized.  Only the 
   * postmaster/creator-of-all-things should have
   * this flag set.
   */
  ShmemBootstrap = TRUE;
}

/*
 *  InitShmem() -- map region into process address space
 *	and initialize shared data structures.
 *
 */
InitShmem(key,size)
unsigned int key;
unsigned int size;
{
  Addr 	sharedRegion;
  unsigned int 	currFreeSpace;

  HASHCTL 	info;
  int 		hash_flags;
  BindingEnt *	result,item;
  Boolean	found;
  IpcMemoryId	shmid;

  /* if zero key, use default memory size */
  if (size)
    ShmemSize = size;

  /* default key is 0 */

  /* attach to shared memory region (SysV or BSD OS specific) */
  if (ShmemBootstrap && key == PrivateIPCKey)
     /* if we are running backend alone */
      shmid = ShmemId;
  else
      shmid = IpcMemoryIdGet(IPCKeyGetBufferMemoryKey(key), ShmemSize);
  sharedRegion = IpcMemoryAttach(shmid);
  if (sharedRegion == NULL) {
    elog(FATAL,"AttachSharedRegion: couldn't attach to shmem\n");
    return(FALSE);
  }

  /* get pointers to the dimensions of shared memory */
  ShmemBase = (unsigned int) sharedRegion;
  ShmemEnd  = (unsigned int) sharedRegion + ShmemSize;
  currFreeSpace = 0;

  /* First int in shared memory is the count of available space */
  ShmemFreeStart = (unsigned int *) ShmemBase;
  /* next is a shmem pointer to the binding table */
  ShmemBindingTabOffset = ShmemFreeStart + 1;

  currFreeSpace += 
    sizeof(ShmemFreeStart) + sizeof(ShmemBindingTabOffset);

  /* bootstrap initialize spin locks so we can start to use the
   * allocator and binding table.
   */
  if (! InitSpinLocks(ShmemBootstrap, &currFreeSpace,
		      IPCKeyGetSpinLockSemaphoreKey(key))) {
    return(FALSE);
  }

  /* We have just allocated additional space for two spinlocks.
   * Now setup the global free space count 
   */
  if (ShmemBootstrap) {
    *ShmemFreeStart = currFreeSpace;
  }

  /* if ShmemFreeStart is NULL, then the allocator won't work */
  Assert(*ShmemFreeStart);

  /* create OR attach to the shared memory binding table */
  info.keysize = BTABLE_KEYSIZE;
  info.datasize = BTABLE_DATASIZE;
  hash_flags = (HASH_ELEM);

  /* This will acquire the binding table lock, but not release it. */
  BindingTable = ShmemInitHash("BindingTable",
			       BTABLE_SIZE,BTABLE_SIZE,
			       &info,hash_flags);

  if (! BindingTable) {
    elog(FATAL,"InitShmem: couldn't initialize Binding Table");
    return(FALSE);
  }

  /* Now, check the binding table for an entry to the binding
   * table.  If there is an entry there, someone else created
   * the table.  Otherwise, we did and we have to initialize it.
   */
  bzero(item.key,BTABLE_KEYSIZE);
  strncpy(item.key,"BindingTable",BTABLE_KEYSIZE);

  result = (BindingEnt *) 
    hash_search(BindingTable,&item,ENTER, &found);


  if (! result ) {
    elog(FATAL,"InitShmem: corrupted binding table");
    return(FALSE);
  }

  if (! found) {
    /* bootstrapping shmem: we have to initialize the 
     * binding table now.
     */

    Assert(ShmemBootstrap);
    result->location = MAKE_OFFSET(BindingTable->hctl);
    *ShmemBindingTabOffset = result->location;
    result->size = BTABLE_SIZE;

    ShmemBootstrap = FALSE;

  }  else {
    Assert(! ShmemBootstrap);
  }
  /* now release the lock acquired in ShmemHashInit */
  SpinRelease (BindingLock);

  Assert (result->location == MAKE_OFFSET(BindingTable->hctl));

  return(TRUE);
}

/*
 * ShmemAlloc -- allocate word-aligned byte string from
 * 	shared memory
 *
 * Assumes ShmemLock and ShmemFreeStart are initialized.
 * Returns: real pointer to memory or NULL if we are out
 * 	of space.  Has to return a real pointer in order 
 *  	to be compatable with malloc().
 */
int *
ShmemAlloc(size)
unsigned int size;
{
  unsigned int tmpFree;
  int *newSpace;

  /* ensure space is word aligned */
  if (size % sizeof(int *))
    size += sizeof(int *) - (size % sizeof(int *));

  Assert(*ShmemFreeStart);

  SpinAcquire(ShmemLock);

  tmpFree = *ShmemFreeStart + size;
  if (tmpFree <= ShmemSize) {
    newSpace = (int *)MAKE_PTR(*ShmemFreeStart);
    *ShmemFreeStart += size;
  } else {
    newSpace = NULL;
  }

  SpinRelease(ShmemLock); 

  if (! newSpace) {
    elog(NOTICE,"ShmemAlloc: out of memory ");
  }
  return(newSpace);
}

/*
 * ShmemIsValid -- test if an offset refers to valid shared memory 
 * 
 * Returns TRUE if the pointer is valid.
 */
ShmemIsValid(addr)
unsigned int addr;
{
  return ((addr<ShmemEnd) && (addr>=ShmemBase));
}

/*
 * ShmemInitHash -- Create/Attach to and initialize 
 * 	shared memory hash table.
 *
 * Notes:
 *
 * assume caller is doing some kind of synchronization
 * so that two people dont try to create/initialize the
 * table at once.  Use SpinAlloc() to create a spinlock
 * for the structure before creating the structure itself.
 */
HTAB *
ShmemInitHash(name,init_size,max_size,infoP,hash_flags)
char *	name;		/* table string name for binding */
int 	init_size;	/* initial size */
int 	max_size;	/* max size of the table */
HASHCTL *infoP;		/* info about key and bucket size */
int hash_flags;		/* info about infoP */
{
  Boolean	found;
  int  *	location;

  /* shared memory hash tables have a fixed max size so that the
   * control structures don't try to grow.  The segbase is for
   * calculating pointer values.  The shared memory allocator
   * must be specified.
   */
  infoP->segbase = (int *) ShmemBase;
  infoP->alloc = ShmemAlloc;
  infoP->max_size = max_size;
  hash_flags |= HASH_SHARED_MEM;

  /* look it up in the binding table */
  location = 
    ShmemInitStruct(name,log2(max_size)+sizeof(HHDR),&found);

  /* binding table is corrupted.  Let someone else give the 
   * error message since they have more information 
   */
  if (location == NULL) {
    return(0);
  }

  /* it already exists, attach to it rather than allocate and
   * initialize new space 
   */
  if (found) {
    hash_flags |= HASH_ATTACH;
  }

  /* these structures were allocated or bound in ShmemInitStruct */
	/* control information and parameters */
  infoP->hctl = (int *) location;
	/* directory for hash lookup */
  infoP->dir = (int *) (location + sizeof(HHDR));
  
  return(hash_create(init_size, infoP, hash_flags));;
}

/*
 * ShmemPIDLookup -- lookup process data structure using process id
 *
 * Returns: TRUE if no error.  locationPtr is initialized if PID is
 *	found in the binding table.
 *
 * NOTES:
 * 	only information about success or failure is the value of
 *	locationPtr.
 */
Boolean
ShmemPIDLookup(pid,locationPtr)
int		pid;
SHMEM_OFFSET*	locationPtr;
{
  BindingEnt *	result,item;
  Boolean	found;

  Assert (BindingTable);
  bzero(item.key,BTABLE_KEYSIZE);
  sprintf(item.key,"PID %d",pid);
  
  SpinAcquire(BindingLock);
  result = (BindingEnt *) 
    hash_search(BindingTable,&item, ENTER, &found);

  if (! result) {

    SpinRelease(BindingLock);
    elog(WARN,"ShmemInitPID: BindingTable corrupted");
    return(FALSE);

  } 

  if (found) {
    *locationPtr = result->location;
  } else {
    result->location = *locationPtr;
  }

  SpinRelease(BindingLock);
  return (TRUE);
}

/*
 * ShmemPIDDestroy -- destroy binding table entry for process
 *	using process id
 *
 * Returns: offset of the process struct in shared memory or
 *	INVALID_OFFSET if not found.
 *
 * Side Effect: removes the entry from the binding table
 */
SHMEM_OFFSET
ShmemPIDDestroy(pid)
int		pid;
{
  BindingEnt *	result,item;
  Boolean	found;
  SHMEM_OFFSET  location;

  Assert(BindingTable);

  bzero(item.key,BTABLE_KEYSIZE);
  sprintf(item.key,"PID %d",pid);
  
  SpinAcquire(BindingLock);
  result = (BindingEnt *) 
    hash_search(BindingTable,&item, REMOVE, &found);

  location = result->location;
  SpinRelease(BindingLock);

  if (! result) {

    elog(WARN,"ShmemPIDDestroy: PID table corrupted");
    return(INVALID_OFFSET);

  } 

  if (found)
    return (location);
  else {
    elog(NOTICE,"ShmemPIDDestroy: PID table entry missing");
    return(INVALID_OFFSET);
  }
}

/*
 * ShmemInitStruct -- Create/attach to a structure in shared
 * 	memory.
 *
 *  This is called during initialization to find or allocate
 *     	a data structure in shared memory.  If no other processes
 *	have created the structure, this routine allocates space
 *	for it.  If it exists already, a pointer to the existing
 * 	table is returned.  
 *
 *  Returns: real pointer to the object.  FoundPtr is TRUE if
 *	the object is already in the binding table (hence, already
 *	initialized).
 */
int *
ShmemInitStruct(name,size,foundPtr)
char *name;
unsigned int size;
Boolean *foundPtr;
{
  BindingEnt *	result,item;
  int * structPtr;

  strncpy(item.key,name,BTABLE_KEYSIZE);
  item.location = BAD_LOCATION;
  
  SpinAcquire(BindingLock);

  if (! BindingTable) {
    /* Assert() is a macro now. substitutes inside quotes. */
    char *strname = "BindingTable";

    /* If the binding table doesnt exist, we fake it.
     *
     * If we are creating the first binding table, then let 
     * shmemalloc() allocate the space for a new HTAB.  Otherwise,
     * find the old one and return that.  Notice that the
     * BindingLock is held until the binding table has been completely
     * initialized.
     */
    Assert (! strcmp(name,strname)) ;
    if (ShmemBootstrap) {
      /* in POSTMASTER/Single process */

      *foundPtr = FALSE;
      return((int *)ShmemAlloc(size));

    } else {
      Assert (ShmemBindingTabOffset);

      *foundPtr = TRUE;
      return((int *)MAKE_PTR(*ShmemBindingTabOffset));
    }


  } else {
	/* look it up in the bindint table */
    result = (BindingEnt *) 
      hash_search(BindingTable,&item,ENTER, foundPtr);
  }

  if (! result) {

    SpinRelease(BindingLock);

    elog(WARN,"ShmemInitStruct: Binding Table corrupted");
    return(NULL);

  } else if (*foundPtr) {
    /*
     * Structure is in the binding table so someone else has allocated 
     * it already.  The size better be the same as the size we are 
     * trying to initialize to or there is a name conflict (or worse).
     */
    if (result->size != size) {
      SpinRelease(BindingLock);

      elog(NOTICE,"ShmemInitStruct: BindingTable entry size is wrong");
      /* let caller print its message too */
      return(NULL);
    }
    structPtr = (int *)MAKE_PTR(result->location);
  } else {

    /* It isn't in the table yet. allocate and initialize it */
    structPtr = ShmemAlloc(size);
    if (! structPtr) {
      /* out of memory */
      Assert (BindingTable);
      (void) hash_search(BindingTable,&item,REMOVE, foundPtr);
      SpinRelease(BindingLock);
      *foundPtr = FALSE;

      elog(NOTICE,"ShmemInitStruct: cannot allocate '%s'",
	   name);
      return(NULL);
    } 
    result->size = size;
    result->location = MAKE_OFFSET(structPtr);
  }
  Assert (ShmemIsValid((unsigned int)structPtr));

  SpinRelease(BindingLock);
  return(structPtr);
}



