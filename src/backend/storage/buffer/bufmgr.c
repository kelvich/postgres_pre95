/*
 * bufmgr.c -- buffer manager interface routines
 *
 * Identification:
 *	$Header$
 *
 * BufferAlloc() -- lookup a buffer in the buffer table.  If
 *	it isn't there add it, but do not read it into memory.
 *	This is used when we are about to reinitialize the
 *	buffer so don't care what the current disk contents are.
 *	BufferAlloc() pins the new buffer in memory.
 *
 * ReadBuffer() -- same as BufferAlloc() but reads the data
 *	on a buffer cache miss.
 *
 * ReleaseBuffer() -- unpin the buffer
 *
 * WriteNoReleaseBuffer() -- mark the buffer contents as "dirty"
 *	but don't unpin.  The disk IO is delayed until buffer
 *	replacement if LateWrite flag is set.
 *
 * WriteBuffer() -- WriteNoReleaseBuffer() + ReleaseBuffer() 
 *
 * FlushBuffer() -- as above but never delayed write.
 *
 * BufferSync() -- flush all dirty buffers in the buffer pool.
 * 
 * InitBufferPool() -- Init the buffer module.
 *
 * See other files:  
 * 	freelist.c -- chooses victim for buffer replacement 
 *	buf_table.c -- manages the buffer lookup table
 */
#include <sys/file.h>
#include <stdio.h>
#include "internal.h"
#include "storage/fd.h"
#include "storage/ipc.h"
#include "storage/ipci.h"
#include "storage/shmem.h"
#include "storage/spin.h"
#include "tmp/miscadmin.h"
#include "utils/hsearch.h"
#include "utils/log.h"


int		NBuffers = NDBUFS;  /* NDBUFS defined in miscadmin.h */
int		Data_Descriptors;
int		Free_List_Descriptor;
int		Lookup_List_Descriptor;
int		Num_Descriptors;

BufferDesc 	*BufferDescriptors;
BufferBlock 	BufferBlocks;

static Buffer           BufferDescriptorGetBuffer();

#define BufferGetBufferDescriptor(buffer) ((BufferDesc *)&BufferDescriptors[buffer-1])

/*
 * Data Structures:
 *      buffers live in a freelist and a lookup data structure.
 *	
 *
 * Buffer Lookup:
 *	Two important notes.  First, the buffer has to be
 *	available for lookup BEFORE an IO begins.  Otherwise
 *	a second process trying to read the buffer will 
 *	allocate its own copy and the buffeer pool will 
 *	become inconsistent.
 *
 * Buffer Replacement:
 *	see freelist.c.  A buffer cannot be replaced while in
 *	use either by data manager or during IO.
 *
 * WriteBufferBack:
 *	currently, a buffer is only written back at the time
 *	it is selected for replacement.  It should 
 *	be done sooner if possible to reduce latency of 
 *	BufferAlloc().  Maybe there should be a daemon process.
 *
 * Synchronization/Locking:
 *
 * BufMgrLock lock -- must be acquired before manipulating the 
 * 	buffer queues (lookup/freelist).  Must be released 
 * 	before exit and before doing any IO.  
 *
 * IO_IN_PROGRESS -- this is a flag in the buffer descriptor.
 *      It must be set when an IO is initiated and cleared at
 *      the end of  the IO.  It is there to make sure that one
 *	process doesn't start to use a buffer while another is
 *	faulting it in.  see IOWait/IOSignal.
 *
 * refcount --  A buffer is pinned during IO and immediately
 *	after a BufferAlloc().  A buffer is always either pinned
 *	or on the freelist but never both.  The buffer must be
 *	released, written, or flushed before the end of 
 * 	transaction.
 *
 *
 */

SPINLOCK BufMgrLock;

/* delayed write: TRUE on, FALSE off */
int LateWrite = TRUE;

static int ReadBufferCount;
static int BufferHitCount;
static int BufferFlushCount;

/*
 * ReadBuffer -- returns a buffer containing the requested
 *	block of the requested relation.  If the blknum
 *	requested is NEW_BLOCK, extend the relation file and
 *	allocate a new block.
 *
 * Returns: the buffer descriptor for the buffer containing
 *	the block read or NULL on an error.
 *
 * Assume when this function is called, that reln has been
 *	opened already.
 */
