/* $Header$ */
extern Pointer FormSortKeys ARGS((Sort sortnode));
extern List ExecSort ARGS((Sort node));
extern List ExecInitSort ARGS((Sort node, EState estate, int level));
extern void ExecEndSort ARGS((Sort node));
extern List ExecSortMarkPos ARGS((Plan node));
extern void ExecSortRestrPos ARGS((Plan node, List pos));
