/*------------------------------------------------------------------
 * Identification:
 *     $Header$
 */

extern LispValue find_join_rels ARGS((LispValue outer_rels));
extern LispValue find_clause_joins ARGS((Rel outer_rel, LispValue joininfo_list));
extern LispValue find_clauseless_joins ARGS((Rel outer_rel, LispValue inner_rels));
extern Rel init_join_rel ARGS((Rel outer_rel, Rel inner_rel, JInfo joininfo));
extern LispValue new_join_tlist ARGS((LispValue tlist, LispValue other_relids, int first_resdomno));
extern LispValue new_joininfo_list ARGS((LispValue joininfo_list, LispValue join_relids));
extern LispValue add_new_joininfos ARGS((LispValue joinrels, LispValue outerrels));
extern LispValue final_join_rels ARGS((LispValue join_rel_list));
void add_superrels ARGS((Rel rel , Rel super_rel ));
bool nonoverlap_rels ARGS((Rel rel1, Rel rel2 ));
bool nonoverlap_sets ARGS((LispValue s1 , LispValue s2 ));
void cleanup_joininfos ARGS((LispValue outer_rels ));
