/*
 * execnodes.a.h --
 *	This came from Cimarron's directory.  -hirohama
 *
 * Identification:
 *	$Header$
 */

#ifndef	ExecNodesADefined		/* Include this file only once */
#define ExecNodesADefined	1

extern void set_direction ARGS((EState node, ScanDirection value));
extern ScanDirection get_direction ARGS((EState node));
extern void set_time ARGS((EState node, abstime value));
extern abstime get_time ARGS((EState node));
extern void set_owner ARGS((EState node, ObjectId value));
extern ObjectId get_owner ARGS((EState node));
extern void set_locks ARGS((EState node, List value));
extern List get_locks ARGS((EState node));
extern void set_subplan_info ARGS((EState node, List value));
extern List get_subplan_info ARGS((EState node));
extern void set_error_message ARGS((EState node, Name value));
extern Name get_error_message ARGS((EState node));
extern void set_range_table ARGS((EState node, List value));
extern List get_range_table ARGS((EState node));
extern void set_qualification_tuple ARGS((EState node, HeapTuple value));
extern HeapTuple get_qualification_tuple ARGS((EState node));
extern void set_qualification_tuple_id ARGS((EState node, ItemPointer value));
extern ItemPointer get_qualification_tuple_id ARGS((EState node));
extern void set_relation_relation_descriptor ARGS((EState node, Relation value));
extern Relation get_relation_relation_descriptor ARGS((EState node));
extern void set_result_relation ARGS((EState node, ObjectId value));
extern ObjectId get_result_relation ARGS((EState node));
extern void set_result_relation_descriptor ARGS((EState node, Relation value));
extern Relation get_result_relation_descriptor ARGS((EState node));
extern void set_LeftTuple ARGS((StateNode node, List value));
extern List get_LeftTuple ARGS((StateNode node));
extern void set_TupType ARGS((StateNode node, AttributePtr value));
extern AttributePtr get_TupType ARGS((StateNode node));
extern void set_TupValue ARGS((StateNode node, Pointer value));
extern Pointer get_TupValue ARGS((StateNode node));
extern void set_Level ARGS((StateNode node, int value));
extern int get_Level ARGS((StateNode node));
extern void set_SatState ARGS((ExistentialState node, List value));
extern List get_SatState ARGS((ExistentialState node));
extern void set_whichplan ARGS((AppendState node, int value));
extern int get_whichplan ARGS((AppendState node));
extern void set_nplans ARGS((AppendState node, int value));
extern int get_nplans ARGS((AppendState node));
extern void set_initialzed ARGS((AppendState node, List value));
extern List get_initialzed ARGS((AppendState node));
extern void set_rtentries ARGS((AppendState node, List value));
extern List get_rtentries ARGS((AppendState node));
extern void set_BufferPage ARGS((CommonState node, List value));
extern List get_BufferPage ARGS((CommonState node));
extern void set_OuterTuple ARGS((CommonState node, List value));
extern List get_OuterTuple ARGS((CommonState node));
extern void set_PortalFlag ARGS((CommonState node, List value));
extern List get_PortalFlag ARGS((CommonState node));
extern void set_ProcLeftFlag ARGS((ScanState node, List value));
extern List get_ProcLeftFlag ARGS((ScanState node));
extern void set_LispIndexPtr ARGS((ScanState node, List value));
extern List get_LispIndexPtr ARGS((ScanState node));
extern void set_SkeysCount ARGS((ScanState node, List value));
extern List get_SkeysCount ARGS((ScanState node));
extern void set_FrwdMarkPos ARGS((MergeJoinState node, List value));
extern List get_FrwdMarkPos ARGS((MergeJoinState node));
extern void set_Flag ARGS((SortState node, List value));
extern List get_Flag ARGS((SortState node));
extern void set_Keys ARGS((SortState node, List value));
extern List get_Keys ARGS((SortState node));
extern EState MakeEState ARGS((ScanDirection direction, abstime time, ObjectId owner, List locks, List subplan_info, Name error_message, List range_table, HeapTuple qualification_tuple, ItemPointer qualification_tuple_id, Relation relation_relation_descriptor, ObjectId result_relation, Relation result_relation_descriptor));
extern void PrintEState ARGS((EState node));
extern bool EqualEState ARGS((EState a, EState b));
extern StateNode MakeStateNode ARGS((List LeftTuple, AttributePtr TupType, Pointer TupValue, int Level));
extern void PrintStateNode ARGS((StateNode node));
extern bool EqualStateNode ARGS((StateNode a, StateNode b));
extern ExistentialState MakeExistentialState ARGS((List SatState));
extern void PrintExistentialState ARGS((ExistentialState node));
extern bool EqualExistentialState ARGS((ExistentialState a, ExistentialState b));
extern ResultState MakeResultState ARGS((int Loop));
extern void PrintResultState ARGS((ResultState node));
extern bool EqualResultState ARGS((ResultState a, ResultState b));
extern AppendState MakeAppendState ARGS((int whichplan, int nplans, List initialzed, List rtentries));
extern void PrintAppendState ARGS((AppendState node));
extern bool EqualAppendState ARGS((AppendState a, AppendState b));
extern CommonState MakeCommonState ARGS((List BufferPage, List OuterTuple, List PortalFlag));
extern void PrintCommonState ARGS((CommonState node));
extern bool EqualCommonState ARGS((CommonState a, CommonState b));
extern NestLoopState MakeNestLoopState ARGS((int BufferPage));
extern void PrintNestLoopState ARGS((NestLoopState node));
extern bool EqualNestLoopState ARGS((NestLoopState a, NestLoopState b));
extern ScanState MakeScanState ARGS((List RuleFlag, List RuleDesc, List ProcLeftFlag, List OldRelId, List LispIndexPtr, List Skeys, List SkeysCount));
extern void PrintScanState ARGS((ScanState node));
extern bool EqualScanState ARGS((ScanState a, ScanState b));
extern MergeJoinState MakeMergeJoinState ARGS((List OSortopI, List ISortopO, List MarkFlag, List FrwdMarkPos, List BkwdMarkPos));
extern void PrintMergeJoinState ARGS((MergeJoinState node));
extern bool EqualMergeJoinState ARGS((MergeJoinState a, MergeJoinState b));
extern SortState MakeSortState ARGS((List Flag, List Keys));
extern void PrintSortState ARGS((SortState node));
extern bool EqualSortState ARGS((SortState a, SortState b));

#endif	/* !defined(ExecNodesADefined) */
