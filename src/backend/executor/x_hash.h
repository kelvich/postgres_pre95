/* $Header$ */
extern TupleTableSlot ExecHash ARGS((Hash node));
extern List ExecInitHash ARGS((Hash node, EState estate, int level, Plan parent));
extern void ExecEndHash ARGS((Hash node));
extern HashJoinTable ExecHashTableCreate ARGS((Plan node, int nbatch));
extern void ExecHashTableInsert ARGS((HashJoinTable hashtable, ExprContext econtext, Var hashkey, HashState hashstate));
extern void ExecHashTableDestroy ARGS((HashJoinTable hashtable));
extern int ExecHashGetBucket ARGS((HashJoinTable hashtable, ExprContext econtext, Var hashkey));
extern void ExecHashOverflowInsert ARGS((HashJoinTable hashtable, HashBucket bucket, HeapTuple heapTuple));
extern HeapTuple ExecScanHashBucket ARGS((HashJoinState hjstate, HashBucket bucket, HeapTuple curtuple, List hjclauses, ExprContext econtext));
extern int hashFunc ARGS((char *key, int len));
extern int ExecHashPartition ARGS((Hash node));
extern void ExecHashTableReset ARGS((HashJoinTable hashtable, int ntuples));
