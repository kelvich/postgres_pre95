/* $Header$ */

extern LispValue new_level_tlist ARGS((LispValue tlist, LispValue prevtlist, int prevlevel));
extern LispValue new_level_qual ARGS((LispValue quals, LispValue prevtlist, int prevlevel));
extern LispValue new_result_tlist ARGS((LispValue tlist, LispValue ltlist, LispValue rtlist, int levelnum, bool sorted));
extern LispValue new_result_qual ARGS((LispValue clauses, LispValue ltlist, LispValue rtlist, int levelnum));
extern Expr replace_clause_resultvar_refs ARGS((Expr clause, List ltlist, List rtlist, int levelnum));
extern LispValue replace_subclause_resultvar_refs ARGS((LispValue clauses, LispValue ltlist, LispValue rtlist, int levelnum));
extern Var replace_resultvar_refs ARGS((Var var, List ltlist, List rtlist, int levelnum));
extern void set_tlist_references ARGS((Plan plan));
extern void set_join_tlist_references ARGS((Join join));
extern void set_tempscan_tlist_references ARGS((SeqScan tempscan));
extern void set_temp_tlist_references ARGS((Temp temp));
extern LispValue join_references ARGS((LispValue clauses, LispValue outer_tlist, LispValue inner_tlist));
extern LispValue index_outerjoin_references ARGS((LispValue inner_indxqual, LispValue outer_tlist, Index inner_relid));
extern LispValue replace_clause_joinvar_refs ARGS((LispValue clause, LispValue outer_tlist, LispValue inner_tlist));
extern LispValue replace_subclause_joinvar_refs ARGS((LispValue clauses, LispValue outer_tlist, LispValue inner_tlist));
extern Var replace_joinvar_refs ARGS((Var var, List outer_tlist, List inner_tlist));
extern List tlist_temp_references ARGS((ObjectId tempid, List tlist));
extern void replace_result_clause ARGS((List clause, List subplanTargetList));
extern void set_result_tlist_references ARGS((Result resultNode));
extern bool OperandIsInner ARGS((LispValue opnd, int inner_relid));