Buffer
ReadBuffer(reln,blockNum)
Relation 	reln;
BlockNumber 	blockNum;
{
  File		virtFile; /* descriptor for reln file */
  BufferDesc *	bufHdr;	  
  int		extend;   /* extending the file by one block */
  int		status;
  Boolean	found;


  ReadBufferCount++;
  extend = (blockNum == NEW_BLOCK);
  /* lookup the buffer.  IO_IN_PROGRESS is set if the requested
   * block is not currently in memory.
   */
  bufHdr = BufferAlloc(reln, blockNum, &found);
  if (! bufHdr) {
    return(InvalidBuffer);
  }

  /* if its already in the buffer pool, we're done */
  if (found) {
    if (extend) {
      /* I don't think this is an error, but should be careful */
      elog(NOTICE,"BufferAlloc: found new block in buf table");
    }
    BufferHitCount++;
    return(BufferDescriptorGetBuffer(bufHdr));
  }

  /* 
   * if we have gotten to this point, the reln pointer must be ok
   * and the relation file must be open.
   */
  virtFile = RelationGetFile(reln);

  if (extend) {
    status = BlockExtend(virtFile,bufHdr);
  } else {
    status = BlockRead(virtFile,bufHdr);
  }

  /* lock buffer manager again to update IO IN PROGRESS */
  SpinAcquire(BufMgrLock);

  if (! status) {
    /* IO Failed.  cleanup the data structures and go home */

    if (! BufTableDelete(bufHdr)) {
      elog(FATAL,"BufRead: buffer table broken after IO error\n");
    }
    /* remember that BufferAlloc() pinned the buffer */
    UnpinBuffer(bufHdr);

    /* 
     * Have to reset the flag so that anyone waiting for
     * the buffer can tell that the contents are invalid.
     */
    bufHdr->flags |= BM_IO_ERROR;

  } else {
    /* IO Succeeded.  clear the flags, finish buffer update */

    bufHdr->flags &= ~(BM_IO_ERROR | BM_IO_IN_PROGRESS);
  }

  /* If anyone was waiting for IO to complete, wake them up now */
#ifdef sequent
  S_UNLOCK(&(bufHdr->io_in_progress_lock));
#else
  if (bufHdr->refcount > 1)
    SignalIO(bufHdr);
#endif
  SpinRelease(BufMgrLock);
    
  return(BufferDescriptorGetBuffer(bufHdr));
} 

/*
 * BufferAlloc -- Get a buffer from the buffer pool but dont
 *	read it.
 *
 * Returns: descriptor for buffer
 */
