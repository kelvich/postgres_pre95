/*
 *	POSTGRES buffer manager code.
 *
 * Note:
 *	The Sbuf, Sfreebuf<, and Buffer will be shared.  Each process will have
 *	its own Buf and Freebuf.  Order info will be local to the process and
 *	will be completely general.  (unimplemented.)
 *
 *	Problems to fix when shared memory implemented:
 *
 *	1)	There may be no need to keep track of the lock held by
 *		a postgres process in struct dbufc, since all locks
 *		should only be very temporary.
 *
 *	2)	Page ordering information is not used/maintained yet.
 *
 *	3)	Concurrent locking using L_UP or L_EX has problems now.
 *
 *	4)	The reclaiming of fd's contained in the cashed struct reldesc
 *		should be handled by the cashe system (not by bflush).
 *
 *	5)	lseek in 4.3 returns off_t.
 *
 *	6)	There may be a problem with direct modification of the
 *		shared bp->db_sb->sb_flag fields.  Also, verify that the
 *		separation of the bufc structure into two parts is correct.
 *
 * NOTES:
         It is expected that the caller explicitly use the lock manager
	 to lock with appropriate locks each buffer it needs to reference.
	 The buffer manager only handles conflicts that may arise
	 from two backends needing control over the free_list at
	 the same time.
 **********************************************************************/

#ifndef	lint
static char
	bufmgr_c[] = "$Header$";
#endif	/* !defined(lint) */

/*
#include <sys/types.h>
#include <sys/stat.h>
*/
#include <sys/file.h>

#include "postgres.h"
#include "context.h"
#include "access.h"

#include "c.h"
#include "os.h"

#include "buf.h"
#include "fd.h"
#include "log.h"
#include "status.h"
#include "lmgr.h"

#include "internal.h"

/*#undef	LATEWRITE	/* LATEWRITE requires reldesc caching */

static Lbufdesc		Local[NDBUFS];
static Lbufdesc		FreeLocal;
#define BufferGetBufferDescriptor(buffer) ((Lbufdesc *)&Local[buffer-1])
static Buffer BufferDescriptorGetBuffer();
/*
 * BufferManagerInit --
 *	Builds free list of buffers and files.
 *
 *	Will change when get shared memory.
 */

void
BufferManagerInit()
{
    register int	i;
    
    for (i = 0; i < NDBUFS; i++) {
	Local[i].next = &Local[i + 1];
	Local[i].prev = &Local[i - 1];
	Local[i].shared = (Sbufdesc *)NULL;
	Local[i].data = (Block)NULL;
	Local[i].reln = (Relation)NULL;
	Local[i].flags= BM_INIT;
	Local[i].refcount = 0;
    }
    FreeLocal.next = &Local[0];
    FreeLocal.prev = &Local[NDBUFS - 1];
    Local[0].prev = Local[NDBUFS - 1].next = &FreeLocal;
}

/**************************************************
  IsPrivate
  - returns true if buffer is in malloc space
    rather than in SHM.
  XXX - for now, always false until pvt buffers
        implemented.
 **************************************************/

bool
IsPrivate(bufnum)
     Buffer bufnum;
{
  return (false); /* for now, always false since only shared 
		 buffers exist */
} /* IsPrivate */

bool
BufferDescriptorIsValid(bufdesc)
     Lbufdesc *bufdesc;
{
    int temp;
    Assert(PointerIsValid(bufdesc));
    temp = (bufdesc-Local)/sizeof(Lbufdesc);
    if(temp >= 0 && temp<NDBUFS)
      return(true);
    else
      return(false);
} /*BufferDescriptorIsValid*/

/**************************************************

  1) remove local descriptor from free_list

 **************************************************/                                                   
Lbufdesc 
*GetFreeLocalBuffer()
{
    Lbufdesc *bp = NULL;
    if (FreeLocal.next == &FreeLocal)
      BM_debug(WARN, "bget: no free buffers");
    bp = FreeLocal.next;
    /* remove from free list */
    bp->next->prev = bp->prev;
    bp->prev->next = bp->next;
    bp->next = NULL;
    bp->prev = NULL;
    return(bp);
}

/**************************************************
  PutOnLocalFree
  takes a local-bufdesc
  1) puts it on the local-free-list
  2) removes any reference to shared buffers
  3) changes the flags on the local-bufdesc
     to reflect change in status
 **************************************************/

