/* ----------------------------------------------------------------
 *   FILE
 *	n_hash.h
 *
 *   DESCRIPTION
 *	prototypes for n_hash.c.
 *
 *   NOTES
 *	Automatically generated using mkproto
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef n_hashIncluded		/* include this file only once */
#define n_hashIncluded	1

extern TupleTableSlot ExecHash ARGS((Hash node));
extern List ExecInitHash ARGS((Hash node, EState estate, Plan parent));
extern int ExecCountSlotsHash ARGS((Plan node));
extern void ExecEndHash ARGS((Hash node));
extern RelativeAddr hashTableAlloc ARGS((int size, HashJoinTable hashtable));
extern HashJoinTable ExecHashTableCreate ARGS((Plan node));
extern void ExecHashTableInsert ARGS((HashJoinTable hashtable, ExprContext econtext, Var hashkey, File *batches));
extern void ExecHashTableDestroy ARGS((HashJoinTable hashtable));
extern int ExecHashGetBucket ARGS((HashJoinTable hashtable, ExprContext econtext, Var hashkey));
extern void ExecHashOverflowInsert ARGS((HashJoinTable hashtable, HashBucket bucket, HeapTuple heapTuple));
extern HeapTuple ExecScanHashBucket ARGS((HashJoinState hjstate, HashBucket bucket, HeapTuple curtuple, List hjclauses, ExprContext econtext));
extern int hashFunc ARGS((char *key, int len));
extern int ExecHashPartition ARGS((Hash node));
extern void ExecHashTableReset ARGS((HashJoinTable hashtable, int ntuples));
extern void mk_hj_temp ARGS((char *tempname));
extern int print_buckets ARGS((HashJoinTable hashtable));

#endif /* n_hashIncluded */
