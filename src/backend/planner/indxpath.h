/* $Header$ */

extern LispValue find_index_paths ARGS((Rel rel, LispValue indices, LispValue clauseinfo_list, LispValue joininfo_list, LispValue sortkeys));

extern void match_index_orclauses ARGS((Rel rel, Rel index, LispValue indexkey, LispValue xclass, LispValue clauseinfo_list));

extern LispValue match_index_orclause ARGS((Rel rel, Rel index, LispValue indexkey, LispValue xclass, LispValue or_clauses, LispValue other_matching_indices));

extern LispValue group_clauses_by_indexkey ARGS((Rel rel, Rel index, LispValue indexkeys, LispValue classes, LispValue clauseinfo_list, bool join));

extern CInfo match_clause_to_indexkey ARGS((Rel rel, Rel index, LispValue indexkey, LispValue xclass, CInfo clauseInfo, bool join));

extern LispValue indexable_joinclauses ARGS((Rel rel, Rel index, LispValue joininfo_list));

extern LispValue index_innerjoin ARGS((Rel rel, LispValue clausegroup_list, Rel index));

extern LispValue create_index_paths ARGS((Rel rel, Rel index, LispValue clausegroup_list, bool join));

extern bool function_index_operand ARGS((LispValue funcOpnd, Rel rel, Rel index));

extern bool SingleAttributeIndex ARGS((Rel index));
