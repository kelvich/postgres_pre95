/* $Header$ */
extern List ExecMaterial ARGS((Material node));
extern List ExecInitMaterial ARGS((Sort node, EState estate, int level));
extern void ExecEndMaterial ARGS((Material node));
extern List ExecMaterialMarkPos ARGS((Material node));
extern void ExecMaterialRestrPos ARGS((Material node, List pos));
