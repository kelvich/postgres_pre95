/* $Header$ */
extern List exec_append_initialize_next ARGS((Append node, int level));
extern List ExecInitAppend ARGS((Append node, EState estate, int level));
extern List ExecProcAppend ARGS((Append node));
extern void ExecEndAppend ARGS((Append node));
