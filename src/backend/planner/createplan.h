/* $Header$ */
extern Plan create_plan ARGS((Path best_path, List origtlist));
extern Scan create_scan_node ARGS((Path best_path, List tlist));
extern Join create_join_node ARGS((Path best_path, List origtlist, List tlist));
extern SeqScan create_seqscan_node ARGS((LispValue best_path, LispValue tlist, LispValue scan_clauses));
extern IndexScan create_indexscan_node ARGS((Path best_path, List tlist, List scan_clauses));
extern LispValue fix_indxqual_references ARGS((LispValue clause, LispValue index_path));
extern NestLoop create_nestloop_node ARGS((Path best_path, List inner_ tlist, List clauses, Node outer_node, List outer_tlist, Node inner_node, int inner_tlist));
extern MergeJoin create_mergejoin_node ARGS((Path best_path, List inner_ tlist, List clauses, Node outer_node, List outer_tlist, Node inner_node, int inner_tlist));
extern LispValue switch_outer ARGS((LispValue clauses));
extern HashJoin create_hashjoin_node ARGS((Path best_path, List inner_ tlist, List clauses, Node outer_node, List outer_tlist, int inner_node, int inner_tlist));
extern Temp make_temp ARGS((List tlist, List keys, List operators, Plan plan_node, int temptype));
extern List set_temp_tlist_operators ARGS((List tlist, List pathkeys, List operators));
extern SeqScan make_seqscan ARGS((List qptlist,List qpqual, Index scanrelid Plan lefttree ));
extern NestLoop make_nestloop ARGS((List qptlist, List qpqual, Plan lefttree, Plan righttree ));
extern HashJoin make_hashjoin ARGS((LispValue tlist, LispValue qpqual, List hashclauses, ObjectId opcode, Plan outer_node, Plan inner_node));
extern MergeJoin make_mergesort ARGS((LispValue tlist, LispValue qpqual, List mergeclauses, ObjectId opcode, Plan outer_node, Plan inner_node));
extern Hash make_hash ARGS((List tlist, ObjectId tempid, Plan inner_node, Count keycount));
extern Agg make_agg ARGS((List tlist, ObjectId tempid, Plan lefttree, Name aggname));
extern Sort make_sort ARGS((List tlist, ObjectId tempid, Plan inner_node, Count keycount));
extern IndexScan make_indexscan ARGS((List qptlist, List qpqual, Index scanrelid, List indxid, List indxqual));
extern Unique make_unique ARGS ((List tlist, Plan plannode));


