/* $Header$ */
extern TupleTableSlot ExecResult ARGS((Result node));
extern List ExecInitResult ARGS((Plan node, EState estate, int level, Plan parent));
extern void ExecEndResult ARGS((Result node));
