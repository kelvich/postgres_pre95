/* $Header$ */
extern TupleTableSlot ExecNestLoop ARGS((NestLoop node));
extern List ExecInitNestLoop ARGS((NestLoop node, EState estate, int level, Plan parent));
extern List ExecEndNestLoop ARGS((NestLoop node));
