/* $Header$ */
extern JInfo joininfo_member ARGS((LispValue join_relids, LispValue joininfo_list));
extern JInfo find_joininfo_node ARGS((LispValue this_rel, LispValue join_relids));
extern Var other_join_clause_var ARGS((Var var, LispValue clause));
