/* ----------------------------------------------------------------
 *   FILE
 *	relation.h
 *
 *   DESCRIPTION
 *	prototypes for relation.c.
 *
 *   NOTES
 *	Automatically generated using mkproto
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef relationIncluded		/* include this file only once */
#define relationIncluded	1

#include "nodes/pg_lisp.h"

extern void RInitRel ARGS((Pointer p));
extern Rel MakeRel ARGS((Relid relids, bool indexed, Count pages, Count tuples, Count size, Count width, List targetlist, List pathlist, PathPtr unorderedpath, PathPtr cheapestpath, bool pruneable, List classlist, List indexkeys, oid indproc, List indpred, List ordering, List clauseinfo, List joininfo, List innerjoin, List superrels));
extern void OutRel ARGS((StringInfo str, Rel node));
extern bool EqualRel ARGS((Rel a, Rel b));
extern bool CopyRel ARGS((Rel from, Rel *to, char *(*alloc)()));
extern Rel IMakeRel ARGS((Relid relids, bool indexed, Count pages, Count tuples, Count size, Count width, List targetlist, List pathlist, PathPtr unorderedpath, PathPtr cheapestpath, bool pruneable, List classlist, List indexkeys, oid indproc, List indpred, List ordering, List clauseinfo, List joininfo, List innerjoin, List superrels));
extern Rel RMakeRel ARGS((void));
extern void RInitSortKey ARGS((Pointer p));
extern SortKey MakeSortKey ARGS((List varkeys, List sortkeys, Relid relid, List sortorder));
extern void OutSortKey ARGS((StringInfo str, SortKey node));
extern bool EqualSortKey ARGS((SortKey a, SortKey b));
extern bool CopySortKey ARGS((SortKey from, SortKey *to, char *(*alloc)()));
extern SortKey IMakeSortKey ARGS((List varkeys, List sortkeys, Relid relid, List sortorder));
extern SortKey RMakeSortKey ARGS((void));
extern void RInitPath ARGS((Pointer p));
extern Path MakePath ARGS((int32 pathtype, Rel parent, Cost path_cost, List p_ordering, List keys, SortKey pathsortkey, Cost outerjoincost, Relid joinid, List locclauseinfo));
extern void OutPath ARGS((StringInfo str, Path node));
extern bool EqualPath ARGS((Path a, Path b));
extern bool CopyPath ARGS((Path from, Path *to, char *(*alloc)()));
extern Path IMakePath ARGS((int32 pathtype, Rel parent, Cost path_cost, List p_ordering, List keys, SortKey pathsortkey, Cost outerjoincost, Relid joinid, List locclauseinfo));
extern Path RMakePath ARGS((void));
extern void RInitIndexPath ARGS((Pointer p));
extern IndexPath MakeIndexPath ARGS((List indexid, List indexqual));
extern void OutIndexPath ARGS((StringInfo str, IndexPath node));
extern bool EqualIndexPath ARGS((IndexPath a, IndexPath b));
extern bool CopyIndexPath ARGS((IndexPath from, IndexPath *to, char *(*alloc)()));
extern IndexPath IMakeIndexPath ARGS((List indexid, List indexqual));
extern IndexPath RMakeIndexPath ARGS((void));
extern void RInitJoinPath ARGS((Pointer p));
extern JoinPath MakeJoinPath ARGS((List pathclauseinfo, pathPtr outerjoinpath, pathPtr innerjoinpath));
extern void OutJoinPath ARGS((StringInfo str, JoinPath node));
extern bool EqualJoinPath ARGS((JoinPath a, JoinPath b));
extern bool CopyJoinPath ARGS((JoinPath from, JoinPath *to, char *(*alloc)()));
extern JoinPath IMakeJoinPath ARGS((List pathclauseinfo, pathPtr outerjoinpath, pathPtr innerjoinpath));
extern JoinPath RMakeJoinPath ARGS((void));
extern void RInitMergePath ARGS((Pointer p));
extern MergePath MakeMergePath ARGS((List path_mergeclauses, List outersortkeys, List innersortkeys));
extern void OutMergePath ARGS((StringInfo str, MergePath node));
extern bool EqualMergePath ARGS((MergePath a, MergePath b));
extern bool CopyMergePath ARGS((MergePath from, MergePath *to, char *(*alloc)()));
extern MergePath IMakeMergePath ARGS((List path_mergeclauses, List outersortkeys, List innersortkeys));
extern MergePath RMakeMergePath ARGS((void));
extern void RInitHashPath ARGS((Pointer p));
extern HashPath MakeHashPath ARGS((List path_hashclauses, List outerhashkeys, List innerhashkeys));
extern void OutHashPath ARGS((StringInfo str, HashPath node));
extern bool EqualHashPath ARGS((HashPath a, HashPath b));
extern bool CopyHashPath ARGS((HashPath from, HashPath *to, char *(*alloc)()));
extern HashPath IMakeHashPath ARGS((List path_hashclauses, List outerhashkeys, List innerhashkeys));
extern HashPath RMakeHashPath ARGS((void));
extern void RInitOrderKey ARGS((Pointer p));
extern OrderKey MakeOrderKey ARGS((int attribute_number, Index array_index));
extern void OutOrderKey ARGS((StringInfo str, OrderKey node));
extern bool EqualOrderKey ARGS((OrderKey a, OrderKey b));
extern bool CopyOrderKey ARGS((OrderKey from, OrderKey *to, char *(*alloc)()));
extern OrderKey IMakeOrderKey ARGS((int attribute_number, Index array_index));
extern OrderKey RMakeOrderKey ARGS((void));
extern void RInitJoinKey ARGS((Pointer p));
extern JoinKey MakeJoinKey ARGS((LispValue outer, LispValue inner));
extern void OutJoinKey ARGS((StringInfo str, JoinKey node));
extern bool EqualJoinKey ARGS((JoinKey a, JoinKey b));
extern bool CopyJoinKey ARGS((JoinKey from, JoinKey *to, char *(*alloc)()));
extern JoinKey IMakeJoinKey ARGS((LispValue outer, LispValue inner));
extern JoinKey RMakeJoinKey ARGS((void));
extern void RInitMergeOrder ARGS((Pointer p));
extern MergeOrder MakeMergeOrder ARGS((ObjectId join_operator, ObjectId left_operator, ObjectId right_operator, ObjectId left_type, ObjectId right_type));
extern void OutMergeOrder ARGS((StringInfo str, MergeOrder node));
extern bool EqualMergeOrder ARGS((MergeOrder a, MergeOrder b));
extern bool CopyMergeOrder ARGS((MergeOrder from, MergeOrder *to, char *(*alloc)()));
extern MergeOrder IMakeMergeOrder ARGS((ObjectId join_operator, ObjectId left_operator, ObjectId right_operator, ObjectId left_type, ObjectId right_type));
extern MergeOrder RMakeMergeOrder ARGS((void));
extern void RInitCInfo ARGS((Pointer p));
extern CInfo MakeCInfo ARGS((LispValue clause, Cost selectivity, bool notclause, List indexids, MergeOrder mergesortorder, ObjectId hashjoinoperator, Relid cinfojoinid));
extern void OutCInfo ARGS((StringInfo str, CInfo node));
extern bool EqualCInfo ARGS((CInfo a, CInfo b));
extern bool CopyCInfo ARGS((CInfo from, CInfo *to, char *(*alloc)()));
extern CInfo IMakeCInfo ARGS((LispValue clause, Cost selectivity, bool notclause, List indexids, MergeOrder mergesortorder, ObjectId hashjoinoperator, Relid cinfojoinid));
extern CInfo RMakeCInfo ARGS((void));
extern void RInitJoinMethod ARGS((Pointer p));
extern JoinMethod MakeJoinMethod ARGS((List jmkeys, List clauses));
extern void OutJoinMethod ARGS((StringInfo str, JoinMethod node));
extern bool EqualJoinMethod ARGS((JoinMethod a, JoinMethod b));
extern bool CopyJoinMethod ARGS((JoinMethod from, JoinMethod *to, char *(*alloc)()));
extern JoinMethod IMakeJoinMethod ARGS((List jmkeys, List clauses));
extern JoinMethod RMakeJoinMethod ARGS((void));
extern void RInitHInfo ARGS((Pointer p));
extern HInfo MakeHInfo ARGS((ObjectId hashop));
extern void OutHInfo ARGS((StringInfo str, HInfo node));
extern bool EqualHInfo ARGS((HInfo a, HInfo b));
extern bool CopyHInfo ARGS((HInfo from, HInfo *to, char *(*alloc)()));
extern HInfo IMakeHInfo ARGS((ObjectId hashop));
extern HInfo RMakeHInfo ARGS((void));
extern void RInitMInfo ARGS((Pointer p));
extern MInfo MakeMInfo ARGS((MergeOrder m_ordering));
extern void OutMInfo ARGS((StringInfo str, MInfo node));
extern bool EqualMInfo ARGS((MInfo a, MInfo b));
extern bool CopyMInfo ARGS((MInfo from, MInfo *to, char *(*alloc)()));
extern MInfo IMakeMInfo ARGS((MergeOrder m_ordering));
extern MInfo RMakeMInfo ARGS((void));
extern void RInitJInfo ARGS((Pointer p));
extern JInfo MakeJInfo ARGS((List otherrels, List jinfoclauseinfo, bool mergesortable, bool hashjoinable, bool inactive));
extern void OutJInfo ARGS((StringInfo str, JInfo node));
extern bool EqualJInfo ARGS((JInfo a, JInfo b));
extern bool CopyJInfo ARGS((JInfo from, JInfo *to, char *(*alloc)()));
extern JInfo IMakeJInfo ARGS((List otherrels, List jinfoclauseinfo, bool mergesortable, bool hashjoinable, bool inactive));
extern JInfo RMakeJInfo ARGS((void));
extern void RInitIter ARGS((Pointer p));
extern Iter MakeIter ARGS((LispValue iterexpr));
extern void OutIter ARGS((StringInfo str, Iter node));
extern bool EqualIter ARGS((Iter a, Iter b));
extern bool CopyIter ARGS((Iter from, Iter *to, char *(*alloc)()));
extern Iter IMakeIter ARGS((LispValue iterexpr));
extern Iter RMakeIter ARGS((void));
extern void RInitStream ARGS((Pointer p));
extern Stream MakeStream ARGS((pathPtr pathptr, CInfo cinfo, int clausetype, StreamPtr upstream, StreamPtr downstream, bool groupup, Cost groupcost, Cost groupsel));
extern void OutStream ARGS((StringInfo str, Stream node));
extern bool EqualStream ARGS((Stream a, Stream b));
extern bool CopyStream ARGS((Stream from, Stream *to, char *(*alloc)()));
extern Stream IMakeStream ARGS((pathPtr pathptr, CInfo cinfo, int clausetype, StreamPtr upstream, StreamPtr downstream, bool groupup, Cost groupcost, Cost groupsel));
extern Stream RMakeStream ARGS((void));

#endif /* relationIncluded */
