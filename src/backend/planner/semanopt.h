extern List SemantOpt ARGS((List root, List rangetable, List tlist, tlist qual));
extern List SemantOpt2 ARGS((List rangetable, List qual, List modqual));
extern void replace_varnodes ARGS((int left, int right, List qual));
extern void replace_tlist ARGS((int left, int right, List tlist));
extern List is_redundent_query ARGS((List qual, int is_first));
extern List add_varlist ARGS((Index leftvarno, Index rightvarno, List varlist, List qual));
extern List find_allvars ARGS((List root,List rangetable, List tlist, List qual));
extern Index ConstVarno ARGS((List rangetable, Const const));
extern List MakeTClause ARGS(());
extern List MakeFClause ARGS(());
extern List update_vars ARGS((List rangetable, List varlist, List qual));

