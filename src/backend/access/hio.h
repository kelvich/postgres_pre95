/*
 * hio.h --
 *	POSTGRES heap access method input/output definitions.
 *
 * Note:
 *	XXX This file should be moved to heap/.
 *
 * Identification:
 *	$Header$
 */

#ifndef	HIOIncluded	/* Include this file only once */
#define HIOIncluded	1

#include "c.h"

#include "block.h"
#include "htup.h"
#include "rel.h"

/*
 * RelationPutHeapTuple --
 *	Places a heap tuple in a specified disk block.
 *
 * Note:
 *	Assumes relation is valid.
 *	Assumes tuple is valid.
 *	Assumes block number is valid.
 *	Assumes tuple will fit in the disk block.
 */
extern
void
RelationPutHeapTuple ARGS((
	Relation	relation,
	BlockNumber	blockIndex,
	HeapTuple	tuple
));

/*
 * RelationPutLongHeapTuple --
 *	Places a long heap tuple in a relation.
 *
 * Note:
 *	Assumes relation is valid.
 *	Assumes tuple is valid.
 *	Assumes block number is valid.
 *	Assumes tuple will not fit into a disk block.
 */
extern
void
RelationPutLongHeapTuple ARGS((
	Relation	relation,
	HeapTuple	tuple
));

#endif	/* !defined(HIOIncluded) */
