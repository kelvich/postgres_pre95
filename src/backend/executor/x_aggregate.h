extern void ExecEndAgg ARGS((Agg node));
extern List ExecInitAgg ARGS((Agg node, EState estate, Plan parent));
extern TupleTableSlot ExecAgg ARGS((Agg node));

