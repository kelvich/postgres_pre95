extern void *initialize_targetlist ARGS((LispValue tlist));
extern void *initialize_qualification ARGS((LispValue clauses));
extern void *add_clause_to_rels ARGS((LispValue clause));
extern void *add_join_clause_info_to_rels ARGS((LispValue clauseinfo, LispValue join_relids));
extern void *add_vars_to_rels ARGS((LispValue vars, LispValue join_relids));
extern void *initialize_join_clause_info ARGS((LispValue rel_list));
extern LispValue mergesortop ARGS((LispValue clause));
extern LispValue hashjoinop ARGS((LispValue clause));
