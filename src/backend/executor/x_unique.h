/* ----------------------------------------------------------------
 *   FILE
 *	n_unique.h
 *
 *   DESCRIPTION
 *	prototypes for n_unique.c.
 *
 *   NOTES
 *	Automatically generated using mkproto
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef n_uniqueIncluded		/* include this file only once */
#define n_uniqueIncluded	1

extern bool ExecIdenticalTuples ARGS((List t1, List t2));
extern TupleTableSlot ExecUnique ARGS((Unique node));
extern List ExecInitUnique ARGS((Unique node, EState estate, Plan parent));
extern int ExecCountSlotsUnique ARGS((Plan node));
extern void ExecEndUnique ARGS((Unique node));

#endif /* n_uniqueIncluded */
