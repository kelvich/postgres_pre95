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
#include "storage/ipci.h"
#include "storage/ipc.h"

/* -----------------
 *  have to use relative address as pointers in the hashtable
 *  because the hashtable may reallocate in difference processes
 * -----------------
 */
typedef int	RelativeAddr;

/* ------------------
 *  the relative addresses are always relative to the head of the
 *  hashtable, the following macro converts them to absolute address.
 * ------------------
 */
#define ABSADDR(X)	((X) < 0 ? NULL: (char*)hashtable + X)
#define RELADDR(X)	(RelativeAddr)((char*)(X) - (char*)hashtable)

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
	RelativeAddr	top;			/* char* */
	RelativeAddr	bottom;			/* char* */
	RelativeAddr	overflownext;		/* char* */
#ifdef sequent
	slock_t		overflowlock;
#endif
	RelativeAddr	batch;			/* char* */
	RelativeAddr	readbuf;		/* char* */
        int             nbatch;
#ifdef sequent
	RelativeAddr	batchLock;		/* slock_t* */
	sbarrier_t	batchBarrier;
	slock_t		tableLock;
#endif
        RelativeAddr    outerbatchNames;	/* RelativeAddr* */
        RelativeAddr    outerbatchPos;		/* RelativeAddr* */
        RelativeAddr    innerbatchNames;	/* RelativeAddr* */
	RelativeAddr	innerbatchPos;		/* RelativeAddr* */
        RelativeAddr    innerbatchSizes;	/* int* */
        int             curbatch;
	int		nprocess;
	int		pcount;
} HashTableData;  /* real hash table follows here */

typedef HashTableData	*HashJoinTable;

typedef struct OverflowTupleData {
	RelativeAddr tuple;		/* HeapTuple */
	RelativeAddr next;		/* struct OverflowTupleData * */
} OverflowTupleData;   /* real tuple follows here */

typedef OverflowTupleData *OverflowTuple;

typedef struct HashBucketData {
	RelativeAddr	top;		/* HeapTuple */
	RelativeAddr	bottom;		/* HeapTuple */
	RelativeAddr	firstotuple;	/* OverflowTuple */
	RelativeAddr	lastotuple;	/* OverflowTuple */
#ifdef sequent
	slock_t		bucketlock;
#endif
} HashBucketData;  /* real bucket follows here */

typedef HashBucketData	*HashBucket;

#define HASH_PERMISSION         0700

#endif	/* !defined(HashJoinIncluded) */
