/*------------------------------------------------------------------
 * Identification:
 *     $Header$
 */

extern void find_all_join_paths ARGS((LispValue joinrels, LispValue previous_level_rels, int nest_level));
extern Path best_innerjoin ARGS((LispValue join_paths, LispValue outer_relid));
extern LispValue sort_inner_and_outer ARGS((Rel joinrel, Rel outerrel, Rel innerrel, LispValue mergeinfo_list));
extern LispValue match_unsorted_outer ARGS((Rel joinrel, Rel outerrel, Rel innerrel, LispValue outerpath_list, Path cheapest_inner, Path best_innerjoin, LispValue mergeinfo_list));
extern LispValue match_unsorted_inner ARGS((Rel joinrel, Rel outerrel, Rel innerrel, LispValue innerpath_list, LispValue mergeinfo_list));
extern LispValue hash_inner_and_outer ARGS((Rel joinrel, Rel outerrel, Rel innerrel, LispValue hashinfo_list));
