/* ----------------------------------------------------------------
 *   FILE
 *	n_hashjoin.h
 *
 *   DESCRIPTION
 *	prototypes for n_hashjoin.c.
 *
 *   NOTES
 *	Automatically generated using mkproto
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef n_hashjoinIncluded		/* include this file only once */
#define n_hashjoinIncluded	1

extern TupleTableSlot ExecHashJoin ARGS((HashJoin node));
extern List ExecInitHashJoin ARGS((HashJoin node, EState estate, Plan parent));
extern int ExecCountSlotsHashJoin ARGS((Plan node));
extern void ExecEndHashJoin ARGS((HashJoin node));
extern TupleTableSlot ExecHashJoinOuterGetTuple ARGS((Plan node, HashJoinState hjstate));
extern TupleTableSlot ExecHashJoinGetSavedTuple ARGS((HashJoinState hjstate, char *buffer, File file, Pointer tupleSlot, int *block, char **position));
extern int ExecHashJoinNewBatch ARGS((HashJoinState hjstate));
extern int ExecHashJoinGetBatch ARGS((int bucketno, HashJoinTable hashtable, int nbatch));
extern char *ExecHashJoinSaveTuple ARGS((HeapTuple heapTuple, char *buffer, File file, char *position));

#endif /* n_hashjoinIncluded */
