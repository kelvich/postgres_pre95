/* ----------------------------------------------------------------
 *   FILE
 *	n_append.h
 *
 *   DESCRIPTION
 *	prototypes for n_append.c.
 *
 *   NOTES
 *	Automatically generated using mkproto
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef n_appendIncluded		/* include this file only once */
#define n_appendIncluded	1

extern List exec_append_initialize_next ARGS((Append node));
extern List ExecInitAppend ARGS((Append node, EState estate, Plan parent));
extern int ExecCountSlotsAppend ARGS((Plan node));
extern TupleTableSlot ExecProcAppend ARGS((Append node));
extern void ExecEndAppend ARGS((Append node));

#endif /* n_appendIncluded */
