/*
 * hrnd.h --
 *	POSTGRES heap access method randomization definitions.
 *
 * Note:
 *	XXX This file should be moved to heap/.
 *
 * Identification:
 *	$Header$
 */

#ifndef	HRndIncluded	/* Include this file only once */
#define HRndIncluded	1

#ifndef C_H
#include "c.h"
#endif

#include "block.h"
#include "oid.h"
#include "rel.h"

typedef BlockNumber	*BlockIndexList;

/* XXX The following values are *not* tuned in any way */

#define MaxLengthOfBlockIndexList	3

#define FillLimitBase		16
#define OneBlockFillLimit	(FillLimitBase * 3/8)
#define TwoBlockFillLimit	(FillLimitBase * 1/4)
#define ThreeBlockFillLimit	(FillLimitBase * 3/16)
#define ManyBlockFillLimit	(FillLimitBase * 1/8)
#define ClusteredBlockFillLimit	(FillLimitBase * 1/16)

#define ClusteredNumberOfFailures	(1 + MaxLengthOfBlockIndexList)

#define FillLimitAdjustment(failures)\
	((failures >= MaxLengthOfBlockIndexList) ? 0 :\
		(FillLimitBase * (MaxLengthOfBlockIndexList - failures - 1)/16))
/*
 * InitRandom --
 *	Initializes randomization support.
 */
extern
void
InitRandom ARGS((
	void
));

/*
 * getclusteredappend --
 *	Returns block index to use for clustering.
 *
 * Note:
 *	This is a hack utill clustering is supported correctly.
 */
extern
BlockNumber
getclusteredappend ARGS((
	void
));

/*
 * setclusterblockindex --
 *	Causes append clustering on indicated block if enabled.
 *
 * Note:
 *	This is a hack utill clustering is supported correctly.
 */
extern
void
setclusterblockindex ARGS((
	BlockNumber	blockIndex
));

/*
 * setclusterblockindex --
 *	Causes append clustering on indicated block if enabled.
 *
 * Note:
 *	This is a hack utill clustering is supported correctly.
 */
extern
void
setclusterblockindex ARGS((
	BlockNumber	blockIndex
));

/*
 * RelationContainsUsableBlock --
 *	True iff free space in a block of relation is sufficient to hold tuple.
 *
 * Note:
 *	Assumes relation is valid and is not physically empty.
 *	Assumes block index is in valid range.
 *	Assumes tuple size is valid.
 */
extern
bool
RelationContainsUsableBlock ARGS((
	Relation	relation,
	BlockNumber	blockIndex,
	Size		size,
	Index		numberOfFailures
));

/*
 * RelationGetRandomBlockIndexList --
 *	Returns pointer to a static array of randomly generated block indexes.
 *
 * Note:
 *	Assumes relation is valid.
 */
extern
BlockIndexList
RelationGetRandomBlockIndexList ARGS((
	Relation	relation,
	ObjectId	id
));

#endif	/* !defined(HRndIncluded) */
