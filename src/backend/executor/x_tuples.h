/* ----------------------------------------------------------------
 *   FILE
 *	ex_tuples.h
 *
 *   DESCRIPTION
 *	prototypes for ex_tuples.c.
 *
 *   NOTES
 *	Automatically generated using mkproto
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef ex_tuplesIncluded		/* include this file only once */
#define ex_tuplesIncluded	1

extern TupleTable ExecCreateTupleTable ARGS((int initialSize));
extern void ExecDestroyTupleTable ARGS((TupleTable table, bool shouldFree));
extern int ExecAllocTableSlot ARGS((TupleTable table));
extern Pointer ExecGetTableSlot ARGS((TupleTable table, int slotnum));
extern Pointer ExecStoreTuple ARGS((Pointer tuple, Pointer slot, Buffer buffer, bool shouldFree));
extern Pointer ExecStoreTupleDebug ARGS((String file, int line, Pointer tuple, Pointer slot, Buffer buffer, bool shouldFree));
extern Pointer ExecClearTuple ARGS((Pointer slot));
extern bool ExecSlotPolicy ARGS((Pointer slot));
extern bool ExecSetSlotPolicy ARGS((Pointer slot, bool shouldFree));
extern TupleDescriptor ExecSetSlotDescriptor ARGS((Pointer slot, TupleDescriptor tupdesc));
extern void ExecSetSlotDescriptorIsNew ARGS((Pointer slot, bool isNew));
extern TupleDescriptor ExecSetNewSlotDescriptor ARGS((Pointer slot, TupleDescriptor tupdesc));
extern Buffer ExecSetSlotBuffer ARGS((Pointer slot, Buffer b));
extern void ExecIncrSlotBufferRefcnt ARGS((Pointer slot));
extern bool ExecNullSlot ARGS((Pointer slot));
extern bool ExecSlotDescriptorIsNew ARGS((Pointer slot));
extern void ExecInitResultTupleSlot ARGS((EState estate, CommonState commonstate));
extern void ExecInitScanTupleSlot ARGS((EState estate, CommonScanState commonscanstate));
extern void ExecInitRawTupleSlot ARGS((EState estate, CommonScanState commonscanstate));
extern void ExecInitMarkedTupleSlot ARGS((EState estate, MergeJoinState mergestate));
extern void ExecInitOuterTupleSlot ARGS((EState estate, HashJoinState hashstate));
extern void ExecInitHashTupleSlot ARGS((EState estate, HashJoinState hashstate));
extern TupleTableSlot NodeGetResultTupleSlot ARGS((Plan node));
extern TupleDescriptor ExecGetTupType ARGS((Plan node));
extern TupleDescriptor ExecCopyTupType ARGS((TupleDescriptor td, int natts));
extern ExecTupDescriptor ExecGetExecTupDesc ARGS((Plan node));
extern TupleDescriptor ExecTypeFromTL ARGS((List targetList));
extern TupleDescriptor ExecTupDescToTupDesc ARGS((ExecTupDescriptor execTupDesc, int len));
extern ExecTupDescriptor TupDescToExecTupDesc ARGS((TupleDescriptor tupDesc, int len));

#endif /* ex_tuplesIncluded */