BufferDesc *
BufferAlloc(reln, blockNum, foundPtr)
Relation	reln;
BlockNumber	blockNum;
Boolean		*foundPtr;
{
  BufferDesc 		*buf;	  
  BufferTag 		newTag;	 /* identity of requested block */
  Boolean		inProgress; /* buffer undergoing IO */
  File			virtFile; /* descriptor for reln file */
  int			status;
  Boolean		newblock = FALSE;
  BufferDesc		oldbufdesc;


    /* create a new tag so we can lookup the buffer */
    /* assume that the relation is already open */
  virtFile = RelationGetFile(reln);
  if (blockNum == NEW_BLOCK) {
      newblock = TRUE;
      /*
       * have to get the block number in order to insert the buffer page
       * into the hash table.
       */
      blockNum = FileSeek(virtFile, 0L, L_XTND)/BLCKSZ;
    }

  INIT_BUFFERTAG(&newTag,reln,blockNum);

  SpinAcquire(BufMgrLock);

  /* see if the block is in the buffer pool already */
  buf = BufTableLookup(&newTag);
  if (buf != NULL) {
    /* Found it.  Now, (a) pin the buffer so no
     * one steals it from the buffer pool, 
     * (b) check IO_IN_PROGRESS, someone may be
     * faulting the buffer into the buffer pool.
     */

    PinBuffer(buf);
    inProgress = (buf->flags & BM_IO_IN_PROGRESS);
    
    *foundPtr = TRUE;
    if (inProgress) {
      WaitIO(buf, BufMgrLock);
      if (buf->flags & BM_IO_ERROR) {
	/* wierd race condition: 
	 *
	 * We were waiting for someone else to read the buffer.  
	 * While we were waiting, the reader boof'd in some
	 *  way, so the contents of the buffer are still
	 * invalid.  By saying that we didn't find it, we can
	 * make the caller reinitialize the buffer.  If two
	 * processes are waiting for this block, both will
	 * read the block.  The second one to finish may overwrite 
	 * any updates made by the first.  (Assume higher level
	 * synchronization prevents this from happening).
	 *
	 * This is never going to happen, don't worry about it.
	 */
	*foundPtr = FALSE;
      }
    }
    SpinRelease(BufMgrLock);
  
    return(buf);
  }

  *foundPtr = FALSE;

  /* Didn't find it in the buffer pool.  We'll have
   * to initialize a new buffer.  First, grab one from
   * the free list.  If it's dirty, flush it to disk.
   * Remember to unlock BufMgr spinloc while doing the IOs.
   */
  buf = GetFreeBuffer();
  if (! buf) {
    /* out of free buffers.  In trouble now. */
     SpinRelease(BufMgrLock);
     return(NULL);
   }

   /* There should be exactly one pin on the buffer
    * after it is allocated.  It isnt in the buffer
    * table yet so no one but us should have a pin.
    */

   Assert(buf->refcount == 0);
   buf->refcount = 1;	       

  /* 
   * Change the name of the buffer in the lookup table:
   *  
   * Need to update the lookup table before the read starts.
   * If someone comes along looking for the buffer while
   * we are reading it in, we don't want them to allocate
   * a new buffer.  For the same reason, we didn't want
   * to erase the buf table entry for the buffer we were
   * writing back until now, either.
   */

  if (! BufTableDelete(buf)) {
    elog(FATAL,"buffer wasn't in the buffer table\n");
  }

  /* save the old buffer descriptor */
  oldbufdesc = *buf;
  if (buf->flags & BM_DIRTY) {
      /* must clear flag first because of wierd race 
       * condition described below.  
       */
      buf->flags &= ~BM_DIRTY;
    }
  /*
   *  record the database name and relation name for this buffer.
   */

  strcpy((char *)&(buf->sb_relname),
         (char *)&(reln->rd_rel->relname),
	 sizeof (NameData));

  INIT_BUFFERTAG(&(buf->tag),reln,blockNum);
  if (! BufTableInsert(buf)) {
    elog(FATAL,"Buffer in lookup table twice \n");
  } 

  /* Buffer contents are currently invalid.  Have
   * to mark IO IN PROGRESS so no one fiddles with
   * them until the read completes.  If this routine
   * has been called simply to allocate a buffer, no
   * io will be attempted, so the flag isnt set.
   */
  buf->flags |= BM_IO_IN_PROGRESS; 
#ifdef sequent
  /* lock the io_in_progress_lock before the read so that
   * other process will wait on it
   */
  Assert(!buf->io_in_progress_lock);
  S_LOCK(&(buf->io_in_progress_lock));
#endif
  SpinRelease(BufMgrLock);

  if (oldbufdesc.flags & BM_DIRTY) {
     (void) BlockReplace(&oldbufdesc);
     BufferFlushCount++;
    } 
  return (buf);
}

/*
 * WriteBuffer--
 *
 *	Pushes buffer contents to disk if LateWrite is
 * not set.  Otherwise, marks contents as dirty.  
 *
 * Assume that buffer is pinned.  Assume that reln is
 *	valid.
 *
 * Side Effects:
 *    	Pin count is decremented.
 */
WriteBuffer(buffer)
Buffer	buffer;
{
  BufferDesc	*bufHdr;
  
  if (! LateWrite) {
    return(FlushBuffer(buffer));
  } else {

    if (BAD_BUFFER_ID(buffer)) {
      return(FALSE);
    }
    bufHdr = BufferGetBufferDescriptor(buffer);

    Assert(bufHdr->refcount > 0);
    SpinAcquire(BufMgrLock);
    bufHdr->flags |= BM_DIRTY; 
    UnpinBuffer(bufHdr);
    SpinRelease(BufMgrLock);
  }
  return(TRUE);
} 

/*
 * BufferRewrite -- special version of WriteBuffer for
 *	BufCopyCommit().  We want to write without
 *	looking up the relation if possible.
 */
