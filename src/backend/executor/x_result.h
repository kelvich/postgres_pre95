/* $Header$ */
extern List ExecResult ARGS((Result node));
extern List ExecInitResult ARGS((Plan node, EState estate, int level));
extern void ExecEndResult ARGS((Result node));