PutOnLocalFree(local)
     Lbufdesc *local;
{
    local->next = &FreeLocal;
    local->prev = FreeLocal.prev;
    FreeLocal.prev = local;
    local->prev->next = local;
    local->reln = NULL; /* since caller may have freed it */
    local->shared = NULL; /* since other buffer managers
			     may have changed the contents ? */
    local->flags = BM_INIT; /* will change once pvt buffers
			       become a reality */

    /* we don't change local->shared or local->flags*/
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
    if(bufnum <= 0 || bufnum > NDBUFS) {
      BM_debug(BUFDEB,"buffer not even in range");
      return(false);
    }
    if(IsPrivate(bufnum)){
	return((bool)(BufferGetBufferDescriptor(bufnum)->refcount > 0));
    } else {
	/* no need to P(sem), since no update by this
	   or any other backend will affect the result */
/*         if(Local[bufnum-1].refcount <= 0)
	   BM_debug(BUFDEB,"buffer: local refcount not > 0");
	 if(Local[bufnum-1].shared == NULL)
	   BM_debug(BUFDEB,"shared buffer pointer is NULL");
	 if(Local[bufnum-1].shared->refcount <= 0)
	   BM_debug(BUFDEB,"shared refcount <=0");
*/
		return((bool)(Local[bufnum - 1].refcount > 0 &&
			Local[bufnum - 1].shared != NULL &&
			Local[bufnum - 1].shared->refcount > 0));
    }
} /* BufferIsValid */

BlockSize
BufferGetBlockSize(buffer)
	Buffer	buffer;
{
	Assert(BufferIsValid(buffer));
	return (BufferGetBufferDescriptor(buffer)->shared->blksz);
}

BlockNumber
BufferGetBlockNumber(buffer)
	Buffer	buffer;
{
	Assert(BufferIsValid(buffer));
	return (BufferGetBufferDescriptor(buffer)->shared->blknum);
}


Relation
BufferGetRelation(buffer)
	Buffer	buffer;
{
	Relation	relation;

	Assert(BufferIsValid(buffer));

	relation = RelationIdGetRelation(LRelIdGetRelationId
		(BufferGetBufferDescriptor(buffer)->shared->reln_oid));

	RelationDecrementReferenceCount(relation);

	if (RelationHasReferenceCountZero(relation)) {
		elog(NOTICE, "BufferGetRelation: 0->1");

		RelationIncrementReferenceCount(relation);
	}

	return (relation);
}

/**************************************************
  FindLocalBuffer
  - takes a relation/blknum pair
  and returns the local buffer corresponding
  to that description if one exists.
  otherwise return NULL;
  Basically, such a buffer would exist iff
  it was already pinned by a previous read.
  SideEffect:
     increases the refcount for both shared/local control structs
 **************************************************/

static Lbufdesc 
*FindLocalBuffer(relation,blknum)
     Relation relation;
     BlockNumber blknum;
{
    /* for now, linear */
    register int i;
    for(i = 1 ;i <= NDBUFS ; i++) {
	if(IsPrivate(i)) {
	} else {
	    if(BufferIsValid((Buffer)i) &&
	       (LtEqualsRelId( RelationGetLRelId(relation),
		BufferGetBufferDescriptor(i)->shared->reln_oid)) &&
	       BufferGetBufferDescriptor(i)->shared->blknum ==
	       blknum) {
		/* unlink from local free_list,
		   unlink from global free_list,
		   return pointer */
		Lbufdesc *local = BufferGetBufferDescriptor(i);
		if(local->prev &&
		   local->next) {
		    local->prev->next = local->next;
		    local->next->prev = local->prev;
		    local->next = NULL;
		    local->prev = NULL;
		}
		if(!IsPrivate(i)) {
		    if(local->shared == NULL) 
		      BM_debug(WARN,"bufmgr: corrupted data structure");
		    else
		      IncrSharedRefCount(local->shared);
		    local->refcount ++;
		}
		if(!BufferIsValid(i))
		  elog(WARN,"something mighty strange");
		return(BufferGetBufferDescriptor(i));
	    } /*if(buffer matches specs)*/
	}/*if(Private)*/
    }/*for*/
    return((Lbufdesc *)NULL);
}