Boolean
BufferRewrite(buffer)
Buffer	buffer;
{
  BufferDesc	*bufHdr;
  
  if (BAD_BUFFER_ID(buffer)) {
    return(STATUS_ERROR);
  }
  bufHdr = BufferGetBufferDescriptor(buffer);
  Assert(bufHdr->refcount > 0);

  if (LateWrite) {
    SpinAcquire(BufMgrLock); 
    bufHdr->flags |= BM_DIRTY; 
    UnpinBuffer(bufHdr);
    SpinRelease(BufMgrLock); 
  } else {
    BlockReplace(bufHdr);
  }


  return(STATUS_OK);
} 


/*
 * FlushBuffer -- like WriteBuffer, but force the page
 * 	to disk.
 */
FlushBuffer(buffer)
Buffer	buffer;
{
  BufferDesc	*bufHdr;
  Relation	reln;
  File		file;
  Boolean	found;


  if (BAD_BUFFER_ID(buffer)) {
    return(STATUS_ERROR);
  }
  bufHdr = BufferGetBufferDescriptor(buffer);

  file = BlockPrepareFile(bufHdr, &found);
  if (! BlockWrite(file,bufHdr)) {
    elog(WARN,"WriteBuffer/FlushBuffer: cannot write file \n");
    return(FALSE);
  } 
  SpinAcquire(BufMgrLock);
  bufHdr->flags &= ~BM_DIRTY; 
  UnpinBuffer(bufHdr);
  SpinRelease(BufMgrLock);

  if (!found) {
      FileClose(file);
    }
  return(STATUS_OK);
}

/*
 * WriteNoReleaseBuffer -- like WriteBuffer, but do not
 * 	unpin when the operation is complete.
 */
WriteNoReleaseBuffer(buffer)
Buffer	buffer;
{
  BufferDesc	*bufHdr;

  if (! LateWrite) {
    return(FlushBuffer(buffer));
  } else {

    if (BAD_BUFFER_ID(buffer)){
      return(STATUS_ERROR);
    }
    bufHdr = BufferGetBufferDescriptor(buffer);

    SpinAcquire(BufMgrLock);
    bufHdr->flags |= BM_DIRTY; 
    SpinRelease(BufMgrLock);
  }
  return(STATUS_OK);
}


/*
 * ReleaseBuffer -- remove the pin on a buffer without
 * 	marking it dirty.
 *
 */
ReleaseBuffer(buffer)
Buffer	buffer;
{
  BufferDesc	*bufHdr;

  if (BAD_BUFFER_ID(buffer)) {
    return(STATUS_ERROR);
  }
  bufHdr = BufferGetBufferDescriptor(buffer);

  SpinAcquire(BufMgrLock);
  UnpinBuffer(bufHdr);
  SpinRelease(BufMgrLock);

  return(STATUS_OK);
}

/*
 * AcquireBuffer -- Pin a buffer that we know is valid.
 *
 * ---There is a race condition.  This routine doesnt make
 * any sense.  We never really know the buffer is valid.
 */
BufferAcquire(bufHdr)
BufferDesc	*bufHdr;
{

  SpinAcquire(BufMgrLock);
  PinBuffer(bufHdr);
  SpinRelease(BufMgrLock);
  return (TRUE);
}

/*
 * BufferRepin -- get a second pin on an already pinned buffer
 */
BufferRepin(buffer)
Buffer	buffer;
{
  BufferDesc	*bufHdr;

  if (BAD_BUFFER_ID(buffer)) {
    return(FALSE);
  }
  bufHdr = BufferGetBufferDescriptor(buffer);

  /* like we said -- already pinned */
  Assert(bufHdr->refcount);

  SpinAcquire(BufMgrLock);
  PinBuffer(bufHdr);
  SpinRelease(BufMgrLock);
  return (TRUE);
}

/*
 * CleanupBuffers -- sync everything.  This is an error
 * 	handling function.  I currently don't use it.
 */
void
BufferSync()
{ 
  int i;
  BufferDesc *bufHdr;

  for(i=0,bufHdr = BufferDescriptors;
      i<NBuffers;
      i++,bufHdr++) { if (bufHdr->flags & BM_VALID) {
      if(bufHdr->flags & BM_DIRTY) {
	bufHdr->flags &= ~BM_DIRTY;
	(void) BlockReplace(bufHdr);
      }
    }
  }
}


