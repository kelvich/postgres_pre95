/*
 * hashjoin.h --
 *	internal structures for hash table and buckets
 */

#ifndef	HashJoinIncluded		/* Include this file only once */
#define HashJoinIncluded	1

/*
 * Identification:
 */
#define HASHJOIN_H

#ifndef HTUP_H
#include "htup.h"
#endif
#ifndef IPC_H
#include "ipc.h"
#endif

typedef struct HashTableData {
	int		nbuckets;
	int		bucketsize;
	IpcMemoryId	shmid;
	char		*top;
	char		*bottom;
	char		*overflownext;
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
HashBucket
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

#endif	/* !defined(HashJoinIncluded) */
