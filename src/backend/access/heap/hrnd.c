
/*
 * 
 * POSTGRES Data Base Management System
 * 
 * Copyright (c) 1988 Regents of the University of California
 * 
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for educational, research, and non-profit purposes and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of the University of California not be used in advertising
 * or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Permission to incorporate this
 * software into commercial products can be obtained from the Campus
 * Software Office, 295 Evans Hall, University of California, Berkeley,
 * Ca., 94720 provided only that the the requestor give the University
 * of California a free licence to any derived software for educational
 * and research purposes.  The University of California makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 * 
 */



/*
 * hrnd.c --
 *	POSTGRES heap access method randomization code.
 */

#include "c.h"

#include "align.h"
#include "block.h"
#include "buf.h"
#include "bufmgr.h"
#include "bufpage.h"
#ifdef	RANDOMDEBUG
#include "log.h"
#endif	/* defined(RANDOMDEBUG) */
#include "oid.h"
#include "rel.h"

#include "hrnd.h"

RcsId("$Header$");

static bool		DisableHeapRandomization = false;
static BlockNumber	RandomBlockIndexList[1 + MaxLengthOfBlockIndexList];
static bool		ClusterIsEnabled;
static BlockNumber	ClusterBlockIndex;

/*
 * BlockIndexRandomlyFillRandomBlockIndexList --
 *	Fills RandomBlockIndexList with random indexes given number of blocks.
 */
extern
void
BlockIndexRandomlyFillRandomBlockIndexList ARGS((
	BlockNumber	numberOfBlocks
));

/*
 * BlockIndexFillRandomBlockIndexList --
 *	Fills RandomBlockIndexList with indexes given number of blocks and id.
 */
extern
void
BlockIndexFillRandomBlockIndexList ARGS((
	BlockNumber	numberOfBlocks,
	ObjectId	id
));

/*
 * ObjectIdHashBlockIndex --
 *	Returns a block index hashed from an object id.
 */
extern
BlockNumber
ObjectIdHashBlockIndex ARGS((
	ObjectId	id,
	Index		method
));

void
InitRandom()
{
	extern Pointer	getenv();

	if (PointerIsValid(getenv("POSTGROWS"))) {
		DisableHeapRandomization = true;
		RandomBlockIndexList[0] = InvalidBlockNumber;
	} else {
		DisableHeapRandomization = false;
		RandomBlockIndexList[MaxLengthOfBlockIndexList] =
			InvalidBlockNumber;
	}
	ClusterBlockIndex = InvalidBlockNumber;
}

void
setclusteredappend(on)
	bool	on;
{
	ClusterBlockIndex = InvalidBlockNumber;
	ClusterIsEnabled = on;
}

BlockNumber
getclusterblockindex()
{
	if (!ClusterIsEnabled) {
		Assert(!BlockNumberIsValid(ClusterBlockIndex));
	}
	return (ClusterBlockIndex);
}

void
setclusterblockindex(blockIndex)
	BlockNumber	blockIndex;
{
	Assert(BlockNumberIsValid(blockIndex));

	if (ClusterIsEnabled) {
		ClusterBlockIndex = blockIndex;

#ifdef	RANDOMDEBUG
		elog(DEBUG, "setclusterblockindex(%d)", blockIndex);
#endif	/* defined(RANDOMDEBUG) */
	}
}

BlockIndexList
RelationGetRandomBlockIndexList(relation, id)
	Relation	relation;
	ObjectId	id;
{
	Assert(RelationIsValid(relation));

	if (!DisableHeapRandomization) {
		if (!ObjectIdIsValid(id)) {
			BlockIndexRandomlyFillRandomBlockIndexList(
				RelationGetNumberOfBlocks(relation));
		} else {
			BlockIndexFillRandomBlockIndexList(
				RelationGetNumberOfBlocks(relation), id);
		}
	}

	return (RandomBlockIndexList);
}

/*
 * XXX RelationContainsUsableBlock uses heuristics instead of statistics.
 */
