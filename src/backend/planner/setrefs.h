/* $Header$ */

extern LispValue new_level_tlist ARGS((LispValue tlist, LispValue prevtlist, LispValue prevlevel));
extern LispValue new_level_qual ARGS((LispValue quals, LispValue prevtlist, LispValue prevlevel));
extern LispValue replace_clause_nestvar_refs ARGS((LispValue clause, LispValue prevtlist, LispValue prevlevel));
extern LispValue replace_subclause_nestvar_refs ARGS((LispValue clauses, LispValue prevtlist, LispValue prevlevel));
extern Var replace_nestvar_refs ARGS((Var var, List prevtlist, int prevlevel));
extern LispValue new_result_tlist ARGS((LispValue tlist, LispValue ltlist, LispValue rtlist, LispValue levelnum, LispValue sorted));
extern LispValue new_result_qual ARGS((LispValue clauses, LispValue ltlist, LispValue rtlist, LispValue levelnum));
extern Expr replace_clause_resultvar_refs ARGS((List clause, List ltlist, List rtlist, int levelnum));
extern LispValue replace_subclause_resultvar_refs ARGS((LispValue clauses, LispValue ltlist, LispValue rtlist, LispValue levelnum));
extern Var replace_resultvar_refs ARGS((Var var, List ltlist, List rtlist, int levelnum));
extern void set_tlist_references ARGS((LispValue plan));
extern void set_join_tlist_references ARGS((Join join));
extern void set_tempscan_tlist_references ARGS((LispValue tempscan));
extern void set_temp_tlist_references ARGS((LispValue temp));
extern LispValue join_references ARGS((LispValue clauses, LispValue outer_tlist, LispValue inner_tlist));
extern LispValue index_outerjoin_references ARGS((LispValue inner_indxqual, LispValue outer_tlist, LispValue inner_relid));
extern LispValue replace_clause_joinvar_refs ARGS((LispValue clause, LispValue outer_tlist, LispValue inner_tlist));
extern LispValue replace_subclause_joinvar_refs ARGS((LispValue clauses, LispValue outer_tlist, LispValue inner_tlist));
extern Var replace_joinvar_refs ARGS((Var var, List outer_tlist, List inner_tlist));
extern List tlist_temp_references ARGS((ObjectId tempid, List tlist));