/*
 * WaitIO -- Block until the IO_IN_PROGRESS flag on 'buf'
 * 	is cleared.  Because IO_IN_PROGRESS conflicts are
 *	expected to be rare, there is only one BufferIO
 *	lock in the entire system.  All processes block
 *	on this semaphore when they try to use a buffer
 *	that someone else is faulting in.  Whenever a
 *	process finishes an IO and someone is waiting for
 *	the buffer, BufferIO is signaled (SignalIO).  All
 *	waiting processes then wake up and check to see
 *	if their buffer is now ready.  This implementation
 *	is simple, but efficient enough if WaitIO is
 *	rarely called by multiple processes simultaneously.
 *
 *  ProcSleep atomically releases the spinlock and goes to
 *	sleep.
 *
 *  Note: there is an easy fix if the queue becomes long.
 *	save the id of the buffer we are waiting for in
 *	the queue structure.  That way signal can figure
 *	out which proc to wake up.
 */
#ifdef sequent
WaitIO(buf, spinlock)
BufferDesc *buf;
SPINLOCK spinlock;
{
    SpinRelease(spinlock);
    S_LOCK(&(buf->io_in_progress_lock));
    S_UNLOCK(&(buf->io_in_progress_lock));
    SpinAcquire(spinlock);
}

#else /* sequent */
static IpcSemaphoreId	WaitIOSemId;

WaitIO(buf,spinlock)
BufferDesc *buf;
SPINLOCK spinlock;
{
  Boolean 	inProgress;

  for (;;) {

    /* wait until someone releases IO lock */
    SpinRelease(spinlock);
    IpcSemaphoreLock(WaitIOSemId, 1, 1);
    SpinAcquire(spinlock);
    inProgress = (buf->flags & BM_IO_IN_PROGRESS);
    if (!inProgress) break;
  }
}

/*
 * SignalIO --
 */
SignalIO(buf)
BufferDesc *buf;
{
  int semncnt;
  /* somebody better be waiting. */
  Assert( buf->refcount > 1);
  semncnt = IpcSemaphoreGetCount(WaitIOSemId, 1);
  IpcSemaphoreUnlock(WaitIOSemId, 1, semncnt);
}
#endif /* sequent */

/*
 * Initialize module:
 *
 * should calculate size of pool dynamically based on the
 * amount of available memory.
 */
InitBufferPool(key)
IPCKey key;
{
  Boolean foundBufs,foundDescs;
  int i;
  int status;


  Data_Descriptors = NBuffers;
  Free_List_Descriptor = Data_Descriptors;
  Lookup_List_Descriptor = Data_Descriptors + 1;
  Num_Descriptors = Data_Descriptors + 1;

  SpinAcquire(BufMgrLock);

  BufferDescriptors = (BufferDesc *)
    ShmemInitStruct("Buffer Descriptors",
		    Num_Descriptors*sizeof(BufferDesc),&foundDescs);

  BufferBlocks = (BufferBlock)
    ShmemInitStruct("Buffer Blocks",
		    NBuffers*BLOCK_SIZE,&foundBufs);


  if (foundDescs || foundBufs) {

    /* both should be present or neither */
    Assert(foundDescs && foundBufs);

  } else {
    BufferDesc *buf;
    unsigned int block;

    buf = BufferDescriptors;
    block = (unsigned int) BufferBlocks;

    /*
     * link the buffers into a circular, doubly-linked list to
     * initialize free list.  Still don't know anything about
     * replacement strategy in this file.
     */
    for (i = 0; i < Data_Descriptors; block+=BLOCK_SIZE,buf++,i++) {
      Assert(ShmemIsValid((unsigned int)block));

      buf->freeNext = i+1;
      buf->freePrev = i-1;

      CLEAR_BUFFERTAG(&(buf->tag));
      buf->data = (BufferBlock) block;
      buf->flags = (BM_DELETED | BM_FREE | BM_VALID);
      buf->refcount = 0;
      buf->id = i;
#ifdef sequent
      S_INIT_LOCK(&(buf->io_in_progress_lock));
#endif
    }

    /* close the circular queue */
    BufferDescriptors[0].freePrev = Data_Descriptors-1;
    BufferDescriptors[Data_Descriptors-1].freeNext = 0;
  }

  /* Init the rest of the module */
  InitBufTable();
  InitFreeList(!foundDescs);

  SpinRelease(BufMgrLock);

#ifndef sequent
  WaitIOSemId = IpcSemaphoreCreate(IPCKeyGetWaitIOSemaphoreKey(key),
				   1, IPCProtection, 0, &status);
#endif
}