bool
RelationContainsUsableBlock(relation, blockIndex, size, numberOfFailures)
	Relation	relation;
	BlockNumber	blockIndex;
	TupleSize	size;
	Index		numberOfFailures;
{
	BlockNumber	numberOfBlocks;
	BlockSize	fillLimit;
	PageFreeSpace	pageFreeSpace;
	Buffer		buffer;

	Assert(RelationIsValid(relation));
	Assert(size < BLCKSZ);

	numberOfBlocks = RelationGetNumberOfBlocks(relation);
	Assert(numberOfBlocks > 0);

	if (ClusterIsEnabled &&
			numberOfFailures == ClusteredNumberOfFailures) {

		fillLimit = ClusteredBlockFillLimit * (BLCKSZ / FillLimitBase);
	} else {
		switch (numberOfBlocks) {
		case 1:
			fillLimit = OneBlockFillLimit;
			break;
		case 2:
			fillLimit = TwoBlockFillLimit;
			break;
		case 3:
			fillLimit = ThreeBlockFillLimit;
			break;
		default:
			fillLimit = ManyBlockFillLimit;
			break;
		}
		fillLimit += FillLimitAdjustment(numberOfFailures);
		fillLimit *= BLCKSZ / FillLimitBase;
	}

	buffer = RelationGetBuffer(relation, blockIndex, L_PIN);
	pageFreeSpace = PageGetFreeSpace(BufferSimpleGetPage(buffer));
	BufferPut(buffer, L_UNPIN);

	return ((bool)((int32)pageFreeSpace - (int32)LONGALIGN(size) >
		(int32)fillLimit));
}

void
BlockIndexRandomlyFillRandomBlockIndexList(numberOfBlocks)
	BlockNumber	numberOfBlocks;
{
	Index	entry;

	if (numberOfBlocks < MaxLengthOfBlockIndexList) {
		RandomBlockIndexList[numberOfBlocks] = InvalidBlockNumber;
	}
	for (entry = 0; entry < Min(numberOfBlocks, MaxLengthOfBlockIndexList);
			entry += 1) {

		Index		index;

		do {
			BlockNumber	blockIndex;

			blockIndex = random() % numberOfBlocks;
			RandomBlockIndexList[entry] = blockIndex;
			for (index = 0; index < entry; index += 1) {
				if (RandomBlockIndexList[index] == blockIndex) {
					break;
				}
			}
		} while (index != entry);
	}
#ifdef	RANDOMDEBUG
	elog(DEBUG, "RandomlyFillRandomBlockIndexList returns [%d,%d,%d,%d]",
		RandomBlockIndexList[0],
		RandomBlockIndexList[1],
		RandomBlockIndexList[2],
		RandomBlockIndexList[3]);
#endif	/* defined(RANDOMDEBUG) */
}

void
BlockIndexFillRandomBlockIndexList(numberOfBlocks, id)
	BlockNumber	numberOfBlocks;
	ObjectId	id;
{
	Index	entry;
	Index	method = 0;

	if (numberOfBlocks < MaxLengthOfBlockIndexList) {
		RandomBlockIndexList[numberOfBlocks] = InvalidBlockNumber;
	}
	for (entry = 0; entry < Min(numberOfBlocks, MaxLengthOfBlockIndexList);
			entry += 1) {

		Index		index;

		do {
			BlockNumber	blockIndex;

			blockIndex = ObjectIdHashBlockIndex(id, method)
				% numberOfBlocks;
			method += 1;
			RandomBlockIndexList[entry] = blockIndex;
			for (index = 0; index < entry; index += 1) {
				if (RandomBlockIndexList[index] == blockIndex) {
					break;
				}
			}
		} while (index != entry);
	}
#ifdef	RANDOMDEBUG
	elog(DEBUG, "FillRandomBlockIndexList(%d,%d) returns [%d,%d,%d,%d]",
		numberOfBlocks, id,
		RandomBlockIndexList[0],
		RandomBlockIndexList[1],
		RandomBlockIndexList[2],
		RandomBlockIndexList[3]);
#endif	/* defined(RANDOMDEBUG) */
}

/* XXX tune me.  case 0 is not very random -> good? bad? */
BlockNumber
ObjectIdHashBlockIndex(id, method)
	ObjectId	id;
	Index		method;
{
	BlockNumber	index;

	switch (method % 5) {
	case 0:
		index = 17 + ((id << 2) ^ (id >> 1)) + id * (1 + (method >> 1));
		break;
	case 1:
		index = 37 + ((id << 3) ^ (id >> 2)) + id * (method >> 2);
		break;
	case 2:
		index = 67 + ((id << 5) ^ (id >> 3)) + id * (method >> 1);
		break;
	case 3:
		index = 107 + ((id << 7) ^ (id >> 5)) + id * (method >> 2);
		break;
	case 4:
		index = 137 + ((id << 11) ^ (id >> 7)) + id * (method >> 1);
		break;
	default:
		Assert(false);
		break;
	}
	return(index + method);
}
