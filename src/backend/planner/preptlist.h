extern LispValue preprocess_targetlist ARGS((LispValue tlist, LispValue command_type, LispValue result_relation, LispValue range_table));
extern LispValue expand_targetlist ARGS((LispValue tlist, LispValue relid, LispValue command_type, LispValue result_relation));
extern LispValue replace_matching_resname ARGS((LispValue new_tlist, LispValue old_tlist));
extern LispValue new_relation_targetlist ARGS((LispValue relid, LispValue rt_index, LispValue node_type));
