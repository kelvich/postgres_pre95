/* $Header$ */

extern LispValue create_or_index_paths ARGS((LispValue rel, LispValue clauses));
extern LispValue best_or_subclause_indices ARGS((LispValue rel, LispValue subclauses, LispValue indices, LispValue examined_indexids, double subcost, LispValue selectivities));
extern LispValue best_or_subclause_index ARGS((LispValue rel, LispValue subclause, LispValue indices));
