extern LispValue match_pathkeys_joinkeys ARGS((LispValue pathkeys, LispValue joinkeys, LispValue joinclauses, LispValue which_subkey));
extern int match_pathkey_joinkeys ARGS((LispValue pathkey, LispValue joinkeys, LispValue which_subkey));
extern Path match_paths_joinkeys ARGS((LispValue joinkeys, LispValue ordering, LispValue paths, LispValue which_subkey));
extern LispValue extract_path_keys ARGS((LispValue joinkeys, LispValue tlist, LispValue which_subkey));
extern LispValue new_join_pathkeys ARGS((LispValue outer_pathkeys, LispValue join_rel_tlist, LispValue joinclauses));
extern LispValue new_join_pathkey ARGS((LispValue subkeys, LispValue considered_subkeys, LispValue join_rel_tlist, LispValue joinclauses));
extern LispValue new_matching_subkeys ARGS((LispValue subkey, LispValue considered_subkeys, LispValue join_rel_tlist, LispValue joinclauses));
