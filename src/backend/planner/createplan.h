/* ----------------------------------------------------------------
 *   FILE
 *	createplan.h
 *
 *   DESCRIPTION
 *	prototypes for createplan.c.
 *
 *   NOTES
 *	Automatically generated using mkproto
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef createplanIncluded		/* include this file only once */
#define createplanIncluded	1

extern Plan create_plan ARGS((Path best_path));
extern Scan create_scan_node ARGS((Path best_path, List tlist));
extern Join create_join_node ARGS((JoinPath best_path, List tlist));
extern SeqScan create_seqscan_node ARGS((Path best_path, List tlist, LispValue scan_clauses));
extern IndexScan create_indexscan_node ARGS((IndexPath best_path, List tlist, List scan_clauses));
extern LispValue fix_indxqual_references ARGS((LispValue clause, Path index_path));
extern NestLoop create_nestloop_node ARGS((JoinPath best_path, List tlist, List clauses, Plan outer_node, List outer_tlist, Plan inner_node, List inner_tlist));
extern MergeJoin create_mergejoin_node ARGS((MergePath best_path, List tlist, List clauses, Plan outer_node, List outer_tlist, Plan inner_node, List inner_tlist));
extern LispValue switch_outer ARGS((LispValue clauses));
extern HashJoin create_hashjoin_node ARGS((HashPath best_path, List tlist, List clauses, Plan outer_node, List outer_tlist, Plan inner_node, List inner_tlist));
extern Temp make_temp ARGS((List tlist, List keys, List operators, Plan plan_node, int temptype));
extern List set_temp_tlist_operators ARGS((List tlist, List pathkeys, List operators));
extern SeqScan make_seqscan ARGS((List qptlist, List qpqual, Index scanrelid, Plan lefttree));
extern IndexScan make_indexscan ARGS((List qptlist, List qpqual, Index scanrelid, List indxid, List indxqual));
extern NestLoop make_nestloop ARGS((List qptlist, List qpqual, Plan lefttree, Plan righttree));
extern HashJoin make_hashjoin ARGS((LispValue tlist, LispValue qpqual, LispValue hashclauses, Plan lefttree, Plan righttree));
extern Hash make_hash ARGS((LispValue tlist, Var hashkey, Plan lefttree));
extern MergeJoin make_mergesort ARGS((LispValue tlist, LispValue qpqual, LispValue mergeclauses, ObjectId opcode, LispValue rightorder, LispValue leftorder, Plan righttree, Plan lefttree));
extern Sort make_sort ARGS((LispValue tlist, ObjectId tempid, Plan lefttree, Count keycount));
extern Material make_material ARGS((LispValue tlist, ObjectId tempid, Plan lefttree, Count keycount));
extern Agg make_agg ARGS((List arglist, ObjectId aggidnum));
extern Unique make_unique ARGS((List tlist, Plan lefttree));
extern List generate_fjoin ARGS((List tlist));

#endif /* createplanIncluded */
