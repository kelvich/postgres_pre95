/* ----------------------------------------------------------------
 *   FILE
 *     	hashjoin.h
 *
 *   DESCRIPTION
 *     	internal structures for hash table and buckets
 *
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef	HashJoinIncluded	/* Include this file only once */
#define HashJoinIncluded	1

/*
 * Identification:
 */
#define HASHJOIN_H

#include "access/htup.h"
#include "storage/ipc.h"

/* ----------------------------------------------------------------
 *		hash-join hash table structures
 * ----------------------------------------------------------------
 */
typedef struct HashTableData {
	int		nbuckets;
	int		totalbuckets;
	int		bucketsize;
	IpcMemoryId	shmid;
	char		*top;
	char		*bottom;
	char		*overflownext;
	char		*batch;
	char		*readbuf;
} HashTableData;  /* real hash table follows here */

typedef HashTableData	*HashJoinTable;

typedef struct OverflowTupleData {
	HeapTuple tuple;
	struct OverflowTupleData *next;
} OverflowTupleData;   /* real tuple follows here */

typedef OverflowTupleData *OverflowTuple;

typedef struct HashBucketData {
	HeapTuple	top;
	HeapTuple	bottom;
	OverflowTuple	firstotuple;
	OverflowTuple	lastotuple;
} HashBucketData;  /* real bucket follows here */

typedef HashBucketData	*HashBucket;

#endif	/* !defined(HashJoinIncluded) */
