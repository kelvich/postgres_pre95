/* ----------------------------------------------------------------
 *   FILE
 *	functions.h
 *
 *   DESCRIPTION
 *	prototypes for functions.c.
 *
 *   NOTES
 *	Automatically generated using mkproto
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef functionsIncluded		/* include this file only once */
#define functionsIncluded	1

extern Datum ProjectAttribute ARGS((TupleDescriptor TD, List tlist, HeapTuple tup, Boolean *isnullP));
extern TupleTableSlot copy_function_result ARGS((FunctionCachePtr fcache, TupleTableSlot resultSlot));
extern Datum postquel_function ARGS((Func funcNode, char *args[], bool *isNull, bool *isDone));

#endif /* functionsIncluded */
