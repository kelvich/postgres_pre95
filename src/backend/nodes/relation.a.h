/* $Header$ */

#include "pg_lisp.h"
extern void set_relids ARGS((Rel node, Relid value));
extern Relid get_relids ARGS((Rel node));
extern void set_indexed ARGS((Rel node, bool value));
extern bool get_indexed ARGS((Rel node));
extern void set_pages ARGS((Rel node, Count value));
extern Count get_pages ARGS((Rel node));
extern void set_tuples ARGS((Rel node, Count value));
extern Count get_tuples ARGS((Rel node));
extern void set_size ARGS((Rel node, Count value));
extern Count get_size ARGS((Rel node));
extern void set_width ARGS((Rel node, Count value));
extern Count get_width ARGS((Rel node));
extern void set_targetlist ARGS((Rel node, List value));
extern List get_targetlist ARGS((Rel node));
extern void set_pathlist ARGS((Rel node, List value));
extern List get_pathlist ARGS((Rel node));
extern void set_classlist ARGS((Rel node, List value));
extern List get_classlist ARGS((Rel node));
extern void set_indexkeys ARGS((Rel node, List value));
extern List get_indexkeys ARGS((Rel node));
extern void set_ordering ARGS((Rel node, List value));
extern List get_ordering ARGS((Rel node));
extern void set_clauseinfo ARGS((Rel node, List value));
extern List get_clauseinfo ARGS((Rel node));
extern void set_joininfo ARGS((Rel node, List value));
extern List get_joininfo ARGS((Rel node));
extern void set_innerjoin ARGS((Rel node, List value));
extern List get_innerjoin ARGS((Rel node));
extern void set_varkeys ARGS((SortKey node, List value));
extern List get_varkeys ARGS((SortKey node));
extern void set_sortkeys ARGS((SortKey node, List value));
extern List get_sortkeys ARGS((SortKey node));
extern void set_relid ARGS((SortKey node, Relid value));
extern Relid get_relid ARGS((SortKey node));
extern void set_sortorder ARGS((SortKey node, List value));
extern List get_sortorder ARGS((SortKey node));
extern void set_pathtype ARGS((Path node, int32 value));
extern int32 get_pathtype ARGS((Path node));
extern void set_parent ARGS((Path node, Rel value));
extern Rel get_parent ARGS((Path node));
extern void set_path_cost ARGS((Path node, Cost value));
extern Cost get_path_cost ARGS((Path node));
extern void set_p_ordering ARGS((Path node, List value));
extern List get_p_ordering ARGS((Path node));
extern void set_keys ARGS((Path node, List value));
extern List get_keys ARGS((Path node));
extern void set_sortpath ARGS((Path node, SortKey value));
extern SortKey get_sortpath ARGS((Path node));
extern void set_indexid ARGS((IndexPath node, ObjectId value));
extern ObjectId get_indexid ARGS((IndexPath node));
extern void set_indexqual ARGS((IndexPath node, List value));
extern List get_indexqual ARGS((IndexPath node));
extern void set_pathclauseinfo ARGS((JoinPath node, List value));
extern List get_pathclauseinfo ARGS((JoinPath node));
extern void set_outerjoinpath ARGS((JoinPath node, struct path *value));
extern struct path *get_outerjoinpath ARGS((JoinPath node));
extern void set_innerjoinpath ARGS((JoinPath node, struct path *value));
extern struct path *get_innerjoinpath ARGS((JoinPath node));
extern void set_outerjoincost ARGS((JoinPath node, Cost value));
extern Cost get_outerjoincost ARGS((JoinPath node));
extern void set_joinid ARGS((JoinPath node, Relid value));
extern Relid get_joinid ARGS((JoinPath node));

extern void set_cinfojoinid ARGS((CInfo node, Relid value));
extern Relid get_cinfojoinid ARGS((CInfo node));