int NDirectFileRead;	/* some I/O's are direct file access.  bypass bufmgr */
int NDirectFileWrite;   /* e.g., I/O in psort and hashjoin.		     */

void
PrintBufferUsage()
{
	fprintf(stderr, "\t%d blocks read, %d blocks written, buffer hit rate = %.2f%%\n", 
		ReadBufferCount - BufferHitCount + NDirectFileRead,
		BufferFlushCount + NDirectFileWrite,
	       (float)BufferHitCount * 100.0/ReadBufferCount);
}

void
ResetBufferUsage()
{
	BufferHitCount = 0;
	ReadBufferCount = 0;
	BufferFlushCount = 0;
	NDirectFileRead = 0;
	NDirectFileWrite = 0;
}

void
BufferManagerFlush(StableMainMemoryFlag)
int StableMainMemoryFlag;
{
    register int i;
    
    /* XXX
     * the following piece of code is completely bogus, unfortunately it has to
     * be preserved from the past for postgres to work.  what happens is
     * that postgres seldom bothers to release a buffer after it finishes
     * using it.  as a last resort, it has to have the following code to
     * blow away the whole buffer at the end of every transaction, otherwise
     * we will just keep losing buffers until we run out of them.
     * the fix for this is to look at every place buffer manager gets called
     * and carefully watch the use of the buffer pages and release them
     * at proper times. -- Wei
     */
    for (i=1; i<=NBuffers; i++) {
        if (BufferIsValid(i)) {
            WriteBuffer(i); 
            while(BufferIsValid(i))
                ReleaseBuffer(i);
        }
    }
    
    /* flush dirty shared memory only when main memory is not stable */
    /* plai 8/7/90                                                   */
 
    if (!StableMainMemoryFlag)
        BufferSync();
}

/**************************************************
  BufferDescriptorIsValid

 **************************************************/

bool
BufferDescriptorIsValid(bufdesc)
     BufferDesc *bufdesc;
{
    int temp;
    
    Assert(PointerIsValid(bufdesc));
    
    temp = (bufdesc-BufferDescriptors)/sizeof(BufferDesc);
    if (temp >= 0 && temp<NBuffers)
        return(true);
    else
        return(false);
    
} /*BufferDescriptorIsValid*/

/**************************************************
  BufferIsValid
  returns true iff the refcnt of the local
  buffer is > 0
 **************************************************/
bool
BufferIsValid(bufnum)
    Buffer bufnum;
{
    if (BAD_BUFFER_ID(bufnum)) {
        return(false);
    }
    return((bool)(BufferDescriptors[bufnum - 1].refcount > 0));
} /* BufferIsValid */

BlockSize
BufferGetBlockSize(buffer)
    Buffer      buffer;
{
    Assert(BufferIsValid(buffer));
  /* Apparently, POSTGRES was supposed to have variable
   * sized buffer blocks.  Current buffer manager will need
   * extensive redesign if that is ever to come to pass, so
   * for now hardwire it to BLCKSZ
   */
    return (BLCKSZ);
}

BlockNumber
BufferGetBlockNumber(buffer)
    Buffer      buffer;
{
    Assert(BufferIsValid(buffer));
    return (BufferGetBufferDescriptor(buffer)->tag.blockNum);
}

Relation
BufferGetRelation(buffer)
    Buffer      buffer;
{
    Relation    relation;

    Assert(BufferIsValid(buffer));

    relation = RelationIdGetRelation(LRelIdGetRelationId
                (BufferGetBufferDescriptor(buffer)->tag.relId));

    RelationDecrementReferenceCount(relation);

    if (RelationHasReferenceCountZero(relation)) {
        elog(NOTICE, "BufferGetRelation: 0->1");

        RelationIncrementReferenceCount(relation);
    }

    return (relation);
}

/**************************************************
  BufferDescriptorGetBuffer

 **************************************************/

