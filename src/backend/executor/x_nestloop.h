/* ----------------------------------------------------------------
 *   FILE
 *	n_nestloop.h
 *
 *   DESCRIPTION
 *	prototypes for n_nestloop.c.
 *
 *   NOTES
 *	Automatically generated using mkproto
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef n_nestloopIncluded		/* include this file only once */
#define n_nestloopIncluded	1

extern TupleTableSlot ExecNestLoop ARGS((NestLoop node));
extern List ExecInitNestLoop ARGS((NestLoop node, EState estate, Plan parent));
extern int ExecCountSlotsNestLoop ARGS((Plan node));
extern List ExecEndNestLoop ARGS((NestLoop node));

#endif /* n_nestloopIncluded */
