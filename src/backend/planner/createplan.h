/* $Header$ */
extern Plan create_plan ARGS((Path best_path, List origtlist));
extern Scan create_scan_node ARGS((Path best_path, List tlist));
extern Join create_join_node ARGS((Path best_path, List origtlist, List tlist));
extern SeqScan create_seqscan_node ARGS((LispValue best_path, LispValue tlist, LispValue scan_clauses));
extern IndexScan create_indexscan_node ARGS((Path best_path, List tlist, List scan_clauses));
extern LispValue fix_indxqual_references ARGS((LispValue clause, LispValue index_path));
extern NestLoop create_nestloop_node ARGS((Path best_path, List inner_ tlist, List clauses, Node outer_node, List outer_tlist, Node inner_node, int inner_tlist));
extern MergeSort create_mergejoin_node ARGS((Path best_path, List inner_ tlist, List clauses, Node outer_node, List outer_tlist, Node inner_node, int inner_tlist));
extern LispValue switch_outer ARGS((LispValue clauses));
extern HashJoin create_hashjoin_node ARGS((Path best_path, List inner_ tlist, List clauses, Node outer_node, List outer_tlist, int inner_node, int inner_tlist));
extern Temp make_temp ARGS((List tlist, List keys, List operators, Plan plan_node, int temptype));
extern List set_temp_tlist_operators ARGS((List tlist, List pathkeys, List operators));
extern SeqScan make_seqscan ARGS((List qptlist,List qpqual, Index scanrelid Plan lefttree ));

