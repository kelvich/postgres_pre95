/* ----------------------------------------------------------------
 *   FILE
 *	n_material.h
 *
 *   DESCRIPTION
 *	prototypes for n_material.c.
 *
 *   NOTES
 *	Automatically generated using mkproto
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef n_materialIncluded		/* include this file only once */
#define n_materialIncluded	1

extern TupleTableSlot ExecMaterial ARGS((Material node));
extern List ExecInitMaterial ARGS((Material node, EState estate, Plan parent));
extern int ExecCountSlotsMaterial ARGS((Plan node));
extern void ExecEndMaterial ARGS((Material node));
extern List ExecMaterialMarkPos ARGS((Material node));
extern void ExecMaterialRestrPos ARGS((Material node));

#endif /* n_materialIncluded */
