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
 * DirtyBufferCopy() -- For a given dbid/relid/blockno, if the buffer is
 *			in the cache and is dirty, mark it clean and copy
 *			it to the requested location.  This is a logical
 *			write, and has been installed to support the cache
 *			management code for write-once storage managers.
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
#include <math.h>
#include <signal.h>

#include "storage/buf_internals.h"
#include "storage/bufmgr.h"

#include "storage/fd.h"
#include "storage/ipc.h"
#include "storage/ipci.h"
#include "storage/shmem.h"
#include "storage/spin.h"
#include "storage/smgr.h"
#include "storage/lmgr.h"
#include "tmp/miscadmin.h"
#include "utils/hsearch.h"
#include "utils/log.h"
#include "utils/memutils.h"

/*
 *  if BMTRACE is defined, we trace the last 200 buffer allocations and
 *  deallocations in a circular buffer in shared memory.
 */
#ifdef	BMTRACE
bmtrace	*TraceBuf;
long	*CurTraceBuf;
#define	BMT_LIMIT	200
#endif /* BMTRACE */

int		NBuffers = NDBUFS;  /* NDBUFS defined in miscadmin.h */
int		Data_Descriptors;
int		Free_List_Descriptor;
int		Lookup_List_Descriptor;
int		Num_Descriptors;

BufferDesc 	*BufferDescriptors;
BufferBlock 	BufferBlocks;
#ifndef HAS_TEST_AND_SET
static long	*NWaitIOBackendP;
#endif

long	*PrivateRefCount;	/* also used in freelist.c */
long	*LastRefCount;  /* refcounts of last ExecMain level */

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
 * PrivateRefCount -- Each buffer also has a private refcount the keeps
 *	track of the number of times the buffer is pinned in the current
 *	processes.  This is used for two purposes, first, if we pin a
 *	a buffer more than once, we only need to change the shared refcount
 *	once, thus only lock the buffer pool once, second, when a transaction
 *	aborts, it should only unpin the buffers exactly the number of times it
 *	has pinned them, so that it will not blow away buffers of another
 *	backend.
 *
 */

SPINLOCK BufMgrLock;

/* delayed write: TRUE on, FALSE off */
int LateWrite = TRUE;

int ReadBufferCount;
int BufferHitCount;
int BufferFlushCount;

/* ---------------------------------------------------
 * RelationGetBufferWithBuffer
 *	see if the given buffer is what we want
 *	if yes, we don't need to bother the buffer manager
 * ---------------------------------------------------
 */
Buffer
RelationGetBufferWithBuffer(relation, blockNumber, buffer)
Relation relation;
BlockNumber blockNumber;
Buffer buffer;
{
    BufferDesc *bufHdr;
    LRelId lrelId;

    if (BufferIsValid(buffer)) {
        bufHdr = BufferGetBufferDescriptor(buffer);
	lrelId = RelationGetLRelId(relation);
	if (bufHdr->tag.blockNum == blockNumber &&
	    bufHdr->tag.relId.relId == lrelId.relId &&
	    bufHdr->tag.relId.dbId == lrelId.dbId)
	    return buffer;
      }
    return(ReadBuffer(relation,blockNumber));
}

/*
 * ReadBuffer -- returns a buffer containing the requested
 *	block of the requested relation.  If the blknum
 *	requested is NEW_BLOCK, extend the relation file and
 *	allocate a new block.
 *
 * Returns: the buffer number for the buffer containing
 *	the block read or NULL on an error.
 *
 * Assume when this function is called, that reln has been
 *	opened already.
 */

extern int ShowPinTrace;
#undef ReadBuffer

Buffer
ReadBuffer(reln, blockNum)
Relation	reln;
BlockNumber	blockNum;
{
    return ReadBufferWithBufferLock(reln, blockNum, false);
}

bool
is_userbuffer(buffer)
Buffer buffer;
{
    BufferDesc *buf = BufferGetBufferDescriptor(buffer);

    if (issystem(&buf->sb_relname))
	return false;
    return true;
}

Buffer
ReadBuffer_Debug(file, line, reln, blockNum)
String file;
int line;
Relation reln;
BlockNumber blockNum;
{
    Buffer buffer;

    buffer = ReadBufferWithBufferLock(reln, blockNum, false);
    if (ShowPinTrace && is_userbuffer(buffer)) {
	BufferDesc *buf;
	buf = BufferGetBufferDescriptor(buffer);
	fprintf(stderr, "PIN(RD) %d relname = %.*s, blockNum = %d, \
refcount = %d, file: %s, line: %d\n",
		buffer, NAMEDATALEN, &(buf->sb_relname), buf->tag.blockNum,
		PrivateRefCount[buffer - 1], file, line);
    }
    return buffer;
}

/*
 * ReadBufferWithBufferLock -- does the work of 
 *	ReadBuffer() but with the possibility that
 *	the buffer lock has already been held. this
 *	is yet another effort to reduce the number of
 *	semops in the system.
 *
 *  This routine locks the buffer pool before calling BufferAlloc to
 *  avoid two semops.
 */

