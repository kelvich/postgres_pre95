/* $Header$ */

extern void RInitIndexInfo ARGS((Pointer p));
extern IndexInfo MakeIndexInfo ARGS((int ii_NumKeyAttributes, AttributeNumberPtr ii_KeyAttributeNumbers));
extern void OutIndexInfo ARGS((StringInfo str, IndexInfo node));
extern bool EqualIndexInfo ARGS((IndexInfo a, IndexInfo b));
extern bool CopyIndexInfo ARGS((IndexInfo from, IndexInfo *to, int alloc));
extern IndexInfo IMakeIndexInfo ARGS((int ii_NumKeyAttributes, AttributeNumberPtr ii_KeyAttributeNumbers));
extern void RInitRelationInfo ARGS((Pointer p));
extern RelationInfo MakeRelationInfo ARGS((Index ri_RangeTableIndex, Relation ri_RelationDesc, int ri_NumIndices, RelationPtr ri_IndexRelationDescs, IndexInfoPtr ri_IndexRelationInfo));
extern void OutRelationInfo ARGS((StringInfo str, RelationInfo node));
extern bool EqualRelationInfo ARGS((RelationInfo a, RelationInfo b));
extern bool CopyRelationInfo ARGS((RelationInfo from, RelationInfo *to, int alloc));
extern RelationInfo IMakeRelationInfo ARGS((Index ri_RangeTableIndex, Relation ri_RelationDesc, int ri_NumIndices, RelationPtr ri_IndexRelationDescs, IndexInfoPtr ri_IndexRelationInfo));
extern void RInitTupleCount ARGS((Pointer p));
extern TupleCount MakeTupleCount ARGS((int tc_retrieved, int tc_appended, int tc_deleted, int tc_replaced, int tc_inserted, int tc_processed));
extern void OutTupleCount ARGS((StringInfo str, TupleCount node));
extern bool EqualTupleCount ARGS((TupleCount a, TupleCount b));
extern bool CopyTupleCount ARGS((TupleCount from, TupleCount *to, int alloc));
extern TupleCount IMakeTupleCount ARGS((int tc_retrieved, int tc_appended, int tc_deleted, int tc_replaced, int tc_inserted, int tc_processed));
extern void RInitTupleTableSlot ARGS((Pointer p));
extern TupleTableSlot MakeTupleTableSlot ARGS((bool ttc_shouldFree, bool ttc_descIsNew, TupleDescriptor ttc_tupleDescriptor, ExecTupDescriptor ttc_execTupDescriptor, Buffer ttc_buffer));
extern void OutTupleTableSlot ARGS((StringInfo str, TupleTableSlot node));
extern bool EqualTupleTableSlot ARGS((TupleTableSlot a, TupleTableSlot b));
extern bool CopyTupleTableSlot ARGS((TupleTableSlot from, TupleTableSlot *to, int alloc));
extern TupleTableSlot IMakeTupleTableSlot ARGS((bool ttc_shouldFree, bool ttc_descIsNew, TupleDescriptor ttc_tupleDescriptor, ExecTupDescriptor ttc_execTupDescriptor, Buffer ttc_buffer));
extern void RInitExprContext ARGS((Pointer p));
extern ExprContext MakeExprContext ARGS((TupleTableSlot ecxt_scantuple, TupleTableSlot ecxt_innertuple, TupleTableSlot ecxt_outertuple, Relation ecxt_relation, Index ecxt_relid, ParamListInfo ecxt_param_list_info));
extern void OutExprContext ARGS((StringInfo str, ExprContext node));
extern bool EqualExprContext ARGS((ExprContext a, ExprContext b));
extern bool CopyExprContext ARGS((ExprContext from, ExprContext *to, int alloc));
extern ExprContext IMakeExprContext ARGS((TupleTableSlot ecxt_scantuple, TupleTableSlot ecxt_innertuple, TupleTableSlot ecxt_outertuple, Relation ecxt_relation, Index ecxt_relid, ParamListInfo ecxt_param_list_info));
extern void RInitProjectionInfo ARGS((Pointer p));
extern ProjectionInfo MakeProjectionInfo ARGS((List pi_targetlist, int pi_len, Pointer pi_tupValue, ExprContext pi_exprContext, TupleTableSlot pi_slot));
extern void OutProjectionInfo ARGS((StringInfo str, ProjectionInfo node));
extern bool EqualProjectionInfo ARGS((ProjectionInfo a, ProjectionInfo b));
extern bool CopyProjectionInfo ARGS((ProjectionInfo from, ProjectionInfo *to, int alloc));
extern ProjectionInfo IMakeProjectionInfo ARGS((List pi_targetlist, int pi_len, Pointer pi_tupValue, ExprContext pi_exprContext, TupleTableSlot pi_slot));
extern void RInitJunkFilter ARGS((Pointer p));
extern JunkFilter MakeJunkFilter ARGS((List jf_targetList, int jf_length, TupleDescriptor jf_tupType, List jf_cleanTargetList, int jf_cleanLength, TupleDescriptor jf_cleanTupType, AttributeNumberPtr jf_cleanMap));
extern void OutJunkFilter ARGS((StringInfo str, JunkFilter node));
extern bool EqualJunkFilter ARGS((JunkFilter a, JunkFilter b));
extern bool CopyJunkFilter ARGS((JunkFilter from, JunkFilter *to, int alloc));
extern JunkFilter IMakeJunkFilter ARGS((List jf_targetList, int jf_length, TupleDescriptor jf_tupType, List jf_cleanTargetList, int jf_cleanLength, TupleDescriptor jf_cleanTupType, AttributeNumberPtr jf_cleanMap));
extern void RInitEState ARGS((Pointer p));
extern EState MakeEState ARGS((ScanDirection es_direction, abstime es_time, ObjectId es_owner, List es_locks, List es_subplan_info, Name es_error_message, List es_range_table, HeapTuple es_qualification_tuple, ItemPointer es_qualification_tuple_id, Buffer es_qualification_tuple_buffer, HeapTuple es_raw_qualification_tuple, Relation es_relation_relation_descriptor, Relation es_into_relation_descriptor, RelationInfo es_result_relation_info, TupleCount es_tuplecount, ParamListInfo es_param_list_info, Prs2EStateInfo es_prs2_info, Relation es_explain_relation, int es_BaseId, TupleTable es_tupleTable, JunkFilter es_junkFilter, Pointer es_result_rel_scanstate, RelationRuleInfo es_result_rel_ruleinfo));
extern void OutEState ARGS((StringInfo str, EState node));
extern bool EqualEState ARGS((EState a, EState b));
extern bool CopyEState ARGS((EState from, EState *to, int alloc));
extern EState IMakeEState ARGS((ScanDirection es_direction, abstime es_time, ObjectId es_owner, List es_locks, List es_subplan_info, Name es_error_message, List es_range_table, HeapTuple es_qualification_tuple, ItemPointer es_qualification_tuple_id, Buffer es_qualification_tuple_buffer, HeapTuple es_raw_qualification_tuple, Relation es_relation_relation_descriptor, Relation es_into_relation_descriptor, RelationInfo es_result_relation_info, TupleCount es_tuplecount, ParamListInfo es_param_list_info, Prs2EStateInfo es_prs2_info, Relation es_explain_relation, int es_BaseId, TupleTable es_tupleTable, JunkFilter es_junkFilter, Pointer es_result_rel_scanstate, RelationRuleInfo es_result_rel_ruleinfo));
extern void RInitHookNode ARGS((Pointer p));
extern HookNode MakeHookNode ARGS((HookFunction hook_at_initnode, HookFunction hook_pre_procnode, HookFunction hook_pre_endnode, HookFunction hook_post_initnode, HookFunction hook_post_procnode, HookFunction hook_post_endnode, Pointer hook_data));
extern void OutHookNode ARGS((StringInfo str, HookNode node));
extern bool EqualHookNode ARGS((HookNode a, HookNode b));
extern bool CopyHookNode ARGS((HookNode from, HookNode *to, int alloc));
extern HookNode IMakeHookNode ARGS((HookFunction hook_at_initnode, HookFunction hook_pre_procnode, HookFunction hook_pre_endnode, HookFunction hook_post_initnode, HookFunction hook_post_procnode, HookFunction hook_post_endnode, Pointer hook_data));
extern void RInitBaseNode ARGS((Pointer p));
extern BaseNode MakeBaseNode ARGS((int base_id, Pointer base_parent, Pointer base_parent_state, HookNode base_hook));
extern void OutBaseNode ARGS((StringInfo str, BaseNode node));
extern bool EqualBaseNode ARGS((BaseNode a, BaseNode b));
extern bool CopyBaseNode ARGS((BaseNode from, BaseNode *to, int alloc));
extern BaseNode IMakeBaseNode ARGS((int base_id, Pointer base_parent, Pointer base_parent_state, HookNode base_hook));
extern void RInitCommonState ARGS((Pointer p));
extern CommonState MakeCommonState ARGS((TupleTableSlot cs_OuterTupleSlot, TupleTableSlot cs_ResultTupleSlot, ExprContext cs_ExprContext, ProjectionInfo cs_ProjInfo, int cs_NumScanAttributes, AttributeNumberPtr cs_ScanAttributes));
extern void OutCommonState ARGS((StringInfo str, CommonState node));
extern bool EqualCommonState ARGS((CommonState a, CommonState b));
extern bool CopyCommonState ARGS((CommonState from, CommonState *to, int alloc));
extern CommonState IMakeCommonState ARGS((int base_id, Pointer base_parent, Pointer base_parent_state, HookNode base_hook, TupleTableSlot cs_OuterTupleSlot, TupleTableSlot cs_ResultTupleSlot, ExprContext cs_ExprContext, ProjectionInfo cs_ProjInfo, int cs_NumScanAttributes, AttributeNumberPtr cs_ScanAttributes));
extern void RInitResultState ARGS((Pointer p));
extern ResultState MakeResultState ARGS((int rs_Loop));
extern void OutResultState ARGS((StringInfo str, ResultState node));
extern bool EqualResultState ARGS((ResultState a, ResultState b));
extern bool CopyResultState ARGS((ResultState from, ResultState *to, int alloc));
extern ResultState IMakeResultState ARGS((int base_id, Pointer base_parent, Pointer base_parent_state, HookNode base_hook, TupleTableSlot cs_OuterTupleSlot, TupleTableSlot cs_ResultTupleSlot, ExprContext cs_ExprContext, ProjectionInfo cs_ProjInfo, int cs_NumScanAttributes, AttributeNumberPtr cs_ScanAttributes, int rs_Loop));
extern void RInitAppendState ARGS((Pointer p));
extern AppendState MakeAppendState ARGS((int as_whichplan, int as_nplans, ListPtr as_initialized, List as_rtentries));
extern void OutAppendState ARGS((StringInfo str, AppendState node));
extern bool EqualAppendState ARGS((AppendState a, AppendState b));
extern bool CopyAppendState ARGS((AppendState from, AppendState *to, int alloc));
extern AppendState IMakeAppendState ARGS((int base_id, Pointer base_parent, Pointer base_parent_state, HookNode base_hook, TupleTableSlot cs_OuterTupleSlot, TupleTableSlot cs_ResultTupleSlot, ExprContext cs_ExprContext, ProjectionInfo cs_ProjInfo, int cs_NumScanAttributes, AttributeNumberPtr cs_ScanAttributes, int as_whichplan, int as_nplans, ListPtr as_initialized, List as_rtentries));
extern void RInitCommonScanState ARGS((Pointer p));
extern CommonScanState MakeCommonScanState ARGS((Relation css_currentRelation, HeapScanDesc css_currentScanDesc, RelationRuleInfo css_ruleInfo, Pointer css_ScanTupleSlot, Pointer css_RawTupleSlot));
extern void OutCommonScanState ARGS((StringInfo str, CommonScanState node));
extern bool EqualCommonScanState ARGS((CommonScanState a, CommonScanState b));
extern bool CopyCommonScanState ARGS((CommonScanState from, CommonScanState *to, int alloc));
extern CommonScanState IMakeCommonScanState ARGS((int base_id, Pointer base_parent, Pointer base_parent_state, HookNode base_hook, TupleTableSlot cs_OuterTupleSlot, TupleTableSlot cs_ResultTupleSlot, ExprContext cs_ExprContext, ProjectionInfo cs_ProjInfo, int cs_NumScanAttributes, AttributeNumberPtr cs_ScanAttributes, Relation css_currentRelation, HeapScanDesc css_currentScanDesc, RelationRuleInfo css_ruleInfo, Pointer css_ScanTupleSlot, Pointer css_RawTupleSlot));
extern void RInitScanState ARGS((Pointer p));
extern ScanState MakeScanState ARGS((bool ss_ProcOuterFlag, Index ss_OldRelId));
extern void OutScanState ARGS((StringInfo str, ScanState node));
extern bool EqualScanState ARGS((ScanState a, ScanState b));
extern bool CopyScanState ARGS((ScanState from, ScanState *to, int alloc));
extern ScanState IMakeScanState ARGS((int base_id, Pointer base_parent, Pointer base_parent_state, HookNode base_hook, TupleTableSlot cs_OuterTupleSlot, TupleTableSlot cs_ResultTupleSlot, ExprContext cs_ExprContext, ProjectionInfo cs_ProjInfo, int cs_NumScanAttributes, AttributeNumberPtr cs_ScanAttributes, Relation css_currentRelation, HeapScanDesc css_currentScanDesc, RelationRuleInfo css_ruleInfo, Pointer css_ScanTupleSlot, Pointer css_RawTupleSlot, bool ss_ProcOuterFlag, Index ss_OldRelId));
extern void RInitScanTempState ARGS((Pointer p));
extern ScanTempState MakeScanTempState ARGS((int st_whichplan, int st_nplans));
extern void OutScanTempState ARGS((StringInfo str, ScanTempState node));
extern bool EqualScanTempState ARGS((ScanTempState a, ScanTempState b));
extern bool CopyScanTempState ARGS((ScanTempState from, ScanTempState *to, int alloc));
extern ScanTempState IMakeScanTempState ARGS((int base_id, Pointer base_parent, Pointer base_parent_state, HookNode base_hook, TupleTableSlot cs_OuterTupleSlot, TupleTableSlot cs_ResultTupleSlot, ExprContext cs_ExprContext, ProjectionInfo cs_ProjInfo, int cs_NumScanAttributes, AttributeNumberPtr cs_ScanAttributes, Relation css_currentRelation, HeapScanDesc css_currentScanDesc, RelationRuleInfo css_ruleInfo, Pointer css_ScanTupleSlot, Pointer css_RawTupleSlot, int st_whichplan, int st_nplans));
extern void RInitIndexScanState ARGS((Pointer p));
extern IndexScanState MakeIndexScanState ARGS((int iss_NumIndices, int iss_IndexPtr, ScanKeyPtr iss_ScanKeys, IntPtr iss_NumScanKeys, Pointer iss_RuntimeKeyInfo, RelationPtr iss_RelationDescs, IndexScanDescPtr iss_ScanDescs));
extern void OutIndexScanState ARGS((StringInfo str, IndexScanState node));
extern bool EqualIndexScanState ARGS((IndexScanState a, IndexScanState b));
extern bool CopyIndexScanState ARGS((IndexScanState from, IndexScanState *to, int alloc));
extern IndexScanState IMakeIndexScanState ARGS((int base_id, Pointer base_parent, Pointer base_parent_state, HookNode base_hook, TupleTableSlot cs_OuterTupleSlot, TupleTableSlot cs_ResultTupleSlot, ExprContext cs_ExprContext, ProjectionInfo cs_ProjInfo, int cs_NumScanAttributes, AttributeNumberPtr cs_ScanAttributes, int iss_NumIndices, int iss_IndexPtr, ScanKeyPtr iss_ScanKeys, IntPtr iss_NumScanKeys, Pointer iss_RuntimeKeyInfo, RelationPtr iss_RelationDescs, IndexScanDescPtr iss_ScanDescs));
extern void RInitJoinState ARGS((Pointer p));
extern JoinState MakeJoinState ARGS((int iss_NumIndices));
extern void OutJoinState ARGS((StringInfo str, JoinState node));
extern bool EqualJoinState ARGS((JoinState a, JoinState b));
extern bool CopyJoinState ARGS((JoinState from, JoinState *to, int alloc));
extern JoinState IMakeJoinState ARGS((int base_id, Pointer base_parent, Pointer base_parent_state, HookNode base_hook, TupleTableSlot cs_OuterTupleSlot, TupleTableSlot cs_ResultTupleSlot, ExprContext cs_ExprContext, ProjectionInfo cs_ProjInfo, int cs_NumScanAttributes, AttributeNumberPtr cs_ScanAttributes));
extern void RInitNestLoopState ARGS((Pointer p));
extern NestLoopState MakeNestLoopState ARGS((bool nl_PortalFlag));
extern void OutNestLoopState ARGS((StringInfo str, NestLoopState node));
extern bool EqualNestLoopState ARGS((NestLoopState a, NestLoopState b));
extern bool CopyNestLoopState ARGS((NestLoopState from, NestLoopState *to, int alloc));
extern NestLoopState IMakeNestLoopState ARGS((int base_id, Pointer base_parent, Pointer base_parent_state, HookNode base_hook, TupleTableSlot cs_OuterTupleSlot, TupleTableSlot cs_ResultTupleSlot, ExprContext cs_ExprContext, ProjectionInfo cs_ProjInfo, int cs_NumScanAttributes, AttributeNumberPtr cs_ScanAttributes, bool nl_PortalFlag));
extern void RInitMergeJoinState ARGS((Pointer p));
extern MergeJoinState MakeMergeJoinState ARGS((List mj_OSortopI, List mj_ISortopO, int mj_JoinState, TupleTableSlot mj_MarkedTupleSlot));
extern void OutMergeJoinState ARGS((StringInfo str, MergeJoinState node));
extern bool EqualMergeJoinState ARGS((MergeJoinState a, MergeJoinState b));
extern bool CopyMergeJoinState ARGS((MergeJoinState from, MergeJoinState *to, int alloc));
extern MergeJoinState IMakeMergeJoinState ARGS((int base_id, Pointer base_parent, Pointer base_parent_state, HookNode base_hook, TupleTableSlot cs_OuterTupleSlot, TupleTableSlot cs_ResultTupleSlot, ExprContext cs_ExprContext, ProjectionInfo cs_ProjInfo, int cs_NumScanAttributes, AttributeNumberPtr cs_ScanAttributes, List mj_OSortopI, List mj_ISortopO, int mj_JoinState, TupleTableSlot mj_MarkedTupleSlot));
extern void RInitHashJoinState ARGS((Pointer p));
extern HashJoinState MakeHashJoinState ARGS((HashJoinTable hj_HashTable, IpcMemoryId hj_HashTableShmId, HashBucket hj_CurBucket, HeapTuple hj_CurTuple, OverflowTuple hj_CurOTuple, Var hj_InnerHashKey, FileP hj_OuterBatches, FileP hj_InnerBatches, charP hj_OuterReadPos, int hj_OuterReadBlk, Pointer hj_OuterTupleSlot, Pointer hj_HashTupleSlot));
extern void OutHashJoinState ARGS((StringInfo str, HashJoinState node));
extern bool EqualHashJoinState ARGS((HashJoinState a, HashJoinState b));
extern bool CopyHashJoinState ARGS((HashJoinState from, HashJoinState *to, int alloc));
extern HashJoinState IMakeHashJoinState ARGS((int base_id, Pointer base_parent, Pointer base_parent_state, HookNode base_hook, TupleTableSlot cs_OuterTupleSlot, TupleTableSlot cs_ResultTupleSlot, ExprContext cs_ExprContext, ProjectionInfo cs_ProjInfo, int cs_NumScanAttributes, AttributeNumberPtr cs_ScanAttributes, HashJoinTable hj_HashTable, IpcMemoryId hj_HashTableShmId, HashBucket hj_CurBucket, HeapTuple hj_CurTuple, OverflowTuple hj_CurOTuple, Var hj_InnerHashKey, FileP hj_OuterBatches, FileP hj_InnerBatches, charP hj_OuterReadPos, int hj_OuterReadBlk, Pointer hj_OuterTupleSlot, Pointer hj_HashTupleSlot));
extern void RInitMaterialState ARGS((Pointer p));
extern MaterialState MakeMaterialState ARGS((bool mat_Flag, Relation mat_TempRelation));
extern void OutMaterialState ARGS((StringInfo str, MaterialState node));
extern bool EqualMaterialState ARGS((MaterialState a, MaterialState b));
extern bool CopyMaterialState ARGS((MaterialState from, MaterialState *to, int alloc));
extern MaterialState IMakeMaterialState ARGS((int base_id, Pointer base_parent, Pointer base_parent_state, HookNode base_hook, TupleTableSlot cs_OuterTupleSlot, TupleTableSlot cs_ResultTupleSlot, ExprContext cs_ExprContext, ProjectionInfo cs_ProjInfo, int cs_NumScanAttributes, AttributeNumberPtr cs_ScanAttributes, Relation css_currentRelation, HeapScanDesc css_currentScanDesc, RelationRuleInfo css_ruleInfo, Pointer css_ScanTupleSlot, Pointer css_RawTupleSlot, bool mat_Flag, Relation mat_TempRelation));
extern void RInitAggState ARGS((Pointer p));
extern AggState MakeAggState ARGS((bool agg_Flag, Relation agg_TempRelation,
Plan outerPlan));
extern void OutAggState ARGS((StringInfo str, AggState node));
extern bool EqualAggState ARGS((AggState a, AggState b));
extern bool CopyAggState ARGS((AggState from, AggState *to, int alloc));
extern AggState IMakeAggState ARGS((int base_id, Pointer base_parent, Pointer base_parent_state, HookNode base_hook, TupleTableSlot cs_OuterTupleSlot, TupleTableSlot cs_ResultTupleSlot,  ExprContext cs_ExprContext, ProjectionInfo cs_ProjInfo, int cs_NumScanAttributes, AttributeNumberPtr cs_ScanAttributes, Relation css_currentRelation, HeapScanDesc css_currentScanDesc, RelationRuleInfo css_ruleInfo, Pointer css_ScanTupleSlot, Pointer css_RawTupleSlot, bool agg_Flag, Relation agg_TempRelation));
extern void RInitSortState ARGS((Pointer p));
extern SortState MakeSortState ARGS((bool sort_Flag, Pointer sort_Keys, Relation sort_TempRelation));
extern void OutSortState ARGS((StringInfo str, SortState node));
extern bool EqualSortState ARGS((SortState a, SortState b));
extern bool CopySortState ARGS((SortState from, SortState *to, int alloc));
extern SortState IMakeSortState ARGS((int base_id, Pointer base_parent, Pointer base_parent_state, HookNode base_hook, TupleTableSlot cs_OuterTupleSlot, TupleTableSlot cs_ResultTupleSlot, ExprContext cs_ExprContext, ProjectionInfo cs_ProjInfo, int cs_NumScanAttributes, AttributeNumberPtr cs_ScanAttributes, Relation css_currentRelation, HeapScanDesc css_currentScanDesc, RelationRuleInfo css_ruleInfo, Pointer css_ScanTupleSlot, Pointer css_RawTupleSlot, bool sort_Flag, Pointer sort_Keys, Relation sort_TempRelation));
extern void RInitUniqueState ARGS((Pointer p));
extern UniqueState MakeUniqueState ARGS((int sort_Flag));
extern void OutUniqueState ARGS((StringInfo str, UniqueState node));
extern bool EqualUniqueState ARGS((UniqueState a, UniqueState b));
extern bool CopyUniqueState ARGS((UniqueState from, UniqueState *to, int alloc));
extern UniqueState IMakeUniqueState ARGS((int base_id, Pointer base_parent, Pointer base_parent_state, HookNode base_hook, TupleTableSlot cs_OuterTupleSlot, TupleTableSlot cs_ResultTupleSlot, ExprContext cs_ExprContext, ProjectionInfo cs_ProjInfo, int cs_NumScanAttributes, AttributeNumberPtr cs_ScanAttributes));
extern void RInitHashState ARGS((Pointer p));
extern HashState MakeHashState ARGS((FileP hashBatches));
extern void OutHashState ARGS((StringInfo str, HashState node));
extern bool EqualHashState ARGS((HashState a, HashState b));
extern bool CopyHashState ARGS((HashState from, HashState *to, int alloc));
