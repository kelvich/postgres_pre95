/* $Header$ */
extern List ExecNestLoop ARGS((NestLoop node));
extern List ExecInitNestLoop ARGS((NestLoop node, EState estate, int level));
extern List ExecEndNestLoop ARGS((NestLoop node));
