/* $Header$ */

extern bool single_node ARGS((Node node));
extern bool var_is_outer ARGS((Var var));
extern bool var_is_inner ARGS((Var var));
extern bool var_is_mat ARGS((Var var));
extern bool var_is_rel ARGS((Var var));
extern Oper replace_opid ARGS((Oper oper));
extern bool constant_p ARGS((Expr node));
extern bool non_null ARGS((Expr c));
extern bool is_null ARGS((Expr c));
extern bool join_p ARGS((Node node));
extern bool scan_p ARGS((Node node));
extern bool temp_p ARGS((Node node));
extern bool plan_p ARGS((Node node));
