/* $Header$ */
extern void set_ii_NumKeyAttributes ARGS((IndexInfo node, int value));
extern int get_ii_NumKeyAttributes ARGS((IndexInfo node));
extern void set_ii_KeyAttributeNumbers ARGS((IndexInfo node, AttributeNumberPtr value));
extern AttributeNumberPtr get_ii_KeyAttributeNumbers ARGS((IndexInfo node));
extern void set_ri_RangeTableIndex ARGS((RelationInfo node, Index value));
extern Index get_ri_RangeTableIndex ARGS((RelationInfo node));
extern void set_ri_RelationDesc ARGS((RelationInfo node, Relation value));
extern Relation get_ri_RelationDesc ARGS((RelationInfo node));
extern void set_ri_NumIndices ARGS((RelationInfo node, int value));
extern int get_ri_NumIndices ARGS((RelationInfo node));
extern void set_ri_IndexRelationDescs ARGS((RelationInfo node, RelationPtr value));
extern RelationPtr get_ri_IndexRelationDescs ARGS((RelationInfo node));
extern void set_ri_IndexRelationInfo ARGS((RelationInfo node, IndexInfoPtr value));
extern IndexInfoPtr get_ri_IndexRelationInfo ARGS((RelationInfo node));
extern void set_tc_retrieved ARGS((TupleCount node, int value));
extern int get_tc_retrieved ARGS((TupleCount node));
extern void set_tc_appended ARGS((TupleCount node, int value));
extern int get_tc_appended ARGS((TupleCount node));
extern void set_tc_deleted ARGS((TupleCount node, int value));
extern int get_tc_deleted ARGS((TupleCount node));
extern void set_tc_replaced ARGS((TupleCount node, int value));
extern int get_tc_replaced ARGS((TupleCount node));
extern void set_tc_inserted ARGS((TupleCount node, int value));
extern int get_tc_inserted ARGS((TupleCount node));
extern void set_tc_processed ARGS((TupleCount node, int value));
extern int get_tc_processed ARGS((TupleCount node));
extern void set_ttc_shouldFree ARGS((TupleTableSlot node, bool value));
extern bool get_ttc_shouldFree ARGS((TupleTableSlot node));
extern void set_ttc_descIsNew ARGS((TupleTableSlot node, bool value));
extern bool get_ttc_descIsNew ARGS((TupleTableSlot node));
extern void set_ttc_tupleDescriptor ARGS((TupleTableSlot node, TupleDescriptor value));
extern TupleDescriptor get_ttc_tupleDescriptor ARGS((TupleTableSlot node));
extern void set_ttc_buffer ARGS((TupleTableSlot node, Buffer value));
extern Buffer get_ttc_buffer ARGS((TupleTableSlot node));
extern void set_ecxt_scantuple ARGS((ExprContext node, TupleTableSlot value));
extern TupleTableSlot get_ecxt_scantuple ARGS((ExprContext node));
extern void set_ecxt_innertuple ARGS((ExprContext node, TupleTableSlot value));
extern TupleTableSlot get_ecxt_innertuple ARGS((ExprContext node));
extern void set_ecxt_outertuple ARGS((ExprContext node, TupleTableSlot value));
extern TupleTableSlot get_ecxt_outertuple ARGS((ExprContext node));
extern void set_ecxt_relation ARGS((ExprContext node, Relation value));
extern Relation get_ecxt_relation ARGS((ExprContext node));
extern void set_ecxt_relid ARGS((ExprContext node, Index value));
extern Index get_ecxt_relid ARGS((ExprContext node));
extern void set_ecxt_param_list_info ARGS((ExprContext node, ParamListInfo value));
extern ParamListInfo get_ecxt_param_list_info ARGS((ExprContext node));
extern void set_pi_targetlist ARGS((ProjectionInfo node, List value));
extern List get_pi_targetlist ARGS((ProjectionInfo node));
extern void set_pi_len ARGS((ProjectionInfo node, int value));
extern int get_pi_len ARGS((ProjectionInfo node));
extern void set_pi_tupValue ARGS((ProjectionInfo node, Pointer value));
extern Pointer get_pi_tupValue ARGS((ProjectionInfo node));
extern void set_pi_exprContext ARGS((ProjectionInfo node, ExprContext value));
extern ExprContext get_pi_exprContext ARGS((ProjectionInfo node));
extern void set_pi_slot ARGS((ProjectionInfo node, TupleTableSlot value));
extern TupleTableSlot get_pi_slot ARGS((ProjectionInfo node));
extern void set_jf_targetList ARGS((JunkFilter node, List value));
extern List get_jf_targetList ARGS((JunkFilter node));
extern void set_jf_length ARGS((JunkFilter node, int value));
extern int get_jf_length ARGS((JunkFilter node));
extern void set_jf_tupType ARGS((JunkFilter node, TupleDescriptor value));
extern TupleDescriptor get_jf_tupType ARGS((JunkFilter node));
extern void set_jf_cleanTargetList ARGS((JunkFilter node, List value));
extern List get_jf_cleanTargetList ARGS((JunkFilter node));
extern void set_jf_cleanLength ARGS((JunkFilter node, int value));
extern int get_jf_cleanLength ARGS((JunkFilter node));
extern void set_jf_cleanTupType ARGS((JunkFilter node, TupleDescriptor value));
extern TupleDescriptor get_jf_cleanTupType ARGS((JunkFilter node));
extern void set_jf_cleanMap ARGS((JunkFilter node, AttributeNumberPtr value));
extern AttributeNumberPtr get_jf_cleanMap ARGS((JunkFilter node));
extern void set_es_direction ARGS((EState node, ScanDirection value));
extern ScanDirection get_es_direction ARGS((EState node));
extern void set_es_time ARGS((EState node, abstime value));
extern abstime get_es_time ARGS((EState node));
extern void set_es_owner ARGS((EState node, ObjectId value));
extern ObjectId get_es_owner ARGS((EState node));
extern void set_es_locks ARGS((EState node, List value));
extern List get_es_locks ARGS((EState node));
extern void set_es_subplan_info ARGS((EState node, List value));
extern List get_es_subplan_info ARGS((EState node));
extern void set_es_error_message ARGS((EState node, Name value));
extern Name get_es_error_message ARGS((EState node));
extern void set_es_range_table ARGS((EState node, List value));
extern List get_es_range_table ARGS((EState node));
extern void set_es_qualification_tuple ARGS((EState node, HeapTuple value));
extern HeapTuple get_es_qualification_tuple ARGS((EState node));
extern void set_es_qualification_tuple_id ARGS((EState node, ItemPointer value));
extern ItemPointer get_es_qualification_tuple_id ARGS((EState node));
extern void set_es_qualification_tuple_buffer ARGS((EState node, Buffer value));
extern Buffer get_es_qualification_tuple_buffer ARGS((EState node));
extern void set_es_raw_qualification_tuple ARGS((EState node, HeapTuple value));
extern HeapTuple get_es_raw_qualification_tuple ARGS((EState node));
extern void set_es_relation_relation_descriptor ARGS((EState node, Relation value));
extern Relation get_es_relation_relation_descriptor ARGS((EState node));
extern void set_es_into_relation_descriptor ARGS((EState node, Relation value));
extern Relation get_es_into_relation_descriptor ARGS((EState node));
extern void set_es_result_relation_info ARGS((EState node, RelationInfo value));
extern RelationInfo get_es_result_relation_info ARGS((EState node));
extern void set_es_tuplecount ARGS((EState node, TupleCount value));
extern TupleCount get_es_tuplecount ARGS((EState node));
extern void set_es_param_list_info ARGS((EState node, ParamListInfo value));
extern ParamListInfo get_es_param_list_info ARGS((EState node));
extern void set_es_prs2_info ARGS((EState node, Prs2EStateInfo value));
extern Prs2EStateInfo get_es_prs2_info ARGS((EState node));
extern void set_es_explain_relation ARGS((EState node, Relation value));
extern Relation get_es_explain_relation ARGS((EState node));
extern void set_es_BaseId ARGS((EState node, int value));
extern int get_es_BaseId ARGS((EState node));
extern void set_es_tupleTable ARGS((EState node, TupleTable value));
extern TupleTable get_es_tupleTable ARGS((EState node));
extern void set_es_junkFilter ARGS((EState node, JunkFilter value));
extern JunkFilter get_es_junkFilter ARGS((EState node));
extern void set_es_result_rel_scanstate ARGS((EState node, Pointer value));
extern Pointer get_es_result_rel_scanstate ARGS((EState node));
extern void set_es_result_rel_ruleinfo ARGS((EState node, RelationRuleInfo value));
extern RelationRuleInfo get_es_result_rel_ruleinfo ARGS((EState node));
extern void set_hook_at_initnode ARGS((HookNode node, HookFunction value));
extern HookFunction get_hook_at_initnode ARGS((HookNode node));
extern void set_hook_pre_procnode ARGS((HookNode node, HookFunction value));
extern HookFunction get_hook_pre_procnode ARGS((HookNode node));
extern void set_hook_pre_endnode ARGS((HookNode node, HookFunction value));
extern HookFunction get_hook_pre_endnode ARGS((HookNode node));
extern void set_hook_post_initnode ARGS((HookNode node, HookFunction value));
extern HookFunction get_hook_post_initnode ARGS((HookNode node));
extern void set_hook_post_procnode ARGS((HookNode node, HookFunction value));
extern HookFunction get_hook_post_procnode ARGS((HookNode node));
extern void set_hook_post_endnode ARGS((HookNode node, HookFunction value));
extern HookFunction get_hook_post_endnode ARGS((HookNode node));
extern void set_hook_data ARGS((HookNode node, Pointer value));
extern Pointer get_hook_data ARGS((HookNode node));
extern void set_base_id ARGS((BaseNode node, int value));
extern int get_base_id ARGS((BaseNode node));
extern void set_base_parent ARGS((BaseNode node, Pointer value));
extern Pointer get_base_parent ARGS((BaseNode node));
extern void set_base_parent_state ARGS((BaseNode node, Pointer value));
extern Pointer get_base_parent_state ARGS((BaseNode node));
extern void set_base_hook ARGS((BaseNode node, HookNode value));
extern HookNode get_base_hook ARGS((BaseNode node));
extern void set_cs_OuterTupleSlot ARGS((CommonState node, TupleTableSlot value));
extern TupleTableSlot get_cs_OuterTupleSlot ARGS((CommonState node));
extern void set_cs_ResultTupleSlot ARGS((CommonState node, TupleTableSlot value));
extern TupleTableSlot get_cs_ResultTupleSlot ARGS((CommonState node));
extern void set_cs_ExprContext ARGS((CommonState node, ExprContext value));
extern ExprContext get_cs_ExprContext ARGS((CommonState node));
extern void set_cs_ProjInfo ARGS((CommonState node, ProjectionInfo value));
extern ProjectionInfo get_cs_ProjInfo ARGS((CommonState node));
extern void set_cs_NumScanAttributes ARGS((CommonState node, int value));
extern int get_cs_NumScanAttributes ARGS((CommonState node));
extern void set_cs_ScanAttributes ARGS((CommonState node, AttributeNumberPtr value));
extern AttributeNumberPtr get_cs_ScanAttributes ARGS((CommonState node));
extern void set_rs_Loop ARGS((ResultState node, int value));
extern int get_rs_Loop ARGS((ResultState node));
extern void set_as_whichplan ARGS((AppendState node, int value));
extern int get_as_whichplan ARGS((AppendState node));
extern void set_as_nplans ARGS((AppendState node, int value));
extern int get_as_nplans ARGS((AppendState node));
extern void set_as_initialized ARGS((AppendState node, ListPtr value));
extern ListPtr get_as_initialized ARGS((AppendState node));
extern void set_as_rtentries ARGS((AppendState node, List value));
extern List get_as_rtentries ARGS((AppendState node));
extern void set_css_currentRelation ARGS((CommonScanState node, Relation value));
extern Relation get_css_currentRelation ARGS((CommonScanState node));
extern void set_css_currentScanDesc ARGS((CommonScanState node, HeapScanDesc value));
extern HeapScanDesc get_css_currentScanDesc ARGS((CommonScanState node));
extern void set_css_ruleInfo ARGS((CommonScanState node, RelationRuleInfo value));
extern RelationRuleInfo get_css_ruleInfo ARGS((CommonScanState node));
extern void set_css_ScanTupleSlot ARGS((CommonScanState node, Pointer value));
extern TupleTableSlot get_css_ScanTupleSlot ARGS((CommonScanState node));
extern void set_css_RawTupleSlot ARGS((CommonScanState node, Pointer value));
extern TupleTableSlot get_css_RawTupleSlot ARGS((CommonScanState node));
extern void set_ss_ProcOuterFlag ARGS((ScanState node, bool value));
extern bool get_ss_ProcOuterFlag ARGS((ScanState node));
extern void set_ss_OldRelId ARGS((ScanState node, Index value));
extern Index get_ss_OldRelId ARGS((ScanState node));
extern void set_st_whichplan ARGS((ScanTempState node, int value));
extern int get_st_whichplan ARGS((ScanTempState node));
extern void set_st_nplans ARGS((ScanTempState node, int value));
extern int get_st_nplans ARGS((ScanTempState node));
extern void set_iss_NumIndices ARGS((IndexScanState node, int value));
extern int get_iss_NumIndices ARGS((IndexScanState node));
extern void set_iss_IndexPtr ARGS((IndexScanState node, int value));
extern int get_iss_IndexPtr ARGS((IndexScanState node));
extern void set_iss_ScanKeys ARGS((IndexScanState node, ScanKeyPtr value));
extern ScanKeyPtr get_iss_ScanKeys ARGS((IndexScanState node));
extern void set_iss_NumScanKeys ARGS((IndexScanState node, IntPtr value));
extern IntPtr get_iss_NumScanKeys ARGS((IndexScanState node));
extern void set_iss_RuntimeKeyInfo ARGS((IndexScanState node, Pointer value));
extern Pointer get_iss_RuntimeKeyInfo ARGS((IndexScanState node));
extern void set_iss_RelationDescs ARGS((IndexScanState node, RelationPtr value));
extern RelationPtr get_iss_RelationDescs ARGS((IndexScanState node));
extern void set_iss_ScanDescs ARGS((IndexScanState node, IndexScanDescPtr value));
extern IndexScanDescPtr get_iss_ScanDescs ARGS((IndexScanState node));
extern void set_nl_PortalFlag ARGS((NestLoopState node, bool value));
extern bool get_nl_PortalFlag ARGS((NestLoopState node));
extern void set_mj_OSortopI ARGS((MergeJoinState node, List value));
extern List get_mj_OSortopI ARGS((MergeJoinState node));
extern void set_mj_ISortopO ARGS((MergeJoinState node, List value));
extern List get_mj_ISortopO ARGS((MergeJoinState node));
extern void set_mj_JoinState ARGS((MergeJoinState node, int value));
extern int get_mj_JoinState ARGS((MergeJoinState node));
extern void set_mj_MarkedTupleSlot ARGS((MergeJoinState node, TupleTableSlot value));
extern TupleTableSlot get_mj_MarkedTupleSlot ARGS((MergeJoinState node));
extern void set_hj_HashTable ARGS((HashJoinState node, HashJoinTable value));
extern HashJoinTable get_hj_HashTable ARGS((HashJoinState node));
extern void set_hj_HashTableShmId ARGS((HashJoinState node, IpcMemoryId value));
extern IpcMemoryId get_hj_HashTableShmId ARGS((HashJoinState node));
extern void set_hj_CurBucket ARGS((HashJoinState node, HashBucket value));
extern HashBucket get_hj_CurBucket ARGS((HashJoinState node));
extern void set_hj_CurTuple ARGS((HashJoinState node, HeapTuple value));
extern HeapTuple get_hj_CurTuple ARGS((HashJoinState node));
extern void set_hj_CurOTuple ARGS((HashJoinState node, OverflowTuple value));
extern OverflowTuple get_hj_CurOTuple ARGS((HashJoinState node));
extern void set_hj_InnerHashKey ARGS((HashJoinState node, Var value));
extern Var get_hj_InnerHashKey ARGS((HashJoinState node));
extern void set_hj_OuterBatches ARGS((HashJoinState node, FileP value));
extern FileP get_hj_OuterBatches ARGS((HashJoinState node));
extern void set_hj_InnerBatches ARGS((HashJoinState node, FileP value));
extern FileP get_hj_InnerBatches ARGS((HashJoinState node));
extern void set_hj_OuterReadPos ARGS((HashJoinState node, charP value));
extern charP get_hj_OuterReadPos ARGS((HashJoinState node));
extern void set_hj_OuterReadBlk ARGS((HashJoinState node, int value));
extern int get_hj_OuterReadBlk ARGS((HashJoinState node));
extern void set_hj_OuterTupleSlot ARGS((HashJoinState node, Pointer value));
extern Pointer get_hj_OuterTupleSlot ARGS((HashJoinState node));
extern void set_hj_HashTupleSlot ARGS((HashJoinState node, Pointer value));
extern Pointer get_hj_HashTupleSlot ARGS((HashJoinState node));
extern void set_mat_Flag ARGS((MaterialState node, bool value));
extern bool get_mat_Flag ARGS((MaterialState node));
extern void set_mat_TempRelation ARGS((MaterialState node, Relation value));
extern Relation get_mat_TempRelation ARGS((MaterialState node));
extern void set_agg_Flag ARGS((AggState node, bool value));
extern bool get_agg_Flag ARGS((AggState node));
extern void set_agg_TempRelation ARGS((AggState node, Relation value));
extern Relation get_agg_TempRelation ARGS((AggState node));
extern void set_sort_Flag ARGS((SortState node, bool value));
extern bool get_sort_Flag ARGS((SortState node));
extern void set_sort_Keys ARGS((SortState node, Pointer value));
extern Pointer get_sort_Keys ARGS((SortState node));
extern void set_sort_TempRelation ARGS((SortState node, Relation value));
extern Relation get_sort_TempRelation ARGS((SortState node));
extern void set_hashBatches ARGS((HashState node, FileP value));
extern FileP get_hashBatches ARGS((HashState node));
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
extern TupleTableSlot MakeTupleTableSlot ARGS((bool ttc_shouldFree, bool ttc_descIsNew, TupleDescriptor ttc_tupleDescriptor, Buffer ttc_buffer));
extern void OutTupleTableSlot ARGS((StringInfo str, TupleTableSlot node));
extern bool EqualTupleTableSlot ARGS((TupleTableSlot a, TupleTableSlot b));
extern bool CopyTupleTableSlot ARGS((TupleTableSlot from, TupleTableSlot *to, int alloc));
extern TupleTableSlot IMakeTupleTableSlot ARGS((bool ttc_shouldFree, bool ttc_descIsNew, TupleDescriptor ttc_tupleDescriptor, Buffer ttc_buffer));
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
