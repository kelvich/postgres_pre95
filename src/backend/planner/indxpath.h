extern LispValue find_index_paths ARGS((LispValue rel, LispValue indices, LispValue clauseinfo_list, LispValue joininfo_list, LispValue sortkeys));
extern void *match_index_orclauses ARGS((LispValue rel, LispValue index, LispValue indexkey, LispValue xclass, LispValue clauseinfo_list));
extern LispValue match_index_orclause ARGS((LispValue rel, LispValue index, LispValue indexkey, LispValue xclass, LispValue or_clauses, LispValue other_matching_indices));
extern LispValue group_clauses_by_indexkey ARGS((LispValue rel, LispValue index, LispValue indexkeys, LispValue classes, LispValue clauseinfo_list, LispValue matched_clauseinfo_list, LispValue join));
extern LispValue match_clauses_to_indexkey ARGS((LispValue rel, LispValue index, LispValue indexkey, LispValue xclass, LispValue clauses, LispValue join));
extern LispValue indexable_joinclauses ARGS((LispValue rel, LispValue index, LispValue joininfo_list));
extern LispValue index_innerjoin ARGS((LispValue rel, LispValue clausegroup_list, LispValue index));
extern LispValue create_index_paths ARGS((LispValue rel, LispValue index, LispValue clausegroup_list, LispValue join));