Buffer
ReadBufferWithBufferLock(reln,blockNum, bufferLockHeld)
Relation 	reln;
BlockNumber 	blockNum;
bool		bufferLockHeld;
{
  BufferDesc *	bufHdr;	  
  int		extend;   /* extending the file by one block */
  int		status;
  Boolean	found;

  ReadBufferCount++;
  extend = (blockNum == NEW_BLOCK);
  /* lookup the buffer.  IO_IN_PROGRESS is set if the requested
   * block is not currently in memory.
   */
  bufHdr = BufferAlloc(reln, blockNum, &found, bufferLockHeld);
  if (! bufHdr) {
    return(InvalidBuffer);
  }

  /* if its already in the buffer pool, we're done */
  if (found) {
    /*
     * This happens when a bogus buffer was returned previously and is
     * floating around in the buffer pool.  A routine calling this would
     * want this extended.
     */
    if (extend) {
      /* new buffers are zero-filled */
      (void) bzero((char *) MAKE_PTR(bufHdr->data), BLCKSZ);
      (void) smgrextend(bufHdr->bufsmgr, reln,
			(char *) MAKE_PTR(bufHdr->data));
    }
    BufferHitCount++;
    return(BufferDescriptorGetBuffer(bufHdr));
  }

  /* 
   * if we have gotten to this point, the reln pointer must be ok
   * and the relation file must be open.
   */

  if (extend) {
    /* new buffers are zero-filled */
    (void) bzero((char *) MAKE_PTR(bufHdr->data), BLCKSZ);
    status = smgrextend(bufHdr->bufsmgr, reln,
			(char *) MAKE_PTR(bufHdr->data));
  } else {
    status = smgrread(bufHdr->bufsmgr, reln, blockNum,
		      (char *) MAKE_PTR(bufHdr->data));
  }

  /* lock buffer manager again to update IO IN PROGRESS */
  SpinAcquire(BufMgrLock);

  if (status == SM_FAIL) {
    /* IO Failed.  cleanup the data structures and go home */

    if (! BufTableDelete(bufHdr)) {
      SpinRelease(BufMgrLock);
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
#ifdef HAS_TEST_AND_SET
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
BufferAlloc(reln, blockNum, foundPtr, bufferLockHeld)
Relation	reln;
BlockNumber	blockNum;
Boolean		*foundPtr;
bool		bufferLockHeld;
{
  BufferDesc 		*buf;	  
  BufferTag 		newTag;	 /* identity of requested block */
  Boolean		inProgress; /* buffer undergoing IO */
  int			status;
  Boolean		newblock = FALSE;
  BufferDesc		oldbufdesc;

    /* create a new tag so we can lookup the buffer */
    /* assume that the relation is already open */
  if (blockNum == NEW_BLOCK) {
      newblock = TRUE;
      blockNum = smgrnblocks(reln->rd_rel->relsmgr, reln);
  }

  INIT_BUFFERTAG(&newTag,reln,blockNum);

  if (!bufferLockHeld)
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
#ifdef BMTRACE
    _bm_trace((reln->rd_rel->relisshared ? 0 : MyDatabaseId), reln->rd_id, blockNum, BufferDescriptorGetBuffer(buf), BMT_ALLOCFND);
#endif BMTRACE

    SpinRelease(BufMgrLock);
  
    return(buf);
  }

  *foundPtr = FALSE;

  /*
   * Didn't find it in the buffer pool.  We'll have
   * to initialize a new buffer.  First, grab one from
   * the free list.  If it's dirty, flush it to disk.
   * Remember to unlock BufMgr spinlock while doing the IOs.
   */
  inProgress = FALSE;
  for (buf = (BufferDesc *) NULL; buf == (BufferDesc *) NULL; ) {
      buf = GetFreeBuffer();
      if (! buf) {
	  /* out of free buffers.  In trouble now. */
	  /* XXX actually, GetFreeBuffer already aborted us... */
	  SpinRelease(BufMgrLock);
	  return((BufferDesc *) NULL);
      }
      
      /*
       * There should be exactly one pin on the buffer after
       * it is allocated -- ours.  If it had a pin it wouldn't
       * have been on the free list.  No one else could have
       * pinned it between GetFreeBuffer and here because we
       * have the BufMgrLock.
       */
      Assert(buf->refcount == 0);
      buf->refcount = 1;	       
      PrivateRefCount[BufferDescriptorGetBuffer(buf) - 1] = 1;
      
      if (buf->flags & BM_DIRTY) {
	  /*
	   * Set BM_IO_IN_PROGRESS to keep anyone from doing anything
	   * with the contents of the buffer while we write it out.
	   * We don't really care if they try to read it, but if they
	   * can complete a BufferAlloc on it they can then scribble
	   * into it, and we'd really like to avoid that while we are
	   * flushing the buffer.  Setting this flag should block them
	   * in WaitIO until we're done.
	   */
	  inProgress = TRUE;
	  buf->flags |= BM_IO_IN_PROGRESS; 
#ifdef HAS_TEST_AND_SET
	  /*
	   * All code paths that acquire this lock pin the buffer
	   * first; since no one had it pinned (it just came off the
	   * free list), no one else can have this lock.
	   */
	  Assert(S_LOCK_FREE(&(buf->io_in_progress_lock)));
	  S_LOCK(&(buf->io_in_progress_lock));
#endif /* HAS_TEST_AND_SET */
	  
	  /*
	   * Write the buffer out, being careful to release BufMgrLock
	   * before starting the I/O.
	   *
	   * This #ifndef is here because a few extra semops REALLY kill
	   * you on machines that don't have spinlocks.  If you don't
	   * operate with much concurrency, well...
	   */
#ifndef OPTIMIZE_SINGLE
	  SpinRelease(BufMgrLock);
#endif /* OPTIMIZE_SINGLE */
	  (void) BufferReplace(buf);
	  BufferFlushCount++;
#ifndef OPTIMIZE_SINGLE
	  SpinAcquire(BufMgrLock);
#endif /* OPTIMIZE_SINGLE */

	  /*
	   * Somebody could have pinned the buffer while we were
	   * doing the I/O and had given up the BufMgrLock (though
	   * they would be waiting for us to clear the BM_IO_IN_PROGRESS
	   * flag).  That's why this is a loop -- if so, we need to clear
	   * the I/O flags, remove our pin and start all over again.
	   *
	   * People may be making buffers free at any time, so there's
	   * no reason to think that we have an immediate disaster on
	   * our hands.
	   */
	  if (buf->refcount > 1) {
	      inProgress = FALSE;
	      buf->flags &= ~BM_IO_IN_PROGRESS;
#ifdef HAS_TEST_AND_SET
	      S_UNLOCK(&(buf->io_in_progress_lock));
#else /* !HAS_TEST_AND_SET */
	      if (buf->refcount > 1)
		  SignalIO(buf);
#endif /* !HAS_TEST_AND_SET */
	      PrivateRefCount[BufferDescriptorGetBuffer(buf) - 1] = 0;
	      buf->refcount--;
	      buf = (BufferDesc *) NULL;
	  }
      }
  }

  /*
   * At this point we should have the sole pin on a non-dirty
   * buffer and we may or may not already have the BM_IO_IN_PROGRESS
   * flag set.
   */

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
    SpinRelease(BufMgrLock);
    elog(FATAL,"buffer wasn't in the buffer table\n");
  }

  if (buf->flags & BM_DIRTY) {
      /* must clear flag first because of wierd race 
       * condition described below.  
       */
      buf->flags &= ~BM_DIRTY;
  }

  /* record the database name and relation name for this buffer */
  (void) namecpy(&(buf->sb_relname), &(reln->rd_rel->relname));
  (void) namecpy(&(buf->sb_dbname), MyDatabaseName);

  /* remember which storage manager is responsible for it */
  buf->bufsmgr = reln->rd_rel->relsmgr;

  INIT_BUFFERTAG(&(buf->tag),reln,blockNum);
  if (! BufTableInsert(buf)) {
    SpinRelease(BufMgrLock);
    elog(FATAL,"Buffer in lookup table twice \n");
  } 

  /* Buffer contents are currently invalid.  Have
   * to mark IO IN PROGRESS so no one fiddles with
   * them until the read completes.  If this routine
   * has been called simply to allocate a buffer, no
   * io will be attempted, so the flag isnt set.
   */
  if (!inProgress) {
      buf->flags |= BM_IO_IN_PROGRESS; 
#ifdef HAS_TEST_AND_SET
      Assert(S_LOCK_FREE(&(buf->io_in_progress_lock)));
      S_LOCK(&(buf->io_in_progress_lock));
#endif /* HAS_TEST_AND_SET */
  }

#ifdef BMTRACE
  _bm_trace((reln->rd_rel->relisshared ? 0 : MyDatabaseId), reln->rd_id, blockNum, BufferDescriptorGetBuffer(buf), BMT_ALLOCNOTFND);
#endif BMTRACE

  SpinRelease(BufMgrLock);

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

#undef WriteBuffer

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

WriteBuffer_Debug(file, line, buffer)
String file;
int line;
Buffer buffer;
{
    WriteBuffer(buffer);
    if (ShowPinTrace && is_userbuffer(buffer)) {
	BufferDesc *buf;
	buf = BufferGetBufferDescriptor(buffer);
	fprintf(stderr, "UNPIN(WR) %d relname = %.*s, blockNum = %d, \
refcount = %d, file: %s, line: %d\n",
		buffer, NAMEDATALEN, &(buf->sb_relname), buf->tag.blockNum,
		PrivateRefCount[buffer - 1], file, line);
    }
}

/*
 *  DirtyBufferCopy() -- Copy a given dirty buffer to the requested
 *			 destination.
 *
 *	We treat this as a write.  If the requested buffer is in the pool
 *	and is dirty, we copy it to the location requested and mark it
 *	clean.  This routine supports the Sony jukebox storage manager,
 *	which agrees to take responsibility for the data once we mark
 *	it clean.
 */

DirtyBufferCopy(dbid, relid, blkno, dest)
  ObjectId dbid;
  ObjectId relid;
  BlockNumber blkno;
  char *dest;
{
  BufferDesc *buf;
  BufferTag btag;

  btag.relId.relId = relid;
  btag.relId.dbId = dbid;
  btag.blockNum = blkno;

  SpinAcquire(BufMgrLock);
  buf = BufTableLookup(&btag);

  if (buf == (BufferDesc *) NULL
      || !(buf->flags & BM_DIRTY)
      || !(buf->flags & BM_VALID)) {
    SpinRelease(BufMgrLock);
    return;
  }

  /* hate to do this holding the lock, but release and reacquire is slower */
  (void) bcopy((char *) MAKE_PTR(buf->data), dest, BLCKSZ);

  buf->flags &= ~BM_DIRTY;

  SpinRelease(BufMgrLock);
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
    BufferReplace(bufHdr);
  }


  return(STATUS_OK);
} 


/*
 * FlushBuffer -- like WriteBuffer, but force the page to disk.
 */
FlushBuffer(buffer)
Buffer	buffer;
{
  BufferDesc	*bufHdr;
  OID		bufdb;
  OID		bufrel;
  Relation	reln;
  int		status;


  if (BAD_BUFFER_ID(buffer)) {
    return(STATUS_ERROR);
  }

  bufHdr = BufferGetBufferDescriptor(buffer);

  /*
   *  If the relation is not in our private cache, we don't bother trying
   *  to instantiate it.  Instead, we call the storage manager routine that
   *  does a blind write.  If we can get the reldesc, then we use the standard
   *  write routine interface.
   */

  bufdb = bufHdr->tag.relId.dbId;
  bufrel = bufHdr->tag.relId.relId;

  if (bufdb == MyDatabaseId || bufdb == (OID) NULL)
      reln = RelationIdCacheGetRelation(bufrel);
  else
      reln = (Relation) NULL;

  if (reln != (Relation) NULL) {
      status = smgrflush(bufHdr->bufsmgr, reln, bufHdr->tag.blockNum,
			 (char *) MAKE_PTR(bufHdr->data));
  } else {

      /* blind write always flushes */
      status = smgrblindwrt(bufHdr->bufsmgr, &bufHdr->sb_dbname,
			    &bufHdr->sb_relname, bufdb, bufrel,
			    bufHdr->tag.blockNum,
			    (char *) MAKE_PTR(bufHdr->data));
  }

  if (status == SM_FAIL) {
      elog(WARN, "FlushBuffer: cannot flush %d for %16s", bufHdr->tag.blockNum,
		 reln->rd_rel->relname);
      /* NOTREACHED */
      return (STATUS_ERROR);
  }

  SpinAcquire(BufMgrLock);
  bufHdr->flags &= ~BM_DIRTY; 
  UnpinBuffer(bufHdr);
  SpinRelease(BufMgrLock);

  return(STATUS_OK);
}

/*
 * WriteNoReleaseBuffer -- like WriteBuffer, but do not unpin the buffer
 * 			   when the operation is complete.
 *
 *	We know that the buffer is for a relation in our private cache,
 *	because this routine is called only to write out buffers that
 *	were changed by the executing backend.
 */

WriteNoReleaseBuffer(buffer)
Buffer	buffer;
{
  BufferDesc	*bufHdr;
  Relation	reln;

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


#undef ReleaseAndReadBuffer
/*
 * ReleaseAndReadBuffer -- combine ReleaseBuffer() and ReadBuffer()
 * 	so that only one semop needs to be called.
 *
 */
Buffer
ReleaseAndReadBuffer(buffer, relation, blockNum)
Buffer buffer;
Relation relation;
BlockNumber blockNum;
{
    BufferDesc	*bufHdr;
    Buffer retbuf;
    if (BufferIsValid(buffer)) {
	bufHdr = BufferGetBufferDescriptor(buffer);
	Assert(PrivateRefCount[buffer - 1] > 0);
	PrivateRefCount[buffer - 1]--;
	if (PrivateRefCount[buffer - 1] == 0 &&
	    LastRefCount[buffer - 1] == 0) {
	/* only release buffer if it is not pinned in previous ExecMain level */
	    SpinAcquire(BufMgrLock);
	    bufHdr->refcount--;
	    if (bufHdr->refcount == 0) {
		AddBufferToFreelist(bufHdr);
		bufHdr->flags |= BM_FREE;
	      }
	    retbuf = ReadBufferWithBufferLock(relation, blockNum, true);
	    return retbuf;
	 }
      }
    return(ReadBuffer(relation, blockNum));
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
 * BufferSync -- Flush all dirty buffers in the pool.
 *
 *	This is called at transaction commit time.  It does the wrong thing,
 *	right now.  We should flush only our own changes to stable storage,
 *	and we should obey the lock protocol on the buffer manager metadata
 *	as we do it.  Also, we need to be sure that no other transaction is
 *	modifying the page as we flush it.  This is only a problem for objects
 *	that use a non-two-phase locking protocol, like btree indices.  For
 *	those objects, we would like to set a write lock for the duration of
 *	our IO.  Another possibility is to code updates to btree pages
 *	carefully, so that writing them out out of order cannot cause
 *	any unrecoverable errors.
 *
 *	I don't want to think hard about this right now, so I will try
 *	to come back to it later.
 */
void
BufferSync()
{ 
  int i;
  OID bufdb;
  OID bufrel;
  Relation reln;
  BufferDesc *bufHdr;
  int status;

  for (i=0, bufHdr = BufferDescriptors; i < NBuffers; i++, bufHdr++) {
      if ((bufHdr->flags & BM_VALID) && (bufHdr->flags & BM_DIRTY)) {
	  bufdb = bufHdr->tag.relId.dbId;
	  bufrel = bufHdr->tag.relId.relId;
	  if (bufdb == MyDatabaseId || bufdb == (OID) 0) {
	      reln = RelationIdCacheGetRelation(bufrel);

	      /*
	       *  If we didn't have the reldesc in our local cache, flush this
	       *  page out using the 'blind write' storage manager routine.  If
	       *  we did find it, use the standard interface.
	       */

	      if (reln == (Relation) NULL) {
		  status = smgrblindwrt(bufHdr->bufsmgr, &bufHdr->sb_dbname,
					&bufHdr->sb_relname, bufdb, bufrel,
					bufHdr->tag.blockNum,
					(char *) MAKE_PTR(bufHdr->data));
	      } else {
		  status = smgrwrite(bufHdr->bufsmgr, reln,
				     bufHdr->tag.blockNum,
				     (char *) MAKE_PTR(bufHdr->data));
	      }

	      if (status == SM_FAIL) {
		  elog(WARN, "cannot write %d for %16s",
		       bufHdr->tag.blockNum, bufHdr->sb_relname);
	      }

	      bufHdr->flags &= ~BM_DIRTY;
	      if (reln != (Relation)NULL)
		  RelationDecrementReferenceCount(reln);
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
#ifdef HAS_TEST_AND_SET
WaitIO(buf, spinlock)
BufferDesc *buf;
SPINLOCK spinlock;
{
    SpinRelease(spinlock);
    S_LOCK(&(buf->io_in_progress_lock));
    S_UNLOCK(&(buf->io_in_progress_lock));
    SpinAcquire(spinlock);
}

#else /* HAS_TEST_AND_SET */
static IpcSemaphoreId	WaitIOSemId;

WaitIO(buf,spinlock)
BufferDesc *buf;
SPINLOCK spinlock;
{
  Boolean 	inProgress;

  for (;;) {

    /* wait until someone releases IO lock */
    (*NWaitIOBackendP)++;
    SpinRelease(spinlock);
    IpcSemaphoreLock(WaitIOSemId, 0, 1);
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
  /* somebody better be waiting. */
  Assert( buf->refcount > 1);
  IpcSemaphoreUnlock(WaitIOSemId, 0, *NWaitIOBackendP);
  *NWaitIOBackendP = 0;
}
#endif /* HAS_TEST_AND_SET */

/*
 * Initialize module:
 *
 * should calculate size of pool dynamically based on the
 * amount of available memory.
 */
InitBufferPool(key)
IPCKey key;
{
  Boolean foundBufs,foundDescs,foundNWaitIO;
  int i;
  int status;


  Data_Descriptors = NBuffers;
  Free_List_Descriptor = Data_Descriptors;
  Lookup_List_Descriptor = Data_Descriptors + 1;
  Num_Descriptors = Data_Descriptors + 1;

  SpinAcquire(BufMgrLock);

#ifdef BMTRACE
  CurTraceBuf = (long *) ShmemInitStruct("Buffer trace",
				(BMT_LIMIT * sizeof(bmtrace)) + sizeof(long),
				&foundDescs);
  if (!foundDescs)
      bzero(CurTraceBuf, (BMT_LIMIT * sizeof(bmtrace)) + sizeof(long));

  TraceBuf = (bmtrace *) &(CurTraceBuf[1]);
#endif

  BufferDescriptors = (BufferDesc *)
    ShmemInitStruct("Buffer Descriptors",
		    Num_Descriptors*sizeof(BufferDesc),&foundDescs);

  BufferBlocks = (BufferBlock)
    ShmemInitStruct("Buffer Blocks",
		    NBuffers*BLOCK_SIZE,&foundBufs);

#ifndef HAS_TEST_AND_SET
  NWaitIOBackendP = (long *)ShmemInitStruct("#Backends Waiting IO",
					  sizeof(long),
					  &foundNWaitIO);
  if (!foundNWaitIO)
      *NWaitIOBackendP = 0;
#endif

  if (foundDescs || foundBufs) {

    /* both should be present or neither */
    Assert(foundDescs && foundBufs);

  } else {
    BufferDesc *buf;
    unsigned long block;

    buf = BufferDescriptors;
    block = (unsigned long) BufferBlocks;

    /*
     * link the buffers into a circular, doubly-linked list to
     * initialize free list.  Still don't know anything about
     * replacement strategy in this file.
     */
    for (i = 0; i < Data_Descriptors; block+=BLOCK_SIZE,buf++,i++) {
      Assert(ShmemIsValid((unsigned long)block));

      buf->freeNext = i+1;
      buf->freePrev = i-1;

      CLEAR_BUFFERTAG(&(buf->tag));
      buf->data = MAKE_OFFSET(block);
      buf->flags = (BM_DELETED | BM_FREE | BM_VALID);
      buf->refcount = 0;
      buf->id = i;
#ifdef HAS_TEST_AND_SET
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

#ifndef HAS_TEST_AND_SET
  WaitIOSemId = IpcSemaphoreCreate(IPCKeyGetWaitIOSemaphoreKey(key),
				   1, IPCProtection, 0, &status);
#endif
  PrivateRefCount = (long *) calloc(NBuffers, sizeof(long));
  LastRefCount = (long *) calloc(NBuffers, sizeof(long));
}

long NDirectFileRead;	/* some I/O's are direct file access.  bypass bufmgr */
long NDirectFileWrite;   /* e.g., I/O in psort and hashjoin.		     */

void
PrintBufferUsage(statfp)
FILE *statfp;
{
	float hitrate;

	if (ReadBufferCount==0)
	    hitrate = 0.0;
	else
	    hitrate = (float)BufferHitCount * 100.0/ReadBufferCount;

	fprintf(statfp, "!\t%d blocks read, %d blocks written, buffer hit rate = %.2f%%\n", 
		ReadBufferCount - BufferHitCount + NDirectFileRead,
		BufferFlushCount + NDirectFileWrite,
		hitrate);
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

/* ----------------------------------------------
 *	ResetBufferPool
 *
 *	this routine is supposed to be called when a transaction aborts.
 *	it will release all the buffer pins held by the transaciton.
 *
 * ----------------------------------------------
 */
void
ResetBufferPool()
{
    register int i;
    for (i=1; i<=NBuffers; i++) {
	if (BufferIsValid(i)) {
	    while(PrivateRefCount[i - 1] > 0) {
		ReleaseBuffer(i);
	      }
	  }
	LastRefCount[i - 1] = 0;
      }
}

/* -----------------------------------------------
 *	BufferPoolCheckLeak
 *
 *	check if there is buffer leak
 *
 * -----------------------------------------------
 */
int
BufferPoolCheckLeak()
{
    register int i;
    void PrintBufferDescs();

    for (i = 1; i <= NBuffers; i++) {
	if (BufferIsValid(i)) {
	    elog(NOTICE, "buffer leak detected in BufferPoolCheckLeak()");
	    PrintBufferDescs();
	    return(1);
	}
      }
    return(0);
}

/* ------------------------------------------------
 *	FlushBufferPool
 *
 *	flush all dirty blocks in buffer pool to disk
 *
 * ------------------------------------------------
 */
void
FlushBufferPool(StableMainMemoryFlag)
int StableMainMemoryFlag;
{
    if (!StableMainMemoryFlag) {
        BufferSync();
	smgrcommit();
    }
}

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
    return((bool)(PrivateRefCount[bufnum - 1] > 0));
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
       /*
        elog(NOTICE, "BufferGetRelation: 0->1");
	*/

        RelationIncrementReferenceCount(relation);
    }

    return (relation);
}

#ifndef NO_ASSERT_CHECKING

/**************************************************
  BufferGetBufferDescriptor
  BufferDescriptorGetBuffer

 **************************************************/

BufferDesc *
BufferGetBufferDescriptor(buffer)
    Buffer buffer;
{
    Assert(!BAD_BUFFER_ID(buffer));

    return(&BufferDescriptors[buffer-1]);
}

Buffer
BufferDescriptorGetBuffer(descriptor)
    BufferDesc *descriptor;
{
    Buffer buffer = 1 + (descriptor - BufferDescriptors);

    Assert(!BAD_BUFFER_ID(buffer));

    return(buffer);
}

#endif /* !NO_ASSERT_CHECKING */

BufferReplace(bufHdr)
    BufferDesc 	*bufHdr;
{
    int		blockSize;
    int		blockNum;
    LRelId	*relIdPtr;
    Relation 	reln;
    ObjectId	bufdb, bufrel;
    int		status;

    blockSize = BLOCKSZ(bufHdr);
    blockNum = bufHdr->tag.blockNum;

    /*
     * first try to find the reldesc in the cache, if no luck,
     * don't bother to build the reldesc from scratch, just do
     * a blind write.
     */

    bufdb = bufHdr->tag.relId.dbId;
    bufrel = bufHdr->tag.relId.relId;

    if (bufdb == MyDatabaseId || bufdb == (OID) NULL)
	reln = RelationIdCacheGetRelation(bufrel);
    else
	reln = (Relation) NULL;

    if (reln != (Relation) NULL) {
	status = smgrflush(bufHdr->bufsmgr, reln, bufHdr->tag.blockNum,
			   (char *) MAKE_PTR(bufHdr->data));
    } else {

	/* blind write always flushes */
	status = smgrblindwrt(bufHdr->bufsmgr, &bufHdr->sb_dbname,
			      &bufHdr->sb_relname, bufdb, bufrel,
			      bufHdr->tag.blockNum,
			      (char *) MAKE_PTR(bufHdr->data));
    }

    if (status == SM_FAIL)
	return (FALSE);

    return (TRUE);
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
     return (smgrnblocks(relation->rd_rel->relsmgr, relation));
}

/**************************************************
  BufferGetBlock

 **************************************************/

Block
BufferGetBlock(buffer)
        Buffer  buffer;
{
    Assert(BufferIsValid(buffer));

    return((Block)MAKE_PTR(BufferDescriptors[buffer-1].data));
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
            (buf->tag.relId.dbId == MyDatabaseId) &&
            (buf->tag.relId.relId == tempreldesc->rd_id)) {
            buf->flags &= ~BM_DIRTY;
            if (!(buf->flags & BM_FREE))
               ReleaseBuffer(i);
        }
     }
}

/* ---------------------------------------------------------------------
 *      DropBuffers
 *
 *	This function marks all the buffers in the buffer cache for a
 *	particular database as clean.  This is used when we destroy a
 *	database, to avoid trying to flush data to disk when the directory
 *	tree no longer exists.
 *
 *	This is an exceedingly non-public interface.
 * --------------------------------------------------------------------
 */

void
DropBuffers(dbid)
ObjectId dbid;
{
    register int i;
    BufferDesc *buf;

    for (i=1; i<=NBuffers; i++) {
	buf = BufferGetBufferDescriptor(i);
        if ((buf->tag.relId.dbId == dbid) && (buf->flags & BM_DIRTY)) {
            buf->flags &= ~BM_DIRTY;
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
    int i;
    BufferDesc *buf = BufferDescriptors;

    for (i = 0; i < NBuffers; ++i, ++buf) {
	elog(NOTICE, "[%02d] (freeNext=%d, freePrev=%d, relname=%.*s, \
blockNum=%d, flags=0x%x, refcount=%d %d)",
	     i, buf->freeNext, buf->freePrev, NAMEDATALEN, &(buf->sb_relname),
	     buf->tag.blockNum, buf->flags,
	     buf->refcount, PrivateRefCount[i]);
    }
}

void
GetPageAddr(bufno)
	long bufno;
{
	long p;

	p = ShmemBase + BufferDescriptors[bufno].data;
	printf("0x%lx (%ld)\n", p, p);
}

void
PrintPinnedBufs()
{
    int i;
    BufferDesc *buf = BufferDescriptors;

    for (i = 0; i < NBuffers; ++i, ++buf) {
	if (PrivateRefCount[i] > 0)
	    elog(NOTICE, "[%02d] (freeNext=%d, freePrev=%d, relname=%.*s, \
blockNum=%d, flags=0x%x, refcount=%d %d)\n",
		 i, buf->freeNext, buf->freePrev, NAMEDATALEN, &(buf->sb_relname),
		 buf->tag.blockNum, buf->flags,
		 buf->refcount, PrivateRefCount[i]);
     }
}

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
    int size = 0;
    int nbuckets;
    int nsegs;
    int tmp;

    nbuckets = 1 << (int)my_log2((NBuffers - 1) / DEF_FFACTOR + 1);
    nsegs = 1 << (int)my_log2((nbuckets - 1) / DEF_SEGSIZE + 1);

    /* size of shmem binding table */
    size += MAXALIGN(my_log2(BTABLE_SIZE) * sizeof(void *)); /* HTAB->dir */
    size += MAXALIGN(sizeof(HHDR));			     /* HTAB->hctl */
    size += MAXALIGN(DEF_SEGSIZE * sizeof(SEGMENT));
    size += BUCKET_ALLOC_INCR * 
	(MAXALIGN(sizeof(BUCKET_INDEX)) +
	 MAXALIGN(BTABLE_KEYSIZE) +
	 MAXALIGN(BTABLE_DATASIZE));
    
    /* size of buffer descriptors */
    size += MAXALIGN((NBuffers + 1) * sizeof(BufferDesc));

    /* size of data pages */
    size += NBuffers * MAXALIGN(BLOCK_SIZE);

    /* size of buffer hash table */
    size += MAXALIGN(my_log2(NBuffers) * sizeof(void *)); /* HTAB->dir */
    size += MAXALIGN(sizeof(HHDR));			  /* HTAB->hctl */
    size += nsegs * MAXALIGN(DEF_SEGSIZE * sizeof(SEGMENT));
    tmp = (int)ceil((double)NBuffers/BUCKET_ALLOC_INCR);
    size += tmp * BUCKET_ALLOC_INCR * 
	(MAXALIGN(sizeof(BUCKET_INDEX)) +
	 MAXALIGN(sizeof(BufferTag)) +
	 MAXALIGN(sizeof(Buffer)));
    
#if 0
    /* extra space, just to make sure there is enough */
    size += NBuffers * 4 + 4096;
#endif

#ifdef BMTRACE
    size += (BMT_LIMIT * sizeof(bmtrace)) + sizeof(long);
#endif
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
        }
        BufTableDelete(BufferGetBufferDescriptor(i));
    }
}

#undef IncrBufferRefCount
#undef ReleaseBuffer

IncrBufferRefCount(buffer)
Buffer buffer;
{
    Assert(!BAD_BUFFER_ID(buffer));
    Assert(PrivateRefCount[buffer - 1] >= 0);
    PrivateRefCount[buffer - 1]++;
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

  Assert(PrivateRefCount[buffer - 1] > 0);
  PrivateRefCount[buffer - 1]--;
  if (PrivateRefCount[buffer - 1] == 0 && LastRefCount[buffer - 1] == 0) {
      /* only release buffer if it is not pinned in previous ExecMain levels */
      SpinAcquire(BufMgrLock);
      bufHdr->refcount--;
      if (bufHdr->refcount == 0) {
	  AddBufferToFreelist(bufHdr);
	  bufHdr->flags |= BM_FREE;
      }
      SpinRelease(BufMgrLock);
  }

  return(STATUS_OK);
}

int ShowPinTrace = 0;

IncrBufferRefCount_Debug(file, line, buffer)
String file;
int line;
Buffer buffer;
{
    IncrBufferRefCount(buffer);
    if (ShowPinTrace && is_userbuffer(buffer)) {
        BufferDesc *buf = BufferGetBufferDescriptor(buffer);

        fprintf(stderr, "PIN(Incr) %d relname = %.*s, blockNum = %d, \
refcount = %d, file: %s, line: %d\n",
		buffer, NAMEDATALEN, &(buf->sb_relname), buf->tag.blockNum,
		PrivateRefCount[buffer - 1], file, line);
      }
}

ReleaseBuffer_Debug(file, line, buffer)
String file;
int line;
Buffer buffer;
{
    ReleaseBuffer(buffer);
    if (ShowPinTrace && is_userbuffer(buffer)) {
        BufferDesc *buf = BufferGetBufferDescriptor(buffer);

        fprintf(stderr, "UNPIN(Rel) %d relname = %.*s, blockNum = %d, \
refcount = %d, file: %s, line: %d\n",
		buffer, NAMEDATALEN, &(buf->sb_relname), buf->tag.blockNum,
		PrivateRefCount[buffer - 1], file, line);
    }
}

ReleaseAndReadBuffer_Debug(file, line, buffer, relation, blockNum)
String file;
int line;
Buffer buffer;
Relation relation;
BlockNumber blockNum;
{
    bool bufferValid;
    Buffer b;

    bufferValid = BufferIsValid(buffer);
    b = ReleaseAndReadBuffer(buffer, relation, blockNum);
    if (ShowPinTrace && bufferValid && is_userbuffer(buffer)) {
	BufferDesc *buf = BufferGetBufferDescriptor(buffer);

        fprintf(stderr, "UNPIN(Rel&Rd) %d relname = %.*s, blockNum = %d, \
refcount = %d, file: %s, line: %d\n",
		buffer, NAMEDATALEN, &(buf->sb_relname), buf->tag.blockNum,
		PrivateRefCount[buffer - 1], file, line);
    }
    if (ShowPinTrace && is_userbuffer(buffer)) {
	BufferDesc *buf = BufferGetBufferDescriptor(b);

        fprintf(stderr, "PIN(Rel&Rd) %d relname = %.*s, blockNum = %d, \
refcount = %d, file: %s, line: %d\n",
		b, NAMEDATALEN, &(buf->sb_relname), buf->tag.blockNum,
		PrivateRefCount[b - 1], file, line);
    }
    return b;
}

#ifdef BMTRACE

/*
 *  trace allocations and deallocations in a circular buffer in
 *  shared memory.  check the buffer before doing the allocation,
 *  and die if there's anything fishy.
 */

_bm_trace(dbId, relId, blkNo, bufNo, allocType)
    long dbId;
    long relId;
    int blkNo;
    int bufNo;
    int allocType;
{
    static int mypid = 0;
    long start, cur;
    bmtrace *tb;

    if (mypid == 0)
	mypid = getpid();

    start = *CurTraceBuf;

    if (start > 0)
	cur = start - 1;
    else
	cur = BMT_LIMIT - 1;

    for (;;) {
	tb = &TraceBuf[cur];
	if (tb->bmt_op != BMT_NOTUSED) {
	    if (tb->bmt_buf == bufNo) {
		if ((tb->bmt_op == BMT_DEALLOC)
		    || (tb->bmt_dbid == dbId && tb->bmt_relid == relId
			&& tb->bmt_blkno == blkNo))
		    goto okay;

		/* die holding the buffer lock */
		_bm_die(dbId, relId, blkNo, bufNo, allocType, start, cur);
	    }
	}

	if (cur == start)
	    goto okay;

	if (cur == 0)
	    cur = BMT_LIMIT - 1;
	else
	    cur--;
    }

okay:
    tb = &TraceBuf[start];
    tb->bmt_pid = mypid;
    tb->bmt_buf = bufNo;
    tb->bmt_dbid = dbId;
    tb->bmt_relid = relId;
    tb->bmt_blkno = blkNo;
    tb->bmt_op = allocType;

    *CurTraceBuf = (start + 1) % BMT_LIMIT;
}

_bm_die(dbId, relId, blkNo, bufNo, allocType, start, cur)
    long dbId;
    long relId;
    int blkNo;
    int bufNo;
    int allocType;
    long start;
    long cur;
{
    FILE *fp;
    bmtrace *tb;
    int i;

    tb = &TraceBuf[cur];

    if ((fp = fopen("/tmp/death_notice", "w")) == (FILE *) NULL)
	elog(FATAL, "buffer alloc trace error and can't open log file");

    fprintf(fp, "buffer alloc trace detected the following error:\n\n");
    fprintf(fp, "    buffer %d being %s inconsistently with a previous %s\n\n",
	    bufNo, (allocType == BMT_DEALLOC ? "deallocated" : "allocated"),
	    (tb->bmt_op == BMT_DEALLOC ? "deallocation" : "allocation"));

    fprintf(fp, "the trace buffer contains:\n");

    i = start;
    for (;;) {
	tb = &TraceBuf[i];
	if (tb->bmt_op != BMT_NOTUSED) {
	    fprintf(fp, "     [%3d]%spid %d buf %2d for <%d,%d,%d> ",
		    i, (i == cur ? " ---> " : "\t"),
		    tb->bmt_pid, tb->bmt_buf,
		    tb->bmt_dbid, tb->bmt_relid, tb->bmt_blkno);

	    switch (tb->bmt_op) {
	      case BMT_ALLOCFND:
		fprintf(fp, "allocate (found)\n");
		break;

	      case BMT_ALLOCNOTFND:
		fprintf(fp, "allocate (not found)\n");
		break;

	      case BMT_DEALLOC:
		fprintf(fp, "deallocate\n");
		break;

	      default:
		fprintf(fp, "unknown op type %d\n", tb->bmt_op);
		break;
	    }
	}

	i = (i + 1) % BMT_LIMIT;
	if (i == start)
	    break;
    }

    fprintf(fp, "\noperation causing error:\n");
    fprintf(fp, "\tpid %d buf %d for <%d,%d,%d> ",
	    getpid(), bufNo, dbId, relId, blkNo);

    switch (allocType) {
      case BMT_ALLOCFND:
	fprintf(fp, "allocate (found)\n");
	break;

      case BMT_ALLOCNOTFND:
	fprintf(fp, "allocate (not found)\n");
	break;

      case BMT_DEALLOC:
	fprintf(fp, "deallocate\n");
	break;

      default:
	fprintf(fp, "unknown op type %d\n", allocType);
	break;
    }

    (void) fclose(fp);

    kill(getpid(), SIGILL);
}

#endif /* BMTRACE */

void
BufferRefCountReset(refcountsave)
long *refcountsave;
{
    int i;
    for (i=0; i<NBuffers; i++) {
	refcountsave[i] = PrivateRefCount[i];
	LastRefCount[i] += PrivateRefCount[i];
	PrivateRefCount[i] = 0;
    }
}

void
BufferRefCountRestore(refcountsave)
long *refcountsave;
{
    int i;
    for (i=0; i<NBuffers; i++) {
	PrivateRefCount[i] = refcountsave[i];
	LastRefCount[i] -= refcountsave[i];
	refcountsave[i] = 0;
    }
}
