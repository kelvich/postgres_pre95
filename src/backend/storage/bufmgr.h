
; /*
; * 
; * POSTGRES Data Base Management System
; * 
; * Copyright (c) 1988 Regents of the University of California
; * 
; * Permission to use, copy, modify, and distribute this software and its
; * documentation for educational, research, and non-profit purposes and
; * without fee is hereby granted, provided that the above copyright
; * notice appear in all copies and that both that copyright notice and
; * this permission notice appear in supporting documentation, and that
; * the name of the University of California not be used in advertising
; * or publicity pertaining to distribution of the software without
; * specific, written prior permission.  Permission to incorporate this
; * software into commercial products can be obtained from the Campus
; * Software Office, 295 Evans Hall, University of California, Berkeley,
; * Ca., 94720 provided only that the the requestor give the University
; * of California a free licence to any derived software for educational
; * and research purposes.  The University of California makes no
; * representations about the suitability of this software for any
; * purpose.  It is provided "as is" without express or implied warranty.
; * 
; */



/*
 * bufmgr.h --
 *	POSTGRES buffer manager definitions.
 *
 * Identification:
 *	$Header$
 */

#ifndef	BufMgrIncluded	/* Include this file only once. */
#define BufMgrIncluded	1

#include "c.h"

#include "postgres.h"
#include "rel.h"

#include "status.h"

#include "buf.h"

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


#define BLCKSZ	8192	/* static not to be >= 65536 */ /* > ??? */
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
BufferGetDiskBlockSize ARGS((
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

/*
 * RelationGetBuffer --
 *	Returns the buffer associated with a page in a relation.
 *
 * Note:
 *	Assumes buffer is valid.
 */
extern
Buffer
RelationGetBuffer ARGS((
	const Relation		relation,
	const BlockNumber	blockNumber,
	const BufferLock	lockLevel
));

/*
 * BufferPut --
 *	Performs operations on a buffer.
 *
 * Note:
 *	Assumes buffer is valid.
 */
extern
ReturnStatus
BufferPut ARGS((
	const Buffer	buffer,
	const BufferLock	lockLevel
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
	const Buffer	predecessorBuffer,
	const Buffer	successorBuffer
));

/* XXX - Remove this cruft below : 
 */
/*
 * BufferIsLocked --
 *	True iff the buffer is locked for shared, update, or exclusive access.
 *
 * Note:
 *	Assumes buffer is valid.
 */
/*extern
bool
BufferIsLocked ARGS((
	const Buffer	buffer
));
*/

#endif	/* !defined(BufMgrIncluded) */
