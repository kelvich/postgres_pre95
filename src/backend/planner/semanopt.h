/*
 * $Header$
 */

extern List is_redundent_query ARGS((List qual, int is_first));
extern List add_varlist ARGS((Index leftvarno, Index rightvarno, List varlist, List qual));
List SemantOpt ARGS((List varlist , List rangetable , List qual , List *is_redundent , int is_first ));
List SemantOpt2 ARGS((List rangetable , List qual , List modqual , List tlist ));
void replace_tlist ARGS((Index left , Index right , List tlist ));
void replace_varnodes ARGS((Index left , Index right , List qual ));
List find_allvars ARGS((List root , List rangetable , List tlist , List qual ));
List update_vars ARGS((List rangetable , List varlist , List qual ));
Index ConstVarno ARGS((List rangetable , Const constnode , char **attname ));
List MakeTClause ARGS((void ));
List MakeFClause ARGS((void ));
