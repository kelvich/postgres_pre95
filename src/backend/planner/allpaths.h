/* $Header$
 */
extern LispValue find_paths ARGS((LispValue rels, int nest_level, LispValue sortkeys));
extern LispValue find_rel_paths ARGS((LispValue rels, int level, LispValue sortkeys));
extern LispValue find_join_paths ARGS((LispValue outer_rels, int levels_left, int nest_level));
