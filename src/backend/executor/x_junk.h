
/* $Header$ */

extern JunkFilter ExecInitJunkFilter ARGS((List targetList));
extern bool ExecGetJunkAttribute ARGS((JunkFilter junkfilter, TupleTableSlot slot, Name attrName, Datum *value, Boolean *isNull));
extern HeapTuple ExecRemoveJunk ARGS(( JunkFilter junkfilter, TupleTableSlot slot));
