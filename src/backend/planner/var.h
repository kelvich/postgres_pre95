/*
 * Header file for varnodes stuff 
 * in var.c
 * $Header$
 */

extern LispValue pull_var_clause ARGS((LispValue clause));
extern bool var_equal ARGS((LispValue var1, LispValue var2, bool dots));
extern int numlevels ARGS((Var var));
extern LispValue pull_agg_clause ARGS((LispValue clause));
