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

typedef char    **charPP;
typedef int     *intP;

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
#ifdef sequent
	slock_t		overflowlock;
#endif
	char		*batch;
	char		*readbuf;
        int             nbatch;
#ifdef sequent
	slock_t		*batchLock;
	sbarrier_t	batchBarrier;
	slock_t		tableLock;
#endif
        charPP          outerbatchNames;
        charPP          outerbatchPos;
        charPP          innerbatchNames;
	charPP		innerbatchPos;
        intP            innerbatchSizes;
        int             curbatch;
	int		nprocess;
	int		pcount;
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
#ifdef sequent
	slock_t		bucketlock;
#endif
} HashBucketData;  /* real bucket follows here */

typedef HashBucketData	*HashBucket;

typedef struct HashBufferData {
	int	pageend;
#ifdef sequent
	slock_t	bufferlock;
#endif
} HashBufferData;  /* real buffer follows here */

typedef HashBufferData *HashBuffer;

#endif	/* !defined(HashJoinIncluded) */
