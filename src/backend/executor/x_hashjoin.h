/* $Header$ */
extern TupleTableSlot ExecHashJoin ARGS((HashJoin node));
extern List ExecInitHashJoin ARGS((Sort node, EState estate, int level, Plan parent));
extern void ExecEndHashJoin ARGS((HashJoin node));
extern TupleTableSlot ExecHashJoinOuterGetTuple ARGS((Plan node, int curbatch, HashJoinState hjstate));
extern TupleTableSlot ExecHashJoinGetSavedTuple ARGS((HashJoinState hjstate, char *buffer, File file, int *block, char **position));
extern int ExecHashJoinNewBatch ARGS((HashJoinState hjstate, int newbatch));
extern int ExecHashJoinGetBatch ARGS((int bucketno, HashJoinTable hashtable, int nbatch));
extern char *ExecHashJoinSaveTuple ARGS((HeapTuple heapTuple, char *buffer, File file, char *position));
