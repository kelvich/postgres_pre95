/*
 * bufmgr.h --
 *	POSTGRES buffer manager definitions.
 */

#ifndef	BufMgrIncluded		/* Included this file only once */
#define BufMgrIncluded	1

/*
 * Identification:
 */
#define BUFMGR_H	"$Header$"

#include "tmp/c.h"

#include "storage/buf.h"
#include "storage/buf_internals.h"
#include "machine.h"		/* for BLCKSZ */
#include "utils/rel.h"

/* lock levels */
#define L_LOCKS	0x07	/* lock mask */
#define L_UN	SIGNBIT	/* release (instead of obtain) */

#define L_PIN	0x00	/* pin (must be 0) */
#define L_SH	0x01	/* shared lock (must be 1) */
#define L_UP	0x02	/* update lock */
#define L_EX	0x04	/* exclusive lock */
#define L_WRITE	0x08	/* dirty flag (must be 1 + L_LOCKS) */
#define L_NEW	0x10	/* complete page rewrite - don't bother to read */
#define L_NB	0x20	/* Don't ever block (unimplemented) */
#define L_COPY	0x40	/* get a version (unimplemented) */
#define L_DUP	L_LOCKS	/* duplicate a buffer */
#define L_UNPIN	(L_UN | L_PIN)	/* unpin */
#define L_UNLOCK	(L_UN | L_LOCKS)	/* unlock */

#define BufferGetBufferDescriptor(buffer) ((BufferDesc *)&BufferDescriptors[buffer-1])

/* MOVED TO $OD/lib/H/installinfo.h */
/* #define BLCKSZ	8192*/	/* static not to be >= 65536 */ /* > ??? */
typedef struct BlockData {
	char	data[BLCKSZ];
} BlockData;

typedef BlockData	*Block;


/* special pageno for bget */
#define P_NEW	((BlockNumber)-1)	/* grow the file to get a new page */

typedef bits16	BufferLock;

/**********************************************************************

  the rest is function defns in the bufmgr that are externally callable

 **********************************************************************/

/*
 * BufferIsPinned --
 *	True iff the buffer is pinned (and therefore valid)
 *
 * Note:
 *	Smenatics are identical to BufferIsValid 
 *      XXX - need to remove either one eventually.
 */

#define BufferIsPinned BufferIsValid

/*
 * BufferGetBlock --
 *	Returns a reference to a disk page image associated with a buffer.
 *
 * Note:
 *	Assumes buffer is valid.
 */
extern
Block
BufferGetBlock ARGS((
	const Buffer	buffer
));

/*
 * BufferGetRelation --
 *	Returns the relation desciptor associated with a buffer.
 *
 * Note:
 *	Assumes buffer is valid.
 */
extern
Relation
BufferGetRelation ARGS((
	const Buffer	buffer
));

/*
 * BufferGetBlockSize --
 *	Returns the size of the buffer page.
 *
 * Note:
 *	Assumes that the buffer is valid.
 */
extern
BlockSize
BufferGetBlockSize ARGS((
	Buffer	buffer
));

/*
 * BufferGetBlockNumber --
 *	Returns the block number associated with a buffer.
 *
 * Note:
 *	Assumes that the buffer is valid.
 */
extern
BlockNumber
BufferGetBlockNumber ARGS((
	const Buffer	buffer
));

#ifndef BUFMGR_DEBUG
/*
 * ReadBuffer --
 *
 */
extern
ReturnStatus
ReadBuffer ARGS((
	Relation	relation,
	BlockNumber	blockNumber
));
#endif

/*
 * ReadBufferWithBufferLock
 *
 */
extern
Buffer
ReadBufferWithBufferLock ARGS((
	Relation	relation,
	BlockNumber	blockNum,
	bool		bufferLockHeld
));

#ifndef BUFMGR_DEBUG
/*
 * WriteBuffer --
 *
 */
extern
ReturnStatus
WriteBuffer ARGS((
	Buffer	buffer
));
#endif

/*
 * WriteNoReleaseBuffer --
 *
 */
extern
ReturnStatus
WriteNoReleaseBuffer ARGS((
	Buffer	buffer
));

#ifndef BUFMGR_DEBUG
/*
 * ReleaseBuffer --
 *
 */
extern
ReturnStatus
ReleaseBuffer ARGS((
	Buffer	buffer
));

/*
 * ReleaseAndReadBuffer
 *
 */
extern
Buffer
ReleaseAndReadBuffer ARGS((
	Buffer buffer,
	Relation relation,
	BlockNumber blockNum
));
#endif

extern
Buffer
RelationGetBufferWithBuffer ARGS((
	Relation 	relation,
	BlockNumber	blockNumber,
	Buffer		buffer
));

/*
 * BufferWriteInOrder --
 *	Causes two buffers to be written in the specified order.
 *
 * Note:
 *	Assumes buffers are valid.
 */
extern
ReturnStatus
BufferWriteInOrder ARGS((
    BufferDesc  *predb, BufferDesc *succb
));

extern
void
FlushBufferPool ARGS((int StableMainmemoryFlag));

extern
void
ResetBufferPool ARGS(());

extern
int
BufferPoolCheckLeak ARGS(());

extern
int
BufferShmemSize ARGS(());

extern
void
BufferPoolBlowaway ARGS(());

extern
void
ResetBufferUsage ARGS(());

extern
void
PrintBufferUsage ARGS(());

extern
void
BufferRefCountReset ARGS((int *refcountsave));

extern
void
BufferRefCountRestore ARGS((int *refcountsave));
#endif	/* !defined(BufMgrIncluded) */
