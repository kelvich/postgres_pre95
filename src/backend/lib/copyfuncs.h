/* ----------------------------------------------------------------
 *   FILE
 *	copyfuncs.h
 *
 *   DESCRIPTION
 *	prototypes for copyfuncs.c.
 *
 *   NOTES
 *	Automatically generated using mkproto
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef copyfuncsIncluded		/* include this file only once */
#define copyfuncsIncluded	1

#include "nodes/primnodes.h"
#include "nodes/plannodes.h"
#include "nodes/relation.h"

extern LispValue lispCopy ARGS((LispValue lispObject));
extern bool NodeCopy ARGS((Node from, Node *toP, char *(*alloc)()));
extern Node CopyObject ARGS((Node from));
extern Node CopyObjectUsing ARGS((Node from, char *(*alloc)()));
extern bool CopyNodeFields ARGS((Node from, Node newnode, char *(*alloc)()));
extern bool _copyNode ARGS((Node from, Node *to, char *(*alloc)()));
extern bool CopyPlanFields ARGS((Plan from, Plan newnode, char *(*alloc)()));
extern bool _copyPlan ARGS((Plan from, Plan *to, char *(*alloc)()));
extern bool _copyExistential ARGS((Existential from, Existential *to, char *(*alloc)()));
extern bool _copyResult ARGS((Result from, Result *to, char *(*alloc)()));
extern bool _copyAppend ARGS((Append from, Append *to, char *(*alloc)()));
extern bool CopyScanFields ARGS((Scan from, Scan newnode, char *(*alloc)()));
extern Relation CopyRelDescUsing ARGS((Relation reldesc, char *(*alloc)()));
extern List copyRelDescsUsing ARGS((List relDescs, char *(*alloc)()));
extern bool CopyScanTempFields ARGS((ScanTemps from, ScanTemps newnode, char *(*alloc)()));
extern bool _copyScan ARGS((Scan from, Scan *to, char *(*alloc)()));
extern bool _copyScanTemps ARGS((ScanTemps from, ScanTemps *to, char *(*alloc)()));
extern bool _copySeqScan ARGS((SeqScan from, SeqScan *to, char *(*alloc)()));
extern bool _copyIndexScan ARGS((IndexScan from, IndexScan *to, char *(*alloc)()));
extern bool _copyJoinRuleInfo ARGS((JoinRuleInfo from, JoinRuleInfo *to, char *(*alloc)()));
extern bool CopyJoinFields ARGS((Join from, Join newnode, char *(*alloc)()));
extern bool _copyJoin ARGS((Join from, Join *to, char *(*alloc)()));
extern bool _copyNestLoop ARGS((NestLoop from, NestLoop *to, char *(*alloc)()));
extern bool _copyMergeJoin ARGS((MergeJoin from, MergeJoin *to, char *(*alloc)()));
extern bool _copyHashJoin ARGS((HashJoin from, HashJoin *to, char *(*alloc)()));
extern bool CopyTempFields ARGS((Temp from, Temp newnode, char *(*alloc)()));
extern bool _copyTemp ARGS((Temp from, Temp *to, char *(*alloc)()));
extern bool _copyMaterial ARGS((Material from, Material *to, char *(*alloc)()));
extern bool _copySort ARGS((Sort from, Sort *to, char *(*alloc)()));
extern bool _copyAgg ARGS((Agg from, Agg *to, char *(*alloc)()));
extern bool _copyUnique ARGS((Unique from, Unique *to, char *(*alloc)()));
extern bool _copyHash ARGS((Hash from, Hash *to, char *(*alloc)()));
extern bool _copyResdom ARGS((Resdom from, Resdom *to, char *(*alloc)()));
extern bool _copyFjoin ARGS((Fjoin from, Fjoin *to, char *(*alloc)()));
extern bool CopyExprFields ARGS((Expr from, Expr newnode, char *(*alloc)()));
extern bool _copyExpr ARGS((Expr from, Expr *to, char *(*alloc)()));
extern bool _copyIter ARGS((Iter from, Iter *to, char *(*alloc)()));
extern bool _copyStream ARGS((Stream from, Stream *to, char *(*alloc)()));
extern bool _copyVar ARGS((Var from, Var *to, char *(*alloc)()));
extern bool _copyArray ARGS((Array from, Array *to, char *(*alloc)()));
extern bool _copyArrayRef ARGS((ArrayRef from, ArrayRef *to, char *(*alloc)()));
extern bool _copyOper ARGS((Oper from, Oper *to, char *(*alloc)()));
extern bool _copyConst ARGS((Const from, Const *to, char *(*alloc)()));
extern bool _copyParam ARGS((Param from, Param *to, char *(*alloc)()));
extern bool _copyFunc ARGS((Func from, Func *to, char *(*alloc)()));
extern bool _copyRel ARGS((Rel from, Rel *to, char *(*alloc)()));
extern bool _copySortKey ARGS((SortKey from, SortKey *to, char *(*alloc)()));
extern bool CopyPathFields ARGS((Path from, Path newnode, char *(*alloc)()));
extern bool _copyPath ARGS((Path from, Path *to, char *(*alloc)()));
extern bool _copyIndexPath ARGS((IndexPath from, IndexPath *to, char *(*alloc)()));
extern bool CopyJoinPathFields ARGS((JoinPath from, JoinPath newnode, char *(*alloc)()));
extern bool _copyJoinPath ARGS((JoinPath from, JoinPath *to, char *(*alloc)()));
extern bool _copyMergePath ARGS((MergePath from, MergePath *to, char *(*alloc)()));
extern bool _copyHashPath ARGS((HashPath from, HashPath *to, char *(*alloc)()));
extern bool _copyOrderKey ARGS((OrderKey from, OrderKey *to, char *(*alloc)()));
extern bool _copyJoinKey ARGS((JoinKey from, JoinKey *to, char *(*alloc)()));
extern bool _copyMergeOrder ARGS((MergeOrder from, MergeOrder *to, char *(*alloc)()));
extern bool _copyCInfo ARGS((CInfo from, CInfo *to, char *(*alloc)()));
extern bool CopyJoinMethodFields ARGS((JoinMethod from, JoinMethod newnode, char *(*alloc)()));
extern bool _copyJoinMethod ARGS((JoinMethod from, JoinMethod *to, char *(*alloc)()));
extern bool _copyHInfo ARGS((HInfo from, HInfo *to, char *(*alloc)()));
extern bool _copyMInfo ARGS((MInfo from, MInfo *to, char *(*alloc)()));
extern bool _copyJInfo ARGS((JInfo from, JInfo *to, char *(*alloc)()));
extern bool _copyLispValue ARGS((LispValue from, LispValue *to, char *(*alloc)()));
extern bool _copyLispSymbol ARGS((LispValue from, LispValue *to, char *(*alloc)()));
extern bool _copyLispStr ARGS((LispValue from, LispValue *to, char *(*alloc)()));
extern bool _copyLispInt ARGS((LispValue from, LispValue *to, char *(*alloc)()));
extern bool _copyLispFloat ARGS((LispValue from, LispValue *to, char *(*alloc)()));
extern bool _copyLispVector ARGS((LispValue from, LispValue *to, char *(*alloc)()));
extern bool _copyLispList ARGS((LispValue from, LispValue *to, char *(*alloc)()));

#endif /* copyfuncsIncluded */
