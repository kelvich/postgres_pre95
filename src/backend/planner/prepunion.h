/* $Header$ */

extern LispValue find_all_inheritors ARGS((LispValue unexamined_relids, LispValue examined_relids));
extern LispValue find_archive_rels ARGS((LispValue relids));
extern int first_matching_rt_entry ARGS((LispValue rangetable, LispValue flag));
extern Append plan_union_queries ARGS((Index rt_index, int flag, LispValue root, LispValue tlist, LispValue qual, LispValue rangetable));
extern LispValue plan_union_query ARGS((LispValue relids, LispValue rt_index, LispValue rt_entry, LispValue root, LispValue tlist, LispValue qual, LispValue rangetable));
extern LispValue new_rangetable_entry ARGS((LispValue new_relid, LispValue old_entry));
extern LispValue subst_rangetable ARGS((LispValue root, LispValue index, LispValue new_entry));
extern LispValue fix_parsetree_attnums ARGS((LispValue rt_index, LispValue old_relid, LispValue new_relid, LispValue parsetree));
extern LispValue fix_rangetable ARGS((LispValue rangetable, LispValue index, LispValue new_entry));
extern TL fix_targetlist ARGS((TL oringtlist, TL tlist));
extern Append make_append ARGS((List unionplans, List union_rt_entries, List tlist, Index rt_index));
