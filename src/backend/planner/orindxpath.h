/* $Header$ */

extern LispValue create_or_index_paths ARGS((Rel rel, LispValue clauses));
extern LispValue best_or_subclause_indices ARGS((Rel rel, LispValue subclauses, LispValue indices, LispValue examined_indexids, Cost subcost, LispValue selectivities));
extern LispValue best_or_subclause_index ARGS((Rel rel, LispValue subclause, LispValue indices));
