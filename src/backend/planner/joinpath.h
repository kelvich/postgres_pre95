extern void find_all_join_paths ARGS((LispValue joinrels, LispValue previous_level_rels, LispValue nest_level));
extern LispValue best_innerjoin ARGS((LispValue join_paths, LispValue outer_relid));
extern LispValue sort_inner_and_outer ARGS((LispValue joinrel, LispValue outerrel, LispValue innerrel, LispValue mergeinfo_list));
extern LispValue match_unsorted_outer ARGS((LispValue joinrel, LispValue outerrel, LispValue innerrel, LispValue outerpath_list, LispValue cheapest_inner, LispValue best_innerjoin, LispValue mergeinfo_list));
extern LispValue match_unsorted_inner ARGS((LispValue joinrel, LispValue outerrel, LispValue innerrel, LispValue innerpath_list, LispValue mergeinfo_list));
extern LispValue hash_inner_and_outer ARGS((LispValue joinrel, LispValue outerrel, LispValue innerrel, LispValue hashinfo_list));
