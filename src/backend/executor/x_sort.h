/* $Header$ */
extern Pointer FormSortKeys ARGS((Sort sortnode));
extern TupleTableSlot ExecSort ARGS((Sort node));
extern List ExecInitSort ARGS((Sort node, EState estate, int level, Plan parent));
extern void ExecEndSort ARGS((Sort node));
extern List ExecSortMarkPos ARGS((Plan node));
extern void ExecSortRestrPos ARGS((Plan node));
