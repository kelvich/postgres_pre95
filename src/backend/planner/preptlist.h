/* $Header$ */

extern LispValue preprocess_targetlist ARGS((LispValue tlist, int command_type, LispValue result_relation, LispValue range_table));
extern LispValue expand_targetlist ARGS((LispValue tlist, ObjectId relid, int command_type, Index result_relation));
extern LispValue replace_matching_resname ARGS((LispValue new_tlist, LispValue old_tlist));
extern LispValue new_relation_targetlist ARGS((ObjectId relid, Index rt_index, NodeTag node_type));