Buffer
ReadBuffer(relation,blknum,flags)
     Relation relation;
     BlockNumber blknum;
     BufFlags flags;
{
    Lbufdesc   *bp = NULL;
    bool IsShared;
    Sbufdesc *sb = NULL;
    if(flags&BM_SHARED) 
      IsShared=true;
    else
      IsShared= false;

    if(!(RelationIsValid(relation)))
	elog(WARN,"ReadBuffer called with null relation");
    if(blknum != P_NEW) {
	BM_debug(BUFDEB,"read:reln/blnum = %.16s/%d\n",
	     RelationGetRelationName(relation),blknum);
	bp = FindLocalBuffer(relation,blknum);
	if(bp == NULL) {
	    BM_debug(BUFDEB,"read : Buffer not in local memory\n");
	    bp = GetFreeLocalBuffer();
	    Assert(PointerIsValid(bp));
	    sb = ReadSharedBuffer(relation,blknum);
	    Assert(PointerIsValid(sb));
	    if(sb != NULL) {
		bp->shared = sb;
		bp->refcount = 1;
		bp->reln = relation;
		bp->flags |= BM_PINNED;
	    }
	}
	BM_debug(BUFDEB,"read:bufdesc is %d\n",bp);
	/* 
	 * At this pt, bp MUST be a valid buffer 
	 * descriptor
	 */
    } else { 
	/* caller wants P_NEW */
	BM_debug(BUFDEB,"Someone actually wants P_NEW ?\n");
	bp = GetFreeLocalBuffer();
	sb = ReadSharedBuffer(relation,blknum);
	Assert(PointerIsValid(sb));
	if(sb != NULL)
	  bp->shared = sb;
	else
	  ; /* XXX BM_debug ? */
	bp->reln = relation;
	bp->refcount = 1;
	bp->flags = flags;
    }
    BM_debug(BUFDEB,"read:buffer number is %d\n",BufferDescriptorGetBuffer(bp));
/* XXX - only if server model is reached
   otherwise, only 1 xaction per backend, so no need 
    bp->xid = GetCurrentTransactionId();*/
    Assert(BufferIsValid(BufferDescriptorGetBuffer(bp)));
    return (BufferDescriptorGetBuffer(bp));
}

/* INTERNAL */

static Buffer
BufferDescriptorGetBuffer(descriptor)
     Lbufdesc *descriptor;
{
/*    BM_debug(BUFDEB,"descriptor = %d,local = %d",descriptor,Local);
    BM_debug(BUFDEB," size = %d\n",sizeof(Lbufdesc));*/
  BM_debug(BUFDEB,"buffer - %d",descriptor-Local);
    if (BufferDescriptorIsValid(descriptor))
      return(1+descriptor - Local);
    BM_debug(WARN,"Invalid bufferDescriptor\n");
    return (-1);
}

/**************************************************
  WriteBuffer
  - assumes that a corresponding call to ReadBuffer
    has been made, pinning the buffer in memory.

  - decrement the refcount locally and globally,
    putting to the free_list (locally and globally)
    where necessary.

 **************************************************/

ReturnStatus
WriteBuffer(bufnum)
     Buffer bufnum;
{
    Lbufdesc *buf = BufferGetBufferDescriptor(bufnum); 
    BM_debug(BUFDEB,"write: buffer number is %d\n",bufnum);
    if(buf->refcount == 0)
      BM_debug(WARN,"Write: Inconsistent Buffer refcount (< 0)");
    if(IsPrivate(bufnum)) {
	/* private buffers */
    } else {
	WriteSharedBuffer(buf->shared);
    }
    buf->refcount -= 1;
#ifdef LATEWRITE
    buf->flags |= BM_DIRTY;
#else
    buf->flags = BM_INIT;  /* flushed */
#endif
    if(buf->refcount == 0)
      PutOnLocalFree(buf);
    return(0);
} /*WriteBuffer*/

ReturnStatus
FlushBuffer(bufnum)
     Buffer bufnum;
{
    Lbufdesc *buf = BufferGetBufferDescriptor(bufnum); 
    BM_debug(BUFDEB,"write: buffer number is %d\n",bufnum);
    if(buf->refcount == 0)
      BM_debug(WARN,"Write: Inconsistent Buffer refcount (< 0)");
    if(IsPrivate(bufnum)) {
	/* private buffers */
    } else {
	FlushSharedBuffer(buf->shared);
    }
    buf->refcount -= 1;
    buf->flags = BM_INIT;  /* flushed */

    if(buf->refcount == 0)
      PutOnLocalFree(buf);
    return(0);
} /*WriteBuffer*/


/**************************************************
  WriteNoRelease
  - writes but does not affect pins (refcounts)

 **************************************************/

