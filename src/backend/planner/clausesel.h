extern void set_clause_selectivities ARGS((LispValue clauseinfo_list, LispValue new_selectivity));
extern double product_selec ARGS((LispValue clauseinfo_list));
extern void set_rest_relselec ARGS((LispValue rel_list));
extern void set_rest_selec ARGS((LispValue clauseinfo_list));
extern double compute_clause_selec ARGS((LispValue clause, LispValue or_selectivities));
extern double compute_selec ARGS((LispValue clauses, LispValue or_selectivities));
extern ObjectId translate_relid ARGS((LispValue relid));
