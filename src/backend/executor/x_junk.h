/* ----------------------------------------------------------------
 *   FILE
 *	ex_junk.h
 *
 *   DESCRIPTION
 *	prototypes for ex_junk.c.
 *
 *   NOTES
 *	Automatically generated using mkproto
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef ex_junkIncluded		/* include this file only once */
#define ex_junkIncluded	1

extern JunkFilter ExecInitJunkFilter ARGS((List targetList));
extern bool ExecGetJunkAttribute ARGS((JunkFilter junkfilter, TupleTableSlot slot, Name attrName, Datum *value, Boolean *isNull));
extern HeapTuple ExecRemoveJunk ARGS((JunkFilter junkfilter, TupleTableSlot slot));

#endif /* ex_junkIncluded */
