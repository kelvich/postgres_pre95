/* $Header$ */
extern List exec_append_initialize_next ARGS((Append node, int level));
extern List ExecInitAppend ARGS((Append node, EState estate, int level, Plan parent));
extern TupleTableSlot ExecProcAppend ARGS((Append node));
extern void ExecEndAppend ARGS((Append node));
