/* 
 * $Header$
 */



/* RewriteDebug.c */
char *VarOrUnionGetDesc ARGS((Var varnode , List rangetable ));
int Print_quals ARGS((List quals ));
int Print_expr ARGS((List expr ));
int Print_targetlist ARGS((List tlist ));
int Print_rangetable ARGS((List rtable ));
int Print_parse ARGS((List parsetree ));
void PrintRuleLock ARGS((Prs2OneLock rlock ));
void PrintRuleLockList ARGS((List rlist ));
