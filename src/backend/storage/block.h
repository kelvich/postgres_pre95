/*
 * block.h --
 *	POSTGRES disk block definitions.
 *
 * Identification:
 *	$Header$
 */

#ifndef	BlockIncluded	/* Include this file only once */
#define BlockIncluded	1

#ifndef C_H
#include "c.h"
#endif

/* XXX this should be called BlockIndex -hirohama */
typedef uint32	BlockNumber;	/* page number */
typedef uint16	BlockSize;

#define InvalidBlockNumber	(-1)

typedef struct BlockIdData {
	int16	data[2];
} BlockIdData;

typedef BlockIdData	*BlockId;	/* block identifier */

/*
 * BlockNumberIsValid --
 *	True iff the block number is valid.
 */
extern
bool
BlockNumberIsValid ARGS((
	BlockNumber	blockNumber
));

/*
 * BlockIdIsValid --
 *	True iff the block identifier is valid.
 */
extern
bool
BlockIdIsValid ARGS((
	BlockId		pageId
));

/*
 * BlockIdSet --
 *	Sets a block identifier to the specified value.
 */
extern
void
BlockIdSet ARGS((
	BlockId		blockId,
	BlockNumber	blockNumber
));

/*
 * BlockIdGetBlockNumber --
 *	Retrieve the block number from a block identifier.
 */
extern
BlockNumber
BlockIdGetBlockNumber ARGS((
	PageId		pageId
));

#endif	/* !defined(BlockIncluded) */
