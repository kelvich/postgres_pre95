/* ----------------------------------------------------------------
 *   FILE
 *	plannodes.h
 *
 *   DESCRIPTION
 *	prototypes for plannodes.c.
 *
 *   NOTES
 *	Automatically generated using mkproto
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef plannodesIncluded		/* include this file only once */
#define plannodesIncluded	1

extern void RInitPlan ARGS((Pointer p));
extern Plan MakePlan ARGS((Cost cost, Count plan_size, Count plan_width, Count plan_tupperpage, int fragment, int parallel, EStatePtr state, List qptargetlist, List qpqual, PlanPtr lefttree, PlanPtr righttree));
extern void OutPlan ARGS((StringInfo str, Plan node));
extern bool EqualPlan ARGS((Plan a, Plan b));
extern bool CopyPlan ARGS((Plan from, Plan *to, char *(*alloc)()));
extern Plan IMakePlan ARGS((Cost cost, Count plan_size, Count plan_width, Count plan_tupperpage, int fragment, int parallel, EStatePtr state, List qptargetlist, List qpqual, PlanPtr lefttree, PlanPtr righttree));
extern Plan RMakePlan ARGS((void));
extern void RInitFragment ARGS((Pointer p));
extern Fragment MakeFragment ARGS((Plan frag_root, Plan frag_parent_op, int frag_parallel, List frag_subtrees, FragmentPtr frag_parent_frag, List frag_parsetree, bool frag_is_inprocess, float frag_iorate));
extern void OutFragment ARGS((StringInfo str, Fragment node));
extern bool EqualFragment ARGS((Fragment a, Fragment b));
extern bool CopyFragment ARGS((Fragment from, Fragment *to, char *(*alloc)()));
extern Fragment IMakeFragment ARGS((Plan frag_root, Plan frag_parent_op, int frag_parallel, List frag_subtrees, FragmentPtr frag_parent_frag, List frag_parsetree, bool frag_is_inprocess, float frag_iorate));
extern Fragment RMakeFragment ARGS((void));
extern void RInitExistential ARGS((Pointer p));
extern Existential MakeExistential ARGS((int frag_root));
extern void OutExistential ARGS((StringInfo str, Existential node));
extern bool EqualExistential ARGS((Existential a, Existential b));
extern bool CopyExistential ARGS((Existential from, Existential *to, char *(*alloc)()));
extern Existential IMakeExistential ARGS((void));
extern Existential RMakeExistential ARGS((void));
extern void RInitResult ARGS((Pointer p));
extern Result MakeResult ARGS((List resrellevelqual, List resconstantqual, ResultState resstate));
extern void OutResult ARGS((StringInfo str, Result node));
extern bool EqualResult ARGS((Result a, Result b));
extern bool CopyResult ARGS((Result from, Result *to, char *(*alloc)()));
extern Result IMakeResult ARGS((List resrellevelqual, List resconstantqual, ResultState resstate));
extern Result RMakeResult ARGS((void));
extern void RInitAppend ARGS((Pointer p));
extern Append MakeAppend ARGS((List unionplans, Index unionrelid, List unionrtentries, AppendState unionstate));
extern void OutAppend ARGS((StringInfo str, Append node));
extern bool EqualAppend ARGS((Append a, Append b));
extern bool CopyAppend ARGS((Append from, Append *to, char *(*alloc)()));
extern Append IMakeAppend ARGS((List unionplans, Index unionrelid, List unionrtentries, AppendState unionstate));
extern Append RMakeAppend ARGS((void));
extern void RInitScan ARGS((Pointer p));
extern Scan MakeScan ARGS((Index scanrelid, ScanState scanstate));
extern void OutScan ARGS((StringInfo str, Scan node));
extern bool EqualScan ARGS((Scan a, Scan b));
extern bool CopyScan ARGS((Scan from, Scan *to, char *(*alloc)()));
extern Scan IMakeScan ARGS((Index scanrelid, ScanState scanstate));
extern Scan RMakeScan ARGS((void));
extern void RInitSeqScan ARGS((Pointer p));
extern SeqScan MakeSeqScan ARGS((int scanrelid));
extern void OutSeqScan ARGS((StringInfo str, SeqScan node));
extern bool EqualSeqScan ARGS((SeqScan a, SeqScan b));
extern bool CopySeqScan ARGS((SeqScan from, SeqScan *to, char *(*alloc)()));
extern SeqScan IMakeSeqScan ARGS((void));
extern SeqScan RMakeSeqScan ARGS((void));
extern void RInitScanTemps ARGS((Pointer p));
extern ScanTemps MakeScanTemps ARGS((List temprelDescs, ScanTempState scantempState));
extern void OutScanTemps ARGS((StringInfo str, ScanTemps node));
extern bool EqualScanTemps ARGS((ScanTemps a, ScanTemps b));
extern bool CopyScanTemps ARGS((ScanTemps from, ScanTemps *to, char *(*alloc)()));
extern ScanTemps IMakeScanTemps ARGS((List temprelDescs, ScanTempState scantempState));
extern ScanTemps RMakeScanTemps ARGS((void));
extern void RInitIndexScan ARGS((Pointer p));
extern IndexScan MakeIndexScan ARGS((List indxid, List indxqual, IndexScanState indxstate));
extern void OutIndexScan ARGS((StringInfo str, IndexScan node));
extern bool EqualIndexScan ARGS((IndexScan a, IndexScan b));
extern bool CopyIndexScan ARGS((IndexScan from, IndexScan *to, char *(*alloc)()));
extern IndexScan IMakeIndexScan ARGS((List indxid, List indxqual, IndexScanState indxstate));
extern IndexScan RMakeIndexScan ARGS((void));
extern void RInitJoinRuleInfo ARGS((Pointer p));
extern JoinRuleInfo MakeJoinRuleInfo ARGS((ObjectId jri_operator, AttributeNumber jri_inattrno, AttributeNumber jri_outattrno, RuleLock jri_lock, ObjectId jri_ruleid, Prs2StubId jri_stubid, Prs2OneStub jri_stub, Prs2StubStats jri_stats));
extern void OutJoinRuleInfo ARGS((StringInfo str, JoinRuleInfo node));
extern bool EqualJoinRuleInfo ARGS((JoinRuleInfo a, JoinRuleInfo b));
extern bool CopyJoinRuleInfo ARGS((JoinRuleInfo from, JoinRuleInfo *to, char *(*alloc)()));
extern JoinRuleInfo IMakeJoinRuleInfo ARGS((ObjectId jri_operator, AttributeNumber jri_inattrno, AttributeNumber jri_outattrno, RuleLock jri_lock, ObjectId jri_ruleid, Prs2StubId jri_stubid, Prs2OneStub jri_stub, Prs2StubStats jri_stats));
extern JoinRuleInfo RMakeJoinRuleInfo ARGS((void));
extern void RInitJoin ARGS((Pointer p));
extern Join MakeJoin ARGS((JoinRuleInfoPtr ruleinfo));
extern void OutJoin ARGS((StringInfo str, Join node));
extern bool EqualJoin ARGS((Join a, Join b));
extern bool CopyJoin ARGS((Join from, Join *to, char *(*alloc)()));
extern Join IMakeJoin ARGS((JoinRuleInfoPtr ruleinfo));
extern Join RMakeJoin ARGS((void));
extern void RInitNestLoop ARGS((Pointer p));
extern NestLoop MakeNestLoop ARGS((NestLoopState nlstate, JoinRuleInfo nlRuleInfo));
extern void OutNestLoop ARGS((StringInfo str, NestLoop node));
extern bool EqualNestLoop ARGS((NestLoop a, NestLoop b));
extern bool CopyNestLoop ARGS((NestLoop from, NestLoop *to, char *(*alloc)()));
extern NestLoop IMakeNestLoop ARGS((NestLoopState nlstate, JoinRuleInfo nlRuleInfo));
extern NestLoop RMakeNestLoop ARGS((void));
extern void RInitMergeJoin ARGS((Pointer p));
extern MergeJoin MakeMergeJoin ARGS((List mergeclauses, ObjectId mergesortop, List mergerightorder, List mergeleftorder, MergeJoinState mergestate));
extern void OutMergeJoin ARGS((StringInfo str, MergeJoin node));
extern bool EqualMergeJoin ARGS((MergeJoin a, MergeJoin b));
extern bool CopyMergeJoin ARGS((MergeJoin from, MergeJoin *to, char *(*alloc)()));
extern MergeJoin IMakeMergeJoin ARGS((List mergeclauses, ObjectId mergesortop, List mergerightorder, List mergeleftorder, MergeJoinState mergestate));
extern MergeJoin RMakeMergeJoin ARGS((void));
extern void RInitHashJoin ARGS((Pointer p));
extern HashJoin MakeHashJoin ARGS((List hashclauses, ObjectId hashjoinop, HashJoinState hashjoinstate, HashJoinTable hashjointable, IpcMemoryKey hashjointablekey, int hashjointablesize, bool hashdone));
extern void OutHashJoin ARGS((StringInfo str, HashJoin node));
extern bool EqualHashJoin ARGS((HashJoin a, HashJoin b));
extern bool CopyHashJoin ARGS((HashJoin from, HashJoin *to, char *(*alloc)()));
extern HashJoin IMakeHashJoin ARGS((List hashclauses, ObjectId hashjoinop, HashJoinState hashjoinstate, HashJoinTable hashjointable, IpcMemoryKey hashjointablekey, int hashjointablesize, bool hashdone));
extern HashJoin RMakeHashJoin ARGS((void));
extern void RInitTemp ARGS((Pointer p));
extern Temp MakeTemp ARGS((ObjectId tempid, Count keycount));
extern void OutTemp ARGS((StringInfo str, Temp node));
extern bool EqualTemp ARGS((Temp a, Temp b));
extern bool CopyTemp ARGS((Temp from, Temp *to, char *(*alloc)()));
extern Temp IMakeTemp ARGS((ObjectId tempid, Count keycount));
extern Temp RMakeTemp ARGS((void));
extern void RInitMaterial ARGS((Pointer p));
extern Material MakeMaterial ARGS((MaterialState matstate));
extern void OutMaterial ARGS((StringInfo str, Material node));
extern bool EqualMaterial ARGS((Material a, Material b));
extern bool CopyMaterial ARGS((Material from, Material *to, char *(*alloc)()));
extern Material IMakeMaterial ARGS((MaterialState matstate));
extern Material RMakeMaterial ARGS((void));
extern void RInitSort ARGS((Pointer p));
extern Sort MakeSort ARGS((SortState sortstate));
extern void OutSort ARGS((StringInfo str, Sort node));
extern bool EqualSort ARGS((Sort a, Sort b));
extern bool CopySort ARGS((Sort from, Sort *to, char *(*alloc)()));
extern Sort IMakeSort ARGS((SortState sortstate));
extern Sort RMakeSort ARGS((void));
extern void RInitAgg ARGS((Pointer p));
extern Agg MakeAgg ARGS((String aggname, AggState aggstate));
extern void OutAgg ARGS((StringInfo str, Agg node));
extern bool EqualAgg ARGS((Agg a, Agg b));
extern bool CopyAgg ARGS((Agg from, Agg *to, char *(*alloc)()));
extern Agg IMakeAgg ARGS((String aggname, AggState aggstate));
extern Agg RMakeAgg ARGS((void));
extern void RInitUnique ARGS((Pointer p));
extern Unique MakeUnique ARGS((UniqueState uniquestate));
extern void OutUnique ARGS((StringInfo str, Unique node));
extern bool EqualUnique ARGS((Unique a, Unique b));
extern bool CopyUnique ARGS((Unique from, Unique *to, char *(*alloc)()));
extern Unique IMakeUnique ARGS((UniqueState uniquestate));
extern Unique RMakeUnique ARGS((void));
extern void RInitHash ARGS((Pointer p));
extern Hash MakeHash ARGS((Var hashkey, HashState hashstate, HashJoinTable hashtable, IpcMemoryKey hashtablekey, int hashtablesize));
extern void OutHash ARGS((StringInfo str, Hash node));
extern bool EqualHash ARGS((Hash a, Hash b));
extern bool CopyHash ARGS((Hash from, Hash *to, char *(*alloc)()));
extern Hash IMakeHash ARGS((Var hashkey, HashState hashstate, HashJoinTable hashtable, IpcMemoryKey hashtablekey, int hashtablesize));
extern Hash RMakeHash ARGS((void));
extern void RInitChoose ARGS((Pointer p));
extern Choose MakeChoose ARGS((List chooseplanlist));
extern void OutChoose ARGS((StringInfo str, Choose node));
extern bool EqualChoose ARGS((Choose a, Choose b));
extern bool CopyChoose ARGS((Choose from, Choose *to, char *(*alloc)()));
extern Choose IMakeChoose ARGS((List chooseplanlist));
extern Choose RMakeChoose ARGS((void));

#endif /* plannodesIncluded */