ReturnStatus
WriteNoReleaseBuffer(bufnum)
     Buffer bufnum;
{
    Lbufdesc *buf = BufferGetBufferDescriptor(bufnum); 
    BM_debug(BUFDEB,"write: buffer number is %d\n",bufnum);

    if(buf->refcount == 0)
      BM_debug(WARN,"Write: Inconsistent Buffer refcount (< 0)");
    if(IsPrivate(bufnum)) {
	/* private buffers */
    } else {
	IncrSharedRefCount(BufferGetBufferDescriptor(bufnum)->shared);
	WriteSharedBuffer(buf->shared); /* WriteShared . . . decrements */
    }
#ifdef LATEWRITE
    buf->flags |= BM_DIRTY;
#else
    buf->flags = BM_INIT;  /* flushed */
#endif
    return(0);
} /*WriteNoReleaseBuffer*/

ReturnStatus
ReleaseBuffer(bufnum)
     Buffer bufnum;
{
    Lbufdesc *buf = BufferGetBufferDescriptor(bufnum);
    BM_debug(BUFDEB,"release: buffer number is %d\n",bufnum);
    if(buf->refcount == 0)
      BM_debug(WARN,"ReleaseBuffer :Inconsistent Buffer refcount (<0)");
    if(IsPrivate(bufnum)) {
	/* private buffers */
    } else {
	ReleaseSharedBuffer(buf->shared);
    }
    buf->refcount --;
    if(buf->refcount == 0)
      PutOnLocalFree(buf);
    return(0);
}

/**************************************************

  BufferPut 
  XXX -  this is is old cruft rehacked so it will
  coninue to behave in a predictably buggy version.
  Done under extreme protest. -jeff goh

  Remove ASAP.
 **************************************************/
ReturnStatus
BufferPut(buffer,lockLevel)
    Buffer buffer;
    BufFlags lockLevel;
    
{
    Assert(BufferIsValid(buffer));
    if(lockLevel & L_UN) {
	switch(lockLevel & L_LOCKS) {
	  case L_PIN:
	    if(!BufferIsValid(buffer))
	      BM_debug(WARN,"BufferPut called with invalid buffer");
	    ReleaseBuffer(buffer);
	    return(0);
	  case L_LOCKS:
	    BM_debug(BUFDEB,"BufferPut called with UN_LOCKS, unsupported");
	    if(!BufferIsValid(buffer))
	      BM_debug(WARN,"BufferPut called with invalid buffer");
	    IncrSharedRefCount(BufferGetBufferDescriptor(buffer)->shared);
	    BufferGetBufferDescriptor(buffer)->refcount ++;
	    WriteBuffer(buffer);
	    return(0);
	  default:
	    if(lockLevel & L_WRITE) 
		WriteBuffer(buffer);
	    else
	        ReleaseBuffer(buffer);
	    return(0);
	}
    }
    switch(lockLevel & L_LOCKS) {
      case L_SH:
	BM_debug(BUFDEB,"BufferPut: unsupported flag L_SH");
	if(lockLevel & L_WRITE) {
	    IncrSharedRefCount(BufferGetBufferDescriptor(buffer)->shared);
	    BufferGetBufferDescriptor(buffer)->refcount++;
	    WriteBuffer(buffer);
	}
	break;
      case L_EX:
	BM_debug(BUFDEB,"BufferPut: unsupported flag L_EX");
	if(lockLevel & L_WRITE) {
	    IncrSharedRefCount(BufferGetBufferDescriptor(buffer)->shared);
	    BufferGetBufferDescriptor(buffer)->refcount++;
	    WriteBuffer(buffer);
	}
	break;
      case L_UP:
	BM_debug(BUFDEB,"BufferPut: unsupported flag L_UP");
	if(lockLevel & L_WRITE) {
	    IncrSharedRefCount(BufferGetBufferDescriptor(buffer)->shared);
	    BufferGetBufferDescriptor(buffer)->refcount++;
	    WriteBuffer(buffer);
	}
	break;
      case L_PIN:
	BM_debug(BUFDEB,"BufferPut: repinning buffer? bad flag L_PIN");
	if(!BufferIsValid(buffer))
	  elog(WARN,"BufferPut: trying to pin invalid buffer");
	if(lockLevel & L_WRITE) {
	    IncrSharedRefCount(BufferGetBufferDescriptor(buffer)->shared);
	    BufferGetBufferDescriptor(buffer)->refcount++;
	    WriteBuffer(buffer);
	}
	break;
      case L_DUP:
	Assert(BufferIsValid(buffer));
	Assert(BufferGetBufferDescriptor(buffer)->shared != 0);
	IncrSharedRefCount(BufferGetBufferDescriptor(buffer)->shared);
	BufferGetBufferDescriptor(buffer)->refcount ++;
	break;
      default:
	BM_debug(BUFDEB,"BufferPut(%x, 0%o)",
	     BufferGetBufferDescriptor(buffer),lockLevel);
	break;
    }
    return(0);
}
/*
 * RelationGetBuffer --
 *	Gets the buffer number of a given disk block.
 *
 *	Returns InvalidBuffer on error.  Currently calls BM_debug() instead.
 */

