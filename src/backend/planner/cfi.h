/*$Header$*/
extern LispValue intermediate_rule_lock_data ARGS((LispValue type, LispValue attribute, LispValue plan));
extern LispValue intermediate_rule_lock ARGS((LispValue rule_id, LispValue priority, LispValue type, LispValue is_early, LispValue rule_lock_intermediate_data_list));
extern LispValue print_rule_lock_intermediate ARGS((LispValue rule_lock_intermediate));
LispValue rule_insert_catalog ARGS((void));
extern List index_info ARGS((bool not_first, int relid));
extern List index_selectivity ARGS((ObjectId indid, List classes, List opnos, ObjectId relid, List attnos, List values, List flags, int32 nkeys));
extern LispValue find_inheritance_children ARGS((LispValue relation_oid));
extern LispValue find_version_parents ARGS((LispValue relation_oid));
extern int32 function_index_info ARGS((int32 function_oid, int32 index_oid));
extern int32 function_index_info ARGS((int32 function_oid, int32 index_oid));
