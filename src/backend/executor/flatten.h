/* ----------------------------------------------------------------
 *   FILE
 *	ex_flatten.h
 *
 *   DESCRIPTION
 *	prototypes for ex_flatten.c.
 *
 *   NOTES
 *	Automatically generated using mkproto
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef ex_flattenIncluded		/* include this file only once */
#define ex_flattenIncluded	1

extern Datum ExecEvalIter ARGS((Iter iterNode, ExprContext econtext, bool *resultIsNull, bool *iterIsDone));
extern void ExecEvalFjoin ARGS((List tlist, ExprContext econtext, bool *isNullVect, bool *fj_isDone));
extern bool FjoinBumpOuterNodes ARGS((List tlist, ExprContext econtext, DatumPtr results, String nulls));

#endif /* ex_flattenIncluded */