Buffer
RelationGetBuffer(relation, blockNumber, lockLevel)
     Relation	relation;		/* relation */
     BlockNumber	blockNumber;		/* block number */
     BufferLock	lockLevel;		/* lock level */

{
  flag_print(lockLevel);
      return(ReadBuffer(relation,blockNumber,BM_PINNED|BM_SHARED));

/*    if(lockLevel == L_PIN)
      return(ReadBuffer(relation,blockNumber,BM_PINNED|BM_SHARED));
    else 
      BM_debug(BUFDEB,"dying in RelationGetBuffer");
    return(InvalidBuffer);*/
}


bool
BufferIsDirty(buffer)
	Buffer buffer;
{
	return((bool)(BufferGetBufferDescriptor(buffer)->shared->flags ==
		      BM_DIRTY));
}

/*
 *
 *	bflush		- flush buffers
 *
 *	Ordering info is currently ignored.  Should also free the
 *	reldescs's and fd's too in conjunction with the reldesc
 *	caching.
 */

void
BufferManagerFlush()
{
    register int i;
    for(i=1;i<=NDBUFS;i++) {
	if(BufferIsValid(i)) {
	  WriteBuffer(i); /* XXX should be WriteBuffer? */
	  while(BufferIsValid(i))
	    ReleaseBuffer(i);
	}
    }
    FlushDirtyShared(); /* XXX should be at exit time ? */
}

bool
BufferIsInvalid(buffer)
	Buffer	buffer;
{
	return ((bool)(buffer == InvalidBuffer));
}

bool
BufferIsUnknown(buffer)
	Buffer	buffer;
{
	return ((bool)(buffer == UnknownBuffer));
}
/*
 * RelationGetNumberOfPages --
 *	Returns number of pages in an open relation.
 *
 * Note:
 *	XXX may fail for huge relations.
 *	XXX should be elsewhere.
 *	XXX maybe should be hidden
 */

BlockNumber
RelationGetNumberOfBlocks(relation)
Relation	relation;
{
	int	i;

	i = (int)FileSeek(relation->rd_fd, 0l, L_XTND) - 1;
	return ((BlockNumber)((i < 0) ? 0 : 1 + i/BLCKSZ));
}




flag_print(flags)
     bits16 flags;
{
  BM_debug(BUFDEB,"called with 0x%x",flags);
  if(flags == L_UNLOCK) {
    BM_debug(BUFDEB,"UNLOCKING");
    return;
  }
  if(flags & L_UN)
    BM_debug(BUFDEB,"L_UN ");
  if(flags & L_SH)
    BM_debug(BUFDEB,"L_SH ");
  if(flags & L_PIN)
    BM_debug(BUFDEB,"L_PIN");
  if(flags & L_UP)
    BM_debug(BUFDEB,"L_UP");
  if(flags & L_WRITE)
    BM_debug(BUFDEB,"L_WRITE");
  if(flags & L_NB)
    BM_debug(BUFDEB,"L_NB: unimplemented No Blocking ");
  if(flags & L_COPY)
    BM_debug(BUFDEB,"L_COPY : get a version ???? ");
  if(flags == L_DUP)
    BM_debug(BUFDEB,"duplicate a buffer");
/*  if(flags & L_UNPIN)
    BM_debug(BUFDEB,"L_UNPIN ");*/
  BM_debug(BUFDEB,"\n");
  BM_debug(BUFDEB,"***");
}

Block
BufferGetBlock(buffer)
	Buffer	buffer;
{
  extern Block shared_bufdata;
	Assert(BufferIsValid(buffer));
/*	BM_debug(BUFDEB,"Bgetblock : buffer number is %d\n",buffer);*/
	if(IsPrivate(buffer)) {
	} else {
	    if(BufferGetBufferDescriptor(buffer)->shared)
	      return(&shared_bufdata
		     [BufferGetBufferDescriptor(buffer)->shared->data]);
	    else
	      BM_debug(WARN,"bufmgr/BufferGetBlock :internal data struct corrupt");
	}
	return ((Block)NULL);
}
