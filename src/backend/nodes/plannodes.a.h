/* $Header$ */
extern void set_cost ARGS((Plan node, Cost value));
extern Cost get_cost ARGS((Plan node));
extern void set_plan_size ARGS((Plan node, Count value));
extern Count get_plan_size ARGS((Plan node));
extern void set_plan_width ARGS((Plan node, Count value));
extern Count get_plan_width ARGS((Plan node));
extern void set_fragment ARGS((Plan node, int value));
extern int get_fragment ARGS((Plan node));
extern void set_parallel ARGS((Plan node, int value));
extern int get_parallel ARGS((Plan node));
extern void set_state ARGS((Plan node, EStatePtr value));
extern EStatePtr get_state ARGS((Plan node));
extern void set_qptargetlist ARGS((Plan node, List value));
extern List get_qptargetlist ARGS((Plan node));
extern void set_qpqual ARGS((Plan node, List value));
extern List get_qpqual ARGS((Plan node));
extern void set_lefttree ARGS((Plan node, PlanPtr value));
extern PlanPtr get_lefttree ARGS((Plan node));
extern void set_righttree ARGS((Plan node, PlanPtr value));
extern PlanPtr get_righttree ARGS((Plan node));
extern void set_frag_root ARGS((Fragment node, Plan value));
extern Plan get_frag_root ARGS((Fragment node));
extern void set_frag_parent_op ARGS((Fragment node, Plan value));
extern Plan get_frag_parent_op ARGS((Fragment node));
extern void set_frag_parallel ARGS((Fragment node, int value));
extern int get_frag_parallel ARGS((Fragment node));
extern void set_frag_subtrees ARGS((Fragment node, List value));
extern List get_frag_subtrees ARGS((Fragment node));
extern void set_frag_parent_frag ARGS((Fragment node, Fragment value));
extern Fragment get_frag_parent_frag ARGS((Fragment node));
extern void set_frag_parsetree ARGS((Fragment node, List value));
extern List get_frag_parsetree ARGS((Fragment node));
extern void set_frag_is_inprocess ARGS((Fragment node, bool value));
extern bool get_frag_is_inprocess ARGS((Fragment node));
extern void set_resrellevelqual ARGS((Result node, List value));
extern List get_resrellevelqual ARGS((Result node));
extern void set_resconstantqual ARGS((Result node, List value));
extern List get_resconstantqual ARGS((Result node));
extern void set_resstate ARGS((Result node, ResultState value));
extern ResultState get_resstate ARGS((Result node));
extern void set_unionplans ARGS((Append node, List value));
extern List get_unionplans ARGS((Append node));
extern void set_unionrelid ARGS((Append node, Index value));
extern Index get_unionrelid ARGS((Append node));
extern void set_unionrtentries ARGS((Append node, List value));
extern List get_unionrtentries ARGS((Append node));
extern void set_unionstate ARGS((Append node, AppendState value));
extern AppendState get_unionstate ARGS((Append node));
extern void set_scanrelid ARGS((Scan node, Index value));
extern Index get_scanrelid ARGS((Scan node));
extern void set_scanstate ARGS((Scan node, ScanState value));
extern ScanState get_scanstate ARGS((Scan node));
extern void set_temprelDescs ARGS((ScanTemps node, List value));
extern List get_temprelDescs ARGS((ScanTemps node));
extern void set_scantempState ARGS((ScanTemps node, ScanTempState value));
extern ScanTempState get_scantempState ARGS((ScanTemps node));
extern void set_indxid ARGS((IndexScan node, List value));
extern List get_indxid ARGS((IndexScan node));
extern void set_indxqual ARGS((IndexScan node, List value));
extern List get_indxqual ARGS((IndexScan node));
extern void set_indxstate ARGS((IndexScan node, IndexScanState value));
extern IndexScanState get_indxstate ARGS((IndexScan node));
extern void set_jri_operator ARGS((JoinRuleInfo node, ObjectId value));
extern ObjectId get_jri_operator ARGS((JoinRuleInfo node));
extern void set_jri_inattrno ARGS((JoinRuleInfo node, AttributeNumber value));
extern AttributeNumber get_jri_inattrno ARGS((JoinRuleInfo node));
extern void set_jri_outattrno ARGS((JoinRuleInfo node, AttributeNumber value));
extern AttributeNumber get_jri_outattrno ARGS((JoinRuleInfo node));
extern void set_jri_lock ARGS((JoinRuleInfo node, RuleLock value));
extern RuleLock get_jri_lock ARGS((JoinRuleInfo node));
extern void set_jri_ruleid ARGS((JoinRuleInfo node, ObjectId value));
extern ObjectId get_jri_ruleid ARGS((JoinRuleInfo node));
extern void set_jri_stubid ARGS((JoinRuleInfo node, Prs2StubId value));
extern Prs2StubId get_jri_stubid ARGS((JoinRuleInfo node));
extern void set_jri_stub ARGS((JoinRuleInfo node, Prs2OneStub value));
extern Prs2OneStub get_jri_stub ARGS((JoinRuleInfo node));
extern void set_jri_stats ARGS((JoinRuleInfo node, Prs2StubStats value));
extern Prs2StubStats get_jri_stats ARGS((JoinRuleInfo node));
extern void set_ruleinfo ARGS((Join node, JoinRuleInfoPtr value));
extern JoinRuleInfoPtr get_ruleinfo ARGS((Join node));
extern void set_nlstate ARGS((NestLoop node, NestLoopState value));
extern NestLoopState get_nlstate ARGS((NestLoop node));
extern void set_nlRuleInfo ARGS((NestLoop node, JoinRuleInfo value));
extern JoinRuleInfo get_nlRuleInfo ARGS((NestLoop node));
extern void set_mergeclauses ARGS((MergeJoin node, List value));
extern List get_mergeclauses ARGS((MergeJoin node));
extern void set_mergesortop ARGS((MergeJoin node, ObjectId value));
extern ObjectId get_mergesortop ARGS((MergeJoin node));
extern void set_mergerightorder ARGS((MergeJoin node, List value));
extern List get_mergerightorder ARGS((MergeJoin node));
extern void set_mergeleftorder ARGS((MergeJoin node, List value));
extern List get_mergeleftorder ARGS((MergeJoin node));
extern void set_mergestate ARGS((MergeJoin node, MergeJoinState value));
extern MergeJoinState get_mergestate ARGS((MergeJoin node));
extern void set_hashclauses ARGS((HashJoin node, List value));
extern List get_hashclauses ARGS((HashJoin node));
extern void set_hashjoinop ARGS((HashJoin node, ObjectId value));
extern ObjectId get_hashjoinop ARGS((HashJoin node));
extern void set_hashjoinstate ARGS((HashJoin node, HashJoinState value));
extern HashJoinState get_hashjoinstate ARGS((HashJoin node));
extern void set_hashjointable ARGS((HashJoin node, HashJoinTable value));
extern HashJoinTable get_hashjointable ARGS((HashJoin node));
extern void set_hashjointablekey ARGS((HashJoin node, IpcMemoryKey value));
extern IpcMemoryKey get_hashjointablekey ARGS((HashJoin node));
extern void set_hashjointablesize ARGS((HashJoin node, int value));
extern int get_hashjointablesize ARGS((HashJoin node));
extern void set_hashdone ARGS((HashJoin node, bool value));
extern bool get_hashdone ARGS((HashJoin node));
extern void set_tempid ARGS((Temp node, ObjectId value));
extern ObjectId get_tempid ARGS((Temp node));
extern void set_keycount ARGS((Temp node, Count value));
extern Count get_keycount ARGS((Temp node));
extern void set_matstate ARGS((Material node, MaterialState value));
extern MaterialState get_matstate ARGS((Material node));
extern void set_sortstate ARGS((Sort node, SortState value));
extern SortState get_sortstate ARGS((Sort node));
extern void set_uniquestate ARGS((Unique node, UniqueState value));
extern UniqueState get_uniquestate ARGS((Unique node));
extern void set_hashkey ARGS((Hash node, Var value));
extern Var get_hashkey ARGS((Hash node));
extern void set_hashstate ARGS((Hash node, HashState value));
extern HashState get_hashstate ARGS((Hash node));
extern void set_hashtable ARGS((Hash node, HashJoinTable value));
extern HashJoinTable get_hashtable ARGS((Hash node));
extern void set_hashtablekey ARGS((Hash node, IpcMemoryKey value));
extern IpcMemoryKey get_hashtablekey ARGS((Hash node));
extern void set_hashtablesize ARGS((Hash node, int value));
extern int get_hashtablesize ARGS((Hash node));
extern void set_chooseplanlist ARGS((Choose node, List value));
extern List get_chooseplanlist ARGS((Choose node));
extern void RInitPlan ARGS((Pointer p));
extern Plan MakePlan ARGS((Cost cost, Count plan_size, Count plan_width, int fragment, int parallel, EStatePtr state, List qptargetlist, List qpqual, PlanPtr lefttree, PlanPtr righttree));
extern void OutPlan ARGS((StringInfo str, Plan node));
extern bool EqualPlan ARGS((Plan a, Plan b));
extern bool CopyPlan ARGS((Plan from, Plan *to, int alloc));
extern Plan IMakePlan ARGS((Cost cost, Count plan_size, Count plan_width, int fragment, int parallel, EStatePtr state, List qptargetlist, List qpqual, PlanPtr lefttree, PlanPtr righttree));
extern void RInitFragment ARGS((Pointer p));
extern Fragment MakeFragment ARGS((Plan frag_root, Plan frag_parent_op, int frag_parallel, List frag_subtrees, FragmentPtr frag_parent_frag));
extern void OutFragment ARGS((StringInfo str, Fragment node));
extern bool EqualFragment ARGS((Fragment a, Fragment b));
extern bool CopyFragment ARGS((Fragment from, Fragment *to, int alloc));
extern Fragment IMakeFragment ARGS((Plan frag_root, Plan frag_parent_op, int frag_parallel, List frag_subtrees, FragmentPtr frag_parent_frag));
extern void RInitResult ARGS((Pointer p));
extern Result MakeResult ARGS((List resrellevelqual, List resconstantqual, ResultState resstate));
extern void OutResult ARGS((StringInfo str, Result node));
extern bool EqualResult ARGS((Result a, Result b));
extern bool CopyResult ARGS((Result from, Result *to, int alloc));
extern Result IMakeResult ARGS((Cost cost, Count plan_size, Count plan_width, int fragment, int parallel, ResultState res state, List qptargetlist, List qpqual, PlanPtr lefttree, PlanPtr righttree, List resrellevelqual, List resconstantqual, int resstate));
extern void RInitAppend ARGS((Pointer p));
extern Append MakeAppend ARGS((List unionplans, Index unionrelid, List unionrtentries, AppendState unionstate));
extern void OutAppend ARGS((StringInfo str, Append node));
extern bool EqualAppend ARGS((Append a, Append b));
extern bool CopyAppend ARGS((Append from, Append *to, int alloc));
extern Append IMakeAppend ARGS((Cost cost, Count plan_size, Count plan_width, int fragment, int parallel, AppendState union state, List qptargetlist, List qpqual, PlanPtr lefttree, PlanPtr righttree, List unionplans, Index unionrelid, List unionrtentries, int unionstate));
extern void RInitScan ARGS((Pointer p));
extern Scan MakeScan ARGS((Index scanrelid, ScanState scanstate));
extern void OutScan ARGS((StringInfo str, Scan node));
extern bool EqualScan ARGS((Scan a, Scan b));
extern bool CopyScan ARGS((Scan from, Scan *to, int alloc));
extern Scan IMakeScan ARGS((Cost cost, Count plan_size, Count plan_width, int fragment, int parallel, ScanState scan state, List qptargetlist, List qpqual, PlanPtr lefttree, PlanPtr righttree, Index scanrelid, int scanstate));
extern void RInitSeqScan ARGS((Pointer p));
extern SeqScan MakeSeqScan ARGS((int scanrelid));
extern void OutSeqScan ARGS((StringInfo str, SeqScan node));
extern bool EqualSeqScan ARGS((SeqScan a, SeqScan b));
extern bool CopySeqScan ARGS((SeqScan from, SeqScan *to, int alloc));
extern SeqScan IMakeSeqScan ARGS((Cost cost, Count plan_size, Count plan_width, int fragment, int parallel, ScanState scan state, List qptargetlist, List qpqual, PlanPtr lefttree, PlanPtr righttree, Index scanrelid, int scanstate));
extern void RInitScanTemps ARGS((Pointer p));
extern ScanTemps MakeScanTemps ARGS((List temprelDescs, ScanTempState scantempState));
extern void OutScanTemps ARGS((StringInfo str, ScanTemps node));
extern bool EqualScanTemps ARGS((ScanTemps a, ScanTemps b));
extern bool CopyScanTemps ARGS((ScanTemps from, ScanTemps *to, int alloc));
extern ScanTemps IMakeScanTemps ARGS((Cost cost, Count plan_size, Count plan_width, int fragment, int parallel, EStatePtr state, List qptargetlist, List qpqual, PlanPtr lefttree, PlanPtr righttree, List temprelDescs, ScanTempState scantempState));
extern void RInitIndexScan ARGS((Pointer p));
extern IndexScan MakeIndexScan ARGS((List indxid, List indxqual, IndexScanState indxstate));
extern void OutIndexScan ARGS((StringInfo str, IndexScan node));
extern bool EqualIndexScan ARGS((IndexScan a, IndexScan b));
extern bool CopyIndexScan ARGS((IndexScan from, IndexScan *to, int alloc));
extern IndexScan IMakeIndexScan ARGS((Cost cost, Count plan_size, Count plan_width, int fragment, int parallel, IndexScanState indx state, List qptargetlist, List qpqual, PlanPtr lefttree, PlanPtr righttree, Index scanrelid, ScanState scanstate, List indxid, List indxqual, int indxstate));
extern void RInitJoinRuleInfo ARGS((Pointer p));
extern JoinRuleInfo MakeJoinRuleInfo ARGS((ObjectId jri_operator, AttributeNumber jri_inattrno, AttributeNumber jri_outattrno, RuleLock jri_lock, ObjectId jri_ruleid, Prs2StubId jri_stubid, Prs2OneStub jri_stub, Prs2StubStats jri_stats));
extern void OutJoinRuleInfo ARGS((StringInfo str, JoinRuleInfo node));
extern bool EqualJoinRuleInfo ARGS((JoinRuleInfo a, JoinRuleInfo b));
extern bool CopyJoinRuleInfo ARGS((JoinRuleInfo from, JoinRuleInfo *to, int alloc));
extern JoinRuleInfo IMakeJoinRuleInfo ARGS((ObjectId jri_operator, AttributeNumber jri_inattrno, AttributeNumber jri_outattrno, RuleLock jri_lock, ObjectId jri_ruleid, Prs2StubId jri_stubid, Prs2OneStub jri_stub, Prs2StubStats jri_stats));
extern void RInitJoin ARGS((Pointer p));
extern Join MakeJoin ARGS((JoinRuleInfoPtr ruleinfo));
extern void OutJoin ARGS((StringInfo str, Join node));
extern bool EqualJoin ARGS((Join a, Join b));
extern bool CopyJoin ARGS((Join from, Join *to, int alloc));
extern Join IMakeJoin ARGS((Cost cost, Count plan_size, Count plan_width, int fragment, int parallel, EStatePtr state, List qptargetlist, List qpqual, PlanPtr lefttree, PlanPtr righttree, JoinRuleInfoPtr ruleinfo));
extern void RInitNestLoop ARGS((Pointer p));
extern NestLoop MakeNestLoop ARGS((NestLoopState nlstate, JoinRuleInfo nlRuleInfo));
extern void OutNestLoop ARGS((StringInfo str, NestLoop node));
extern bool EqualNestLoop ARGS((NestLoop a, NestLoop b));
extern bool CopyNestLoop ARGS((NestLoop from, NestLoop *to, int alloc));
extern NestLoop IMakeNestLoop ARGS((Cost cost, Count plan_size, Count plan_width, int fragment, int parallel, NestLoopState nl state, List qptargetlist, List qpqual, PlanPtr lefttree, PlanPtr righttree, JoinRuleInfoPtr ruleinfo, int nlstate, JoinRuleInfo nlRuleInfo));
extern void RInitMergeJoin ARGS((Pointer p));
extern MergeJoin MakeMergeJoin ARGS((List mergeclauses, ObjectId mergesortop, List mergerightorder, List mergeleftorder, MergeJoinState mergestate));
extern void OutMergeJoin ARGS((StringInfo str, MergeJoin node));
extern bool EqualMergeJoin ARGS((MergeJoin a, MergeJoin b));
extern bool CopyMergeJoin ARGS((MergeJoin from, MergeJoin *to, int alloc));
extern MergeJoin IMakeMergeJoin ARGS((Cost cost, Count plan_size, Count plan_width, int fragment, int parallel, MergeJoinState merge state, List qptargetlist, List qpqual, PlanPtr lefttree, PlanPtr righttree, JoinRuleInfoPtr ruleinfo, List mergeclauses, ObjectId mergesortop, List mergerightorder, List mergeleftorder, int mergestate));
extern void RInitHashJoin ARGS((Pointer p));
extern HashJoin MakeHashJoin ARGS((List hashclauses, ObjectId hashjoinop, HashJoinState hashjoinstate, HashJoinTable hashjointable, IpcMemoryKey hashjointablekey, int hashjointablesize, bool hashdone));
extern void OutHashJoin ARGS((StringInfo str, HashJoin node));
extern bool EqualHashJoin ARGS((HashJoin a, HashJoin b));
extern bool CopyHashJoin ARGS((HashJoin from, HashJoin *to, int alloc));
extern HashJoin IMakeHashJoin ARGS((Cost cost, Count plan_size, Count plan_width, int fragment, int parallel, HashJoinState hashjoin state, List qptargetlist, List qpqual, PlanPtr lefttree, PlanPtr righttree, JoinRuleInfoPtr ruleinfo, List hashclauses, ObjectId hashjoinop, int hashjoinstate, HashJoinTable hashjointable, IpcMemoryKey hashjointablekey, int hashjointablesize, bool hashdone));
extern void RInitTemp ARGS((Pointer p));
extern Temp MakeTemp ARGS((ObjectId tempid, Count keycount));
extern void OutTemp ARGS((StringInfo str, Temp node));
extern bool EqualTemp ARGS((Temp a, Temp b));
extern bool CopyTemp ARGS((Temp from, Temp *to, int alloc));
extern Temp IMakeTemp ARGS((Cost cost, Count plan_size, Count plan_width, int fragment, int parallel, EStatePtr state, List qptargetlist, List qpqual, PlanPtr lefttree, PlanPtr righttree, ObjectId tempid, Count keycount));
extern void RInitMaterial ARGS((Pointer p));
extern Material MakeMaterial ARGS((MaterialState matstate));
extern void OutMaterial ARGS((StringInfo str, Material node));
extern bool EqualMaterial ARGS((Material a, Material b));
extern bool CopyMaterial ARGS((Material from, Material *to, int alloc));
extern Material IMakeMaterial ARGS((Cost cost, Count plan_size, Count plan_width, int fragment, int parallel, MaterialState mat state, List qptargetlist, List qpqual, PlanPtr lefttree, PlanPtr righttree, ObjectId tempid, Count keycount, int matstate));
extern void RInitSort ARGS((Pointer p));
extern Sort MakeSort ARGS((SortState sortstate));
extern void OutSort ARGS((StringInfo str, Sort node));
extern bool EqualSort ARGS((Sort a, Sort b));
extern bool CopySort ARGS((Sort from, Sort *to, int alloc));
extern Sort IMakeSort ARGS((Cost cost, Count plan_size, Count plan_width, int fragment, int parallel, SortState sort state, List qptargetlist, List qpqual, PlanPtr lefttree, PlanPtr righttree, ObjectId tempid, Count keycount, int sortstate));
extern void RInitUnique ARGS((Pointer p));
extern Unique MakeUnique ARGS((UniqueState uniquestate));
extern void OutUnique ARGS((StringInfo str, Unique node));
extern bool EqualUnique ARGS((Unique a, Unique b));
extern bool CopyUnique ARGS((Unique from, Unique *to, int alloc));
extern Unique IMakeUnique ARGS((Cost cost, Count plan_size, Count plan_width, int fragment, int parallel, UniqueState unique state, List qptargetlist, List qpqual, PlanPtr lefttree, PlanPtr righttree, ObjectId tempid, Count keycount, int uniquestate));
extern void RInitHash ARGS((Pointer p));
extern Hash MakeHash ARGS((Var hashkey, HashState hashstate, HashJoinTable hashtable, IpcMemoryKey hashtablekey, int hashtablesize));
extern void OutHash ARGS((StringInfo str, Hash node));
extern bool EqualHash ARGS((Hash a, Hash b));
extern bool CopyHash ARGS((Hash from, Hash *to, int alloc));
extern Hash IMakeHash ARGS((Cost cost, Count plan_size, Count plan_width, int fragment, int parallel, HashState hash state, List qptargetlist, List qpqual, PlanPtr lefttree, PlanPtr righttree, Var hashkey, int hashstate, HashJoinTable hashtable, IpcMemoryKey hashtablekey, int hashtablesize));
extern void RInitChoose ARGS((Pointer p));
extern Choose MakeChoose ARGS((List chooseplanlist));
extern void OutChoose ARGS((StringInfo str, Choose node));
extern bool EqualChoose ARGS((Choose a, Choose b));
extern bool CopyChoose ARGS((Choose from, Choose *to, int alloc));
extern Choose IMakeChoose ARGS((Cost cost, Count plan_size, Count plan_width, int fragment, int parallel, EStatePtr state, List qptargetlist, List qpqual, PlanPtr lefttree, PlanPtr righttree, List chooseplanlist));