extern void set_mergeclauses ARGS((MergePath node, List value));
extern List get_mergeclauses ARGS((MergePath node));
extern void set_outersortkeys ARGS((MergePath node, List value));
extern List get_outersortkeys ARGS((MergePath node));
extern void set_innersortkeys ARGS((MergePath node, List value));
extern List get_innersortkeys ARGS((MergePath node));
extern void set_hashclauses ARGS((HashPath node, List value));
extern List get_hashclauses ARGS((HashPath node));
extern void set_outerhashkeys ARGS((HashPath node, List value));
extern List get_outerhashkeys ARGS((HashPath node));
extern void set_innerhashkeys ARGS((HashPath node, List value));
extern List get_innerhashkeys ARGS((HashPath node));
extern void set_attribute_number ARGS((OrderKey node, int value));
extern int get_attribute_number ARGS((OrderKey node));
extern void set_array_index ARGS((OrderKey node, Index value));
extern Index get_array_index ARGS((OrderKey node));
extern void set_outer ARGS((JoinKey node, Node value));
extern Node get_outer ARGS((JoinKey node));
extern void set_inner ARGS((JoinKey node, Node value));
extern Node get_inner ARGS((JoinKey node));
extern void set_join_operator ARGS((MergeOrder node, ObjectId value));
extern ObjectId get_join_operator ARGS((MergeOrder node));
extern void set_left_operator ARGS((MergeOrder node, ObjectId value));
extern ObjectId get_left_operator ARGS((MergeOrder node));
extern void set_right_operator ARGS((MergeOrder node, ObjectId value));
extern ObjectId get_right_operator ARGS((MergeOrder node));
extern void set_left_type ARGS((MergeOrder node, ObjectId value));
extern ObjectId get_left_type ARGS((MergeOrder node));
extern void set_right_type ARGS((MergeOrder node, ObjectId value));
extern ObjectId get_right_type ARGS((MergeOrder node));
extern void set_clause ARGS((CInfo node, Expr value));
extern Expr get_clause ARGS((CInfo node));
extern void set_selectivity ARGS((CInfo node, Cost value));
extern Cost get_selectivity ARGS((CInfo node));
extern void set_notclause ARGS((CInfo node, bool value));
extern bool get_notclause ARGS((CInfo node));
extern void set_indexids ARGS((CInfo node, List value));
extern List get_indexids ARGS((CInfo node));
extern void set_mergesortorder ARGS((CInfo node, MergeOrder value));
extern MergeOrder get_mergesortorder ARGS((CInfo node));
extern void set_hashjoinoperator ARGS((CInfo node, ObjectId value));
extern ObjectId get_hashjoinoperator ARGS((CInfo node));
extern void set_otherrels ARGS((JInfo node, List value));
extern List get_otherrels ARGS((JInfo node));
extern void set_jinfoclauseinfo ARGS((JInfo node, List value));
extern List get_jinfoclauseinfo ARGS((JInfo node));
extern void set_mergesortable ARGS((JInfo node, bool value));
extern bool get_mergesortable ARGS((JInfo node));
extern void set_hashjoinable ARGS((JInfo node, bool value));
extern bool get_hashjoinable ARGS((JInfo node));
extern Rel MakeRel ARGS(());

extern List get_superrels ARGS((Rel node));
extern void set_superrels ARGS((Rel node, List value));
extern bool get_inactive ARGS((JInfo node));
extern void set_inactive ARGS((JInfo node, bool value));

extern SortKey MakeSortKey ARGS((List varkeys, List sortkeys, Relid relid, List sortorder));
extern Path MakePath ARGS((SortKey sort pathtype, int parent, int cost, int ordering, int keys, int sortpath));
extern IndexPath MakeIndexPath ARGS((ObjectId indexid, List indexqual));
extern JoinPath MakeJoinPath ARGS((int clauseinfo, int path, int path, int outerjoincost, Relid joinid));
extern MergePath MakeMergePath ARGS((List mergeclauses, List outersortkeys, List innersortkeys));
extern HashPath MakeHashPath ARGS((List hashclauses, List outerhashkeys, List innerhashkeys));
extern OrderKey MakeOrderKey ARGS((int attribute_number, Index array_index));
extern JoinKey MakeJoinKey ARGS((Node outer, Node inner));
extern MergeOrder MakeMergeOrder ARGS((ObjectId join_operator, ObjectId left_operator, ObjectId right_operator, ObjectId left_type, ObjectId right_type));
extern CInfo MakeCInfo ARGS((bool not clause, Cost selectivity, int notclause, List indexids, MergeOrder mergesortorder, ObjectId hashjoinoperator));
extern JInfo MakeJInfo ARGS((List otherrels, List clauseinfo, bool mergesortable, bool hashjoinable));
extern JoinMethod MakeJoinMethod ARGS((List jmkeys, List clauses));
extern List get_jmkeys ARGS((JoinMethod node));
extern List get_clauses ARGS((JoinMethod node));
extern void set_jmkeys ARGS((JoinMethod node, List value));
extern void set_clauses ARGS((JoinMethod node, List value));
extern HInfo MakeHInfo ARGS((List jmkeys, List clauses, ObjectId hashop));
extern ObjectId get_hashop ARGS((HInfo node));
extern void set_hashop ARGS((HInfo node));
extern MInfo MakeMInfo ARGS((List jmkeys, List clauses, List m_ordering));
extern void set_m_ordering ARGS((MInfo node));
extern List get_m_ordering ARGS((MInfo node, List m_ordering));
