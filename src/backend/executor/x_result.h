/* ----------------------------------------------------------------
 *   FILE
 *	n_result.h
 *
 *   DESCRIPTION
 *	prototypes for n_result.c.
 *
 *   NOTES
 *	Automatically generated using mkproto
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef n_resultIncluded		/* include this file only once */
#define n_resultIncluded	1

extern TupleTableSlot ExecResult ARGS((Result node));
extern List ExecInitResult ARGS((Plan node, EState estate, Plan parent));
extern int ExecCountSlotsResult ARGS((Plan node));
extern void ExecEndResult ARGS((Result node));

#endif /* n_resultIncluded */
