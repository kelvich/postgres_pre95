/* ----------------------------------------------------------------
 *   FILE
 *	aggregate.h
 *
 *   DESCRIPTION
 *	prototypes for aggregate.c.
 *
 *   NOTES
 *	Automatically generated using mkproto
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef aggregateIncluded		/* include this file only once */
#define aggregateIncluded	1

extern TupleTableSlot ExecAgg ARGS((Agg node));
extern List ExecInitAgg ARGS((Agg node, EState estate, Plan parent));
extern int ExecCountSlotsAgg ARGS((Plan node));
extern void ExecEndAgg ARGS((Agg node));

#endif /* aggregateIncluded */
