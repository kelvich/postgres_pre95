/* ----------------------------------------------------------------
 *   FILE
 *	n_mergejoin.h
 *
 *   DESCRIPTION
 *	prototypes for n_mergejoin.c.
 *
 *   NOTES
 *	Automatically generated using mkproto
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef n_mergejoinIncluded		/* include this file only once */
#define n_mergejoinIncluded	1

extern List MJFormOSortopI ARGS((List qualList, ObjectId sortOp));
extern List MJFormISortopO ARGS((List qualList, ObjectId sortOp));
extern bool MergeCompare ARGS((List eqQual, List compareQual, ExprContext econtext));
extern void ExecMergeTupleDumpInner ARGS((ExprContext econtext));
extern void ExecMergeTupleDumpOuter ARGS((ExprContext econtext));
extern void ExecMergeTupleDumpMarked ARGS((ExprContext econtext, MergeJoinState mergestate));
extern void ExecMergeTupleDump ARGS((ExprContext econtext, MergeJoinState mergestate));
extern TupleTableSlot ExecMergeJoin ARGS((MergeJoin node));
extern List ExecInitMergeJoin ARGS((MergeJoin node, EState estate, Plan parent));
extern int ExecCountSlotsMergeJoin ARGS((Plan node));
extern void ExecEndMergeJoin ARGS((MergeJoin node));

#endif /* n_mergejoinIncluded */
