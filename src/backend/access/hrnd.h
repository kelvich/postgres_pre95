
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

#include "c.h"

#include "block.h"
#include "oid.h"
#include "rel.h"
#include "tupsiz.h"

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
	TupleSize	size,
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
