/* ----------------------------------------------------------------
 *   FILE
 *	n_sort.h
 *
 *   DESCRIPTION
 *	prototypes for n_sort.c.
 *
 *   NOTES
 *	Automatically generated using mkproto
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef n_sortIncluded		/* include this file only once */
#define n_sortIncluded	1

extern Pointer FormSortKeys ARGS((Sort sortnode));
extern TupleTableSlot ExecSort ARGS((Sort node));
extern List ExecInitSort ARGS((Sort node, EState estate, Plan parent));
extern int ExecCountSlotsSort ARGS((Plan node));
extern void ExecEndSort ARGS((Sort node));
extern List ExecSortMarkPos ARGS((Plan node));
extern void ExecSortRestrPos ARGS((Plan node));

#endif /* n_sortIncluded */