/* INTERNAL */

static Buffer
BufferDescriptorGetBuffer(descriptor)
    BufferDesc *descriptor;
{
    
    if (BufferDescriptorIsValid(descriptor))
        return(1+descriptor - BufferDescriptors);

    elog(WARN,"Invalid bufferDescriptor\n");
    return (-1);
}

void
IncrBufferRefCount(buffer)
Buffer buffer;
{
    SpinAcquire(BufMgrLock);
    BufferDescriptors[buffer - 1].refcount++;
    SpinRelease(BufMgrLock);
}

/**************************************************
  BufferPut 
  XXX -  this is is old cruft rehacked so it will
  coninue to behave in a predictably buggy version.
  Done under extreme protest. -jeff goh

  Remove ASAP.

  Unfortunately, this function is called everywhere,
  so it still gets preversed from the past. -- Wei
 **************************************************/

ReturnStatus
BufferPut(buffer,lockLevel)
    Buffer buffer;
    BufFlags lockLevel;
    
{
    Assert(BufferIsValid(buffer));
    
    if (lockLevel & L_UN) {
        switch(lockLevel & L_LOCKS) {
        case L_PIN:
#ifndef NO_BUFFERISVALID
            if (!BufferIsValid(buffer))
                elog(WARN,"BufferPut called with invalid buffer");
#endif
            ReleaseBuffer(buffer);
            return(0);
            
        case L_LOCKS:
#ifndef NO_BUFFERISVALID
            if (!BufferIsValid(buffer))
                elog(WARN,"BufferPut called with invalid buffer");
#endif
            IncrBufferRefCount(buffer);
            WriteBuffer(buffer);
            return(0);
            
        default:
            if (lockLevel & L_WRITE) 
                WriteBuffer(buffer);
            else
                ReleaseBuffer(buffer);
            return(0);
        }
    }
    
    switch(lockLevel & L_LOCKS) {
    case L_SH:
        if (lockLevel & L_WRITE) {
            IncrBufferRefCount(buffer);
            WriteBuffer(buffer);
        }
        break;
    case L_EX:
        if (lockLevel & L_WRITE) {
            IncrBufferRefCount(buffer);
            WriteBuffer(buffer);
        }
        break;
    case L_UP:
        if (lockLevel & L_WRITE) {
            IncrBufferRefCount(buffer);
            WriteBuffer(buffer);
        }
        break;
    case L_PIN:
#ifndef NO_BUFFERISVALID
        if (!BufferIsValid(buffer))
          elog(WARN,"BufferPut: trying to pin invalid buffer");
#endif
        if (lockLevel & L_WRITE) {
            IncrBufferRefCount(buffer);
            WriteBuffer(buffer);
        }
        break;
    case L_DUP:
        Assert(BufferIsValid(buffer));
        IncrBufferRefCount(buffer);
        break;
    default:
        break;
    }

    return(0);
}

/**************************************************
 * RelationGetBuffer --
 *      Gets the buffer number of a given disk block.
 *
 *      Returns InvalidBuffer on error.  Currently calls elog() instead.
 **************************************************
 */

Buffer
RelationGetBuffer(relation, blockNumber, lockLevel)
     Relation    relation;              /* relation */
     BlockNumber blockNumber;   /* block number */
     BufferLock  lockLevel;             /* lock level */

{
    return(ReadBuffer(relation,blockNumber));
}


/**************************************************
  BufferIsDirty

 **************************************************/

bool
BufferIsDirty(buffer)
    Buffer buffer;
{
    return (bool)
        (BufferGetBufferDescriptor(buffer)->flags & BM_DIRTY);
}


/**************************************************
  BufferIsInvalid

 **************************************************/
bool
BufferIsInvalid(buffer)
        Buffer  buffer;
{
    return (bool)
        (buffer == InvalidBuffer);
}


/**************************************************
  BufferIsUnknown

 **************************************************/
bool
BufferIsUnknown(buffer)
    Buffer      buffer;
{
    return (bool)
        (buffer == UnknownBuffer);
}

/***************************************************
 * RelationGetNumberOfPages --
 *      Returns number of pages in an open relation.
 *
 * Note:
 *      XXX may fail for huge relations.
 *      XXX should be elsewhere.
 *      XXX maybe should be hidden
 ***************************************************
 */

