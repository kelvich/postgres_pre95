/* $Header$ */
extern void ExecHashInsert ARGS((Relation hashRelation, HeapTuple tuple));
extern List ExecHashJoin ARGS((HashJoin node));
extern List ExecInitHashJoin ARGS((Sort node, EState estate, int level));
extern void ExecEndHashJoin ARGS((HashJoin node));
