extern LispValue find_join_rels ARGS((LispValue outer_rels));
extern LispValue find_clause_joins ARGS((LispValue outer_rel, LispValue joininfo_list));
extern LispValue find_clauseless_joins ARGS((LispValue outer_rel, LispValue inner_rels));
extern Rel init_join_rel ARGS((LispValue outer_rel, LispValue inner_rel, LispValue joininfo));
extern LispValue new_join_tlist ARGS((LispValue tlist, LispValue other_relids, LispValue first_resdomno));
extern LispValue new_joininfo_list ARGS((LispValue current_joininfo_list, LispValue joininfo_list, LispValue join_relids));
