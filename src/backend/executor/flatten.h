
/*
 * flatten.h -
 *    header file for target list flattening routines
 *
 * $Header$
 */

#ifndef _FLATTEN_INCL_
#define _FLATTEN_INCL_

Datum ExecEvalIter ARGS((Iter iterNode , bool *funcIsDone , bool *resultIsNull ));
void ExecEvalFjoin ARGS((List tlist , bool *isNullVect , bool *fj_isDone ));
bool FjoinBumpOuterNodes ARGS((List tlist , DatumPtr results , String nulls ));

#endif _FLATTEN_INCL_
