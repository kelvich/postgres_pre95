/* ----------------------------------------------------------------
 *   FILE
 *	block.h
 *
 *   DESCRIPTION
 *	POSTGRES disk block definitions.
 *
 *   NOTES
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef	BlockIncluded		/* Include this file only once */
#define BlockIncluded	1

#define BLOCK_H	"$Header$"

#include "tmp/c.h"

/* XXX this should be called BlockIndex -hirohama */

typedef uint32	BlockNumber;	/* page number */
typedef uint16	BlockSize;

#define InvalidBlockNumber	(-1)

typedef struct BlockIdData {
	int16	data[2];
} BlockIdData;

typedef BlockIdData	*BlockId;	/* block identifier */

/* ----------------
 *	support macros
 * ----------------
 */
/*
 * BlockSizeIsValid --
 *	True iff size is valid.
 *	should check that this is a power of 2
 *
 * XXX currently any block size is valid
 */
#define BlockSizeIsValid(blockSize) 1

/*
 * BlockNumberIsValid --
 *	True iff blockNumber is valid.
 */
#define BlockNumberIsValid(blockNumber) \
    ((bool) \
     (((blockNumber) >= 0) && ((int32) (blockNumber) != InvalidBlockNumber)))

/*
 * BlockIdIsValid --
 *	True iff the block identifier is valid.
 */
#define BlockIdIsValid(blockId) \
    ((bool) \
     (PointerIsValid(blockId) && ((blockId)->data[0] >= 0)))

/*
 * BlockIdSet --
 *	Sets a block identifier to the specified value.
 */
#define BlockIdSet(blockId, blockNumber) \
    Assert(PointerIsValid(blockId)); \
    (blockId)->data[0] = (blockNumber) >> 16; \
    (blockId)->data[1] = (blockNumber) & 0xffff

/*
 * BlockIdCopy --
 *	Copy a block identifier.
 */
#define BlockIdCopy(toBlockId, fromBlockId) \
    Assert(PointerIsValid(toBlockId)); \
    Assert(PointerIsValid(fromBlockId)); \
    (toBlockId)->data[0] = (fromBlockId)->data[0]; \
    (toBlockId)->data[1] = (fromBlockId)->data[1]

/*
 * BlockIdEquals --
 *	Check for block number equality.
 */
#define BlockIdEquals(blockId1, blockId2) \
    ((blockId1)->data[0] == (blockId2)->data[0] && \
     (blockId1)->data[1] == (blockId2)->data[1])

/*
 * BlockIdGetBlockNumber --
 *	Retrieve the block number from a block identifier.
 */
#define BlockIdGetBlockNumber(blockId) \
    (AssertMacro(BlockIdIsValid(blockId)) ? \
     (BlockNumber) \
     (((blockId)->data[0] << 16) + ((uint16) (blockId)->data[1])) : \
     (BlockNumber) InvalidBlockNumber)

#endif	/* !defined(BlockIncluded) */
