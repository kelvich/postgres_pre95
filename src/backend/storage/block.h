/*
 * block.h --
 *	POSTGRES disk block definitions.
 */

#ifndef	BlockIncluded		/* Include this file only once */
#define BlockIncluded	1

/*
 * Identification:
 */
#define BLOCK_H	"$Header$"

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
 * BlockSizeIsValid --
 *	True iff size is valid.
 */
extern
bool
BlockSizeIsValid ARGS((
	BlockSize	size
));

/*
 * BlockNumberIsValid --
 *	True iff blockNumber is valid.
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

#define BLOCK_SYMBOLS \
	SymbolDecl(BlockSizeIsValid, "_BlockSizeIsValid"), \
	SymbolDecl(BlockNumberIsValid, "_BlockNumberIsValid"), \
	SymbolDecl(BlockIdIsValid, "_BlockIdIsValid"), \
	SymbolDecl(BlockIdSet, "_BlockIdSet"), \
	SymbolDecl(BlockIdGetBlockNumber, "_BlockIdGetBlockNumber")

#endif	/* !defined(BlockIncluded) */
