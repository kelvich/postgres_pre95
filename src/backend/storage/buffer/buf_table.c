/*
 * buf_table.c -- routines for finding buffers in the buffer pool.
 *
 * Identification:
 *	$Header$
 *
 * Data Structures:
 *
 *	Buffers are identified by their BufferTag (buf.h).  This
 * file contains routines for allocating a shmem hash table to
 * map buffer tags to buffer descriptors.
 *
 * Synchronization:
 *  
 *  All routines in this file assume buffer manager spinlock is
 *  held by their caller.
 */
#include "internal.h"
#include "storage/shmem.h"
#include "storage/spin.h"
#include "utils/hsearch.h"
#include "utils/log.h"

HTAB *SharedBufHash,*ShmemInitHash();
int tag_hash();

extern int NBuffers;

/* 
 * declared for debugging only.  
 */
extern SPINLOCK *BufMgrLock;

typedef struct lookup { 
  BufferTag	key; 
  Buffer	id; 
} LookupEnt;

/*
 * Initialize shmem hash table for mapping buffers
 */
InitBufTable()
{
  HASHCTL info;
  int hash_flags;

  /* assume lock is held */
  is_LOCKED(BufMgrLock);

  /* BufferTag maps to Buffer */
  info.keysize = sizeof(BufferTag);
  info.datasize = sizeof(Buffer);
  info.hash = tag_hash;

  hash_flags = (HASH_ELEM | HASH_FUNCTION);


  SharedBufHash = (HTAB *) ShmemInitHash("Shared Buf Lookup Table",
		  NBuffers,NBuffers,
		  &info,hash_flags);

  if (! SharedBufHash) {
    elog(FATAL,"couldn't initialize shared buffer pool Hash Tbl");
    exit(1);
  }

}

BufferDesc *
BufTableLookup(tagPtr)
BufferTag *tagPtr;
{
  LookupEnt *	result;
  Boolean	found;

  if (tagPtr->blockNum == NEW_BLOCK)
      return(NULL);
  is_LOCKED(BufMgrLock);


  result = (LookupEnt *) 
    hash_search(SharedBufHash,tagPtr,FIND,&found);

  if (! result){
    elog(WARN,"BufTableLookup: BufferLookup table corrupted");
    return(NULL);
  }
  if (! found) {
    return(NULL);
  }
  return(&(BufferDescriptors[result->id]));
}

/*
 * BufTableDelete
 */
BufTableDelete(buf)
BufferDesc *buf;
{
  LookupEnt *	result;
  Boolean	found;

  /* buffer not initialized or has been removed from
   * table already.  BM_DELETED keeps us from removing 
   * buffer twice.
   */
  if (buf->flags & BM_DELETED) {
    return(TRUE);
  }

  buf->flags |= BM_DELETED;

  result = (LookupEnt *)
    hash_search(SharedBufHash,&(buf->tag),REMOVE,&found);

  if (! (result && found)) {
    elog(WARN,"BufTableDelete: BufferLookup table corrupted");    
    return(FALSE);
  }

  return(TRUE);
}

BufTableInsert(buf)
BufferDesc *buf;
{
  LookupEnt *	result;
  Boolean	found;

  /* cannot insert it twice */
  Assert (buf->flags & BM_DELETED);
  buf->flags &= ~(BM_DELETED);

  result = (LookupEnt *)
    hash_search(SharedBufHash,&(buf->tag),ENTER,&found);

  if (! result) {
    elog(WARN,"BufTableInsert: BufferLookup table corrupted");
    return(FALSE);
  }
  /* found something else in the table ! */
  if (found) {
    elog(WARN,"BufTableInsert: BufferLookup table corrupted");
    return(FALSE);
  } 

  result->id = buf->id;
  return(TRUE);
}

/* prints out collision stats for the buf table */
DBG_LookupListCheck(nlookup)
int nlookup;
{
  nlookup = 10;

  hash_stats("Shared",SharedBufHash);
}
