/* $Header$ */
extern bool ExecIdenticalTuples ARGS((List t1, List t2));
extern TupleTableSlot ExecUnique ARGS((Unique node));
extern List ExecInitUnique ARGS((Unique node, EState estate, int level, Plan parent));
extern void ExecEndUnique ARGS((Unique node));