BlockNumber
RelationGetNumberOfBlocks(relation)
Relation        relation;
{
     return(FileGetNumberOfBlocks(relation->rd_fd));
}

/**************************************************
  BufferGetBlock

 **************************************************/

Block
BufferGetBlock(buffer)
        Buffer  buffer;
{
    Assert(BufferIsValid(buffer));

    return((Block)(BufferDescriptors[buffer-1].data));
}

/* ---------------------------------------------------------------------
 *      ReleaseTmpRelBuffers
 *
 *      this function unmarks all the dirty pages of a temporary
 *      relation in the buffer pool so that at the end of transaction
 *      these pages will not be flushed.
 *      XXX currently it sequentially searches the buffer pool, should be
 *      changed to more clever ways of searching.
 * --------------------------------------------------------------------
 */
void
ReleaseTmpRelBuffers(tempreldesc)
Relation tempreldesc;
{
    register int i;
    BufferDesc *buf;

    for (i=1; i<=NBuffers; i++) {
	buf = BufferGetBufferDescriptor(i);
        if (BufferIsDirty(i) &&
            (buf->tag.relId.relId == tempreldesc->rd_id)) {
            buf->flags &= ~BM_DIRTY;
            if (!(buf->flags & BM_FREE))
               ReleaseBuffer(i);
        }
     }
}

/* -----------------------------------------------------------------
 *	PrintBufferDescs
 *
 *	this function prints all the buffer descriptors, for debugging
 *	use only.
 * -----------------------------------------------------------------
 */

void
PrintBufferDescs()
{
    register int i;
    BufferDesc *buf;

    for (i=0; i<NBuffers; i++) {
	buf = &(BufferDescriptors[i]);
	printf("(freeNext=%d, freePrev=%d, relname=%s, blockNum=%d, flags=0x%x, refcount=%d)\n", buf->freeNext, buf->freePrev, &(buf->sb_relname), buf->tag.blockNum, buf->flags, buf->refcount);
     }
}

#define MAX(X,Y)	((X) > (Y) ? (X) : (Y))

/* -----------------------------------------------------
 * BufferShmemSize
 *
 * compute the size of shared memory for the buffer pool including
 * data pages, buffer descriptors, hash tables, etc.
 * ----------------------------------------------------
 */

int
BufferShmemSize()
{
    int size;
    int nbuckets;
    int nsegs;

    nbuckets = 1 << log2((NBuffers - 1) / DEF_FFACTOR + 1);
    nsegs = 1 << log2((nbuckets - 1) / DEF_SEGSIZE + 1);
    size =  /* size of shmem binding table */
	    log2(BTABLE_SIZE) + sizeof(HHDR)
	    + DEF_SEGSIZE * sizeof(SEGMENT) + BUCKET_ALLOC_INCR * 
	    (sizeof(BUCKET_INDEX) + BTABLE_KEYSIZE + BTABLE_DATASIZE)
 	    /* size of buffer descriptors */
            + (NBuffers + 1) * sizeof(BufferDesc)
	    /* size of data pages */
            + NBuffers * BLOCK_SIZE
	    /* size of buffer hash table */
            + log2(NBuffers) + sizeof(HHDR)
	    + nsegs * DEF_SEGSIZE * sizeof(SEGMENT) 
	    + (int)ceil((double)NBuffers/BUCKET_ALLOC_INCR)*BUCKET_ALLOC_INCR * 
	    (sizeof(BUCKET_INDEX) + sizeof(BufferTag) + sizeof(Buffer))
	    /* extra space, just to make sure there is enough  */
            + NBuffers * 4 + 4096;
    return size;
}

/*
 * BufferPoolBlowaway
 *
 * this routine is solely for the purpose of experiments -- sometimes
 * you may want to blowaway whatever is left from the past in buffer
 * pool and start measuring some performance with a clean empty buffer
 * pool.
 */
void
BufferPoolBlowaway()
{
    register int i;
    
    BufferSync();
    for (i=1; i<=NBuffers; i++) {
        if (BufferIsValid(i)) {
            while(BufferIsValid(i))
                ReleaseBuffer(i);
	    BufTableDelete(BufferGetBufferDescriptor(i));
        }
    }
}
