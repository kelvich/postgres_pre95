/*
 * Header file for varnodes stuff 
 * in var.c
 * $Header$
 */

extern LispValue pull_var_clause ARGS((LispValue clause));
extern bool var_equal ARGS((Var var1, Var var2));
extern int numlevels ARGS((Var var));
extern LispValue pull_agg_clause ARGS((LispValue clause));
/* should be somewhere else */

