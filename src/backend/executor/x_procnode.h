/* ----------------------------------------------------------------
 *   FILE
 *	ex_procnode.h
 *
 *   DESCRIPTION
 *	prototypes for ex_procnode.c.
 *
 *   NOTES
 *	Automatically generated using mkproto
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef ex_procnodeIncluded		/* include this file only once */
#define ex_procnodeIncluded	1

extern List ExecInitNode ARGS((Plan node, EState estate, Plan parent));
extern TupleTableSlot ExecProcNode ARGS((Plan node));
extern int ExecCountSlotsNode ARGS((Plan node));
extern void ExecEndNode ARGS((Plan node));

#endif /* ex_procnodeIncluded */
