/* $Header$ */
extern bool ExecIdenticalTuples ARGS((List t1, List t2));
extern List ExecUnique ARGS((Unique node));
extern List ExecInitUnique ARGS((Unique node, EState estate, int level));
extern void ExecEndUnique ARGS((Unique node));
