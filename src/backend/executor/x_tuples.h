/* $Header$ */

extern TupleTable ExecCreateTupleTable ARGS((int initialSize));
extern void ExecDestroyTupleTable ARGS((TupleTable table, bool shouldFree));
extern int ExecAllocTableSlot ARGS((TupleTable table));
extern Pointer ExecGetTableSlot ARGS((TupleTable table, int slotnum));
    
#ifndef EXEC_DEBUGSTORETUP
extern Pointer ExecStoreTuple ARGS((Pointer tuple, Pointer slot, bool shouldFree));
#else    
extern Pointer ExecStoreTupleDebug ARGS((String file, int line, Pointer tuple, Pointer slot, bool shouldFree));
#endif EXEC_DEBUGSTORETUP
    
extern Pointer ExecFetchTuple ARGS((Pointer slot));
extern Pointer ExecClearTuple ARGS((Pointer slot));
extern bool ExecNullSlot ARGS((Pointer slot));
extern bool ExecSlotPolicy ARGS((Pointer slot));
extern bool ExecSetSlotPolicy ARGS((Pointer slot, bool shouldFree));
extern void ExecInitResultTupleSlot ARGS((EState estate, CommonState commonstate));
extern void ExecInitScanTupleSlot ARGS((EState estate, CommonScanState commonscanstate));
extern void ExecInitViewTupleSlot ARGS((EState estate, CommonScanState commonscanstate));
extern void ExecInitMarkedTupleSlot ARGS((MergeJoinState merg estate, int mergestate));
extern void ExecInitSavedTupleSlot ARGS((EState estate, HashJoinState hashstate));
extern void ExecInitTemporaryTupleSlot ARGS((EState estate, HashJoinState hashstate));
extern AttributePtr ExecGetTupType ARGS((Plan node));
extern Buffer ExecGetBuffer ARGS((Plan node));
extern AttributePtr ExecTypeFromTL ARGS((List targetList));
