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

/* ----------------------------------------------------------------
 *			extern declarations
 * ----------------------------------------------------------------
 */
extern
List
ExecHashJoin ARGS((
	HashJoin node
));

extern
List
ExecInitHashJoin ARGS((
	HashJoin node;
	EState estate;
	int level
));

extern
void
ExecEndHashJoin ARGS((
	HashJoin node
));

extern
List
ExecHash ARGS((
	Hash node
));

extern
List
ExecInitHash ARGS((
	Hash node;
	EState estate;
	int level
));

extern
void
ExecEndHash ARGS((
	Hash node
));

extern
HashJoinTable
ExecHashTableCreate ARGS((
	Plan node
));

extern
void
ExecHashTableInsert ARGS((
	HashJoinTable hashtable;
	ExprContext econtext;
	Var hashkey
));

extern
void
ExecHashTableDestroy ARGS((
	HashJoinTable hashtable
));

extern
int
ExecHashGetBucket ARGS((
	HashJoinTable hashtable;
	ExprContext econtext;
	Var hashkey
));

extern
void
ExecHashOverflowInsert ARGS((
	HashJoinTable hashtable;
	HashBucket bucket;
	List tuple
));

extern
HeapTuple
ExecScanHashBucket ARGS((
	HashBucket bucket;
	HeapTuple curtuple;
	List hjclauses;
	ExprContext econtext
));

extern
int
hashFunc ARGS((
	char *key;
	int len
));

extern
int
ExecHashPartition ARGS((
	Hash node
));

extern
void
ExecHashTableReset ARGS((
	HashJoinTable hashtable;
	int ntuple
));

extern
List
ExecHashJoinOuterGetTuple ARGS((
	Plan node;
	int curbatch;
	HashJoinState hjstate
));

extern
List
ExecHashJoinGetSavedTuple ARGS((
	char *buffer;
	File file;
	int *block;
	char **position
));

extern
void
ExecHashJoinNewBatch ARGS((
	HashJoinState hjstate;
	int newbatch
));

extern
int
ExecHashJoinGetBatch ARGS((
	int bucketno;
	HashJoinTable hashtable;
	int nbatch
));

extern
char *
ExecHashJoinSaveTuple ARGS((
	List tuple;
	char *buffer;
	File file;
	char *position
));

#endif	/* !defined(HashJoinIncluded) */
